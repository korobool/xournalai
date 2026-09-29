#include "Transactions.h"

#include <algorithm>  // for find, remove_if
#include <set>        // for set

#include "api/DocumentApi.h"       // for locateId
#include "api/Drafts.h"            // for Drafts
#include "api/DrawApi.h"           // for DrawApi
#include "api/EditApi.h"           // for EditApi
#include "api/ElementIds.h"        // for ElementIds
#include "api/EventHub.h"          // for EventHub
#include "control/Control.h"       // for Control
#include "model/Document.h"        // for Document
#include "model/Layer.h"           // for Layer
#include "tools/ToolUtil.h"        // for parseColor
#include "undo/GroupUndoAction.h"  // for GroupUndoAction
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler

#include "McpServer.h"

namespace xoj::mcp {

namespace {
/// One transaction = one undo step, named after what it did
class TransactionUndoAction: public GroupUndoAction {
public:
    explicit TransactionUndoAction(std::string text): text(std::move(text)) {}
    std::string getText() override { return text; }

private:
    std::string text;
};

bool overlaps(const std::optional<xoj::util::Rectangle<double>>& a,
              const std::optional<xoj::util::Rectangle<double>>& b) {
    if (!a || !b) {
        return true;  // a transaction without a region claims the whole page
    }
    return a->x < b->x + b->width && b->x < a->x + a->width && a->y < b->y + b->height && b->y < a->y + a->height;
}

std::vector<std::string> idsOf(const json& op) {
    std::vector<std::string> out;
    if (op.contains("ids") && op["ids"].is_array()) {
        for (const auto& v: op["ids"]) {
            if (v.is_string()) {
                out.push_back(v.get<std::string>());
            }
        }
    }
    return out;
}
}  // namespace

const Transaction& Transactions::begin(size_t page, std::optional<xoj::util::Rectangle<double>> region,
                                       std::vector<std::string> base, std::string label, int zone) {
    expire();
    Control* ctrl = server.getControl();
    const int max = std::clamp(server.getConfig().assistant.maxParallel, 1, 5);
    if (static_cast<int>(txns.size()) >= max) {
        std::string ids;
        for (const auto& [id, t]: txns) {
            ids += (ids.empty() ? "" : ", ") + id;
        }
        throw std::invalid_argument("At most " + std::to_string(max) + " transactions at once (open: " + ids +
                                    "). Commit or abort one first.");
    }
    for (const auto& [id, t]: txns) {
        if (t.page == page && overlaps(t.region, region)) {
            throw std::invalid_argument("That area is claimed by transaction " + id + " (" + t.label +
                                        "). Wait for it, or choose an area that doesn't overlap.");
        }
    }
    {
        Document* doc = ctrl->getDocument();
        std::shared_lock lock(*doc);
        if (page >= doc->getPageCount()) {
            throw std::invalid_argument("Page " + std::to_string(page + 1) + " does not exist");
        }
    }
    for (const auto& id: base) {
        api::locateId(ctrl->getDocument(), id);  // throws for unknown ids
    }
    const auto& draft = api::Drafts::get().begin(ctrl, page);
    Transaction t;
    t.id = "t" + std::to_string(++counter);
    t.page = page;
    t.region = region;
    t.base = std::move(base);
    t.draft = draft.id;
    t.draftLayer = draft.layerName;
    t.label = label.empty() ? "AI edit" : std::move(label);
    t.zone = zone;
    if (auto* hub = server.getEvents()) {
        hub->flush();
        t.cursor = hub->lastSeq();
    }
    t.beganUs = g_get_monotonic_time();
    if (beginListener) {
        t.zone = beginListener(t);
    }
    return txns[t.id] = t;
}

void Transactions::expire() {
    const gint64 now = g_get_monotonic_time();
    std::vector<std::string> old;
    for (const auto& [id, t]: txns) {
        if (!t.committing && now - t.beganUs > LEASE_US) {
            old.push_back(id);
        }
    }
    for (const auto& id: old) {
        abort(id, "its lease expired (10 minutes without a commit)");
    }
}

void Transactions::finish(const std::string& id, const std::string& state, const std::string& why) {
    auto it = txns.find(id);
    if (it == txns.end()) {
        return;
    }
    const int zone = it->second.zone;
    try {
        api::Drafts::get().close(server.getControl(), it->second.draft);
    } catch (const std::exception&) {
        // the draft is gone already (e.g. the document was replaced)
    }
    txns.erase(it);
    if (zone && zoneListener) {
        zoneListener(zone, state, why);
    }
}

void Transactions::abort(const std::string& id, const std::string& why) {
    if (!txns.count(id)) {
        return;
    }
    aborted[id] = why;
    finish(id, "failed", why);
    // queued commits of it get their answer now
    for (auto it = queue.begin(); it != queue.end();) {
        if (it->id == id) {
            auto done = std::move(it->done);
            it = queue.erase(it);
            done({{"error", "Transaction " + id + " was aborted: " + why}});
        } else {
            ++it;
        }
    }
}

void Transactions::abortAll(const std::string& why) {
    std::vector<std::string> ids;
    for (const auto& [id, t]: txns) {
        if (!t.committing) {
            ids.push_back(id);
        }
    }
    for (const auto& id: ids) {
        abort(id, why);
    }
}

std::vector<int> Transactions::openZones() const {
    std::vector<int> out;
    for (const auto& [id, t]: txns) {
        if (t.zone) {
            out.push_back(t.zone);
        }
    }
    return out;
}

std::vector<Transaction> Transactions::list() {
    expire();
    std::vector<Transaction> out;
    for (const auto& [id, t]: txns) {
        out.push_back(t);
    }
    return out;
}

void Transactions::commit(const std::string& id, json ops, std::function<void(json)> done) {
    expire();
    if (!txns.count(id)) {
        auto a = aborted.find(id);
        done({{"error", a != aborted.end() ? "Transaction " + id + " was aborted: " + a->second :
                                             "No open transaction " + id + " (see transaction_list)"}});
        return;
    }
    queue.push_back({id, std::move(ops), std::move(done)});
    pump();
}

void Transactions::pump() {
    if (playing || queue.empty()) {
        return;
    }
    Pending p = std::move(queue.front());
    queue.pop_front();
    auto it = txns.find(p.id);
    if (it == txns.end()) {
        p.done({{"error", "Transaction " + p.id + " is no longer open"}});
        pump();
        return;
    }
    playing = true;
    it->second.committing = true;
    play(it->second, std::move(p.ops), std::move(p.done));
}

std::optional<std::string> Transactions::conflict(const Transaction& t, const std::vector<std::string>& touched) {
    Control* ctrl = server.getControl();
    Document* doc = ctrl->getDocument();
    // everything it depends on must still exist
    xoj::util::Rectangle<double> area{0, 0, 0, 0};
    bool haveArea = false;
    for (const auto& id: touched) {
        try {
            auto loc = api::locateId(doc, id);
            const auto& bb = loc.element->getBoundingBox();
            if (!haveArea) {
                area = bb;
                haveArea = true;
            } else {
                area.unite(bb);
            }
        } catch (const std::exception&) {
            return id + " no longer exists";
        }
    }
    auto* hub = server.getEvents();
    if (!hub) {
        return std::nullopt;
    }
    hub->flush();
    const auto events = hub->since(t.cursor, 5000);
    if (hub->lastSeq() > t.cursor && (events.empty() || events.front().seq > t.cursor + 1)) {
        return "too much changed on the canvas since the transaction began";
    }
    const std::set<std::string> ids(touched.begin(), touched.end());
    for (const auto& e: events) {
        if (e.type == "element_changed" || e.type == "element_removed") {
            for (const auto& id: e.ids) {
                if (ids.count(id)) {
                    return id + " was " + (e.type == "element_changed" ? "changed" : "removed") + " by " +
                           (e.origin == "user" ? "the user" : "another AI edit") + " since the transaction began";
                }
            }
        }
        // the user drew over what it is about to replace (e.g. corrected the formula meanwhile)
        if (haveArea && e.type == "element_added" && e.origin == "user" && e.page == t.page) {
            const auto& r = e.area;
            const double m = 4;
            if (r.x < area.x + area.width + m && area.x - m < r.x + r.width && r.y < area.y + area.height + m &&
                area.y - m < r.y + r.height) {
                return "the user drew over it since the transaction began";
            }
        }
    }
    return std::nullopt;
}

void Transactions::play(Transaction t, json ops, std::function<void(json)> done) {
    Control* ctrl = server.getControl();
    auto fail = [this, &t, &done](const std::string& msg, bool isConflict) {
        auto it = txns.find(t.id);
        if (it != txns.end()) {
            it->second.committing = false;
            if (isConflict) {
                if (auto* hub = server.getEvents()) {
                    it->second.cursor = hub->lastSeq();  // after re-reading, the next commit is checked from here
                }
            }
        }
        playing = false;
        json out = {{"error", msg}, {"transaction", t.id}};
        if (isConflict) {
            out["conflict"] = true;
            out["hint"] = "Re-read the area (page_elements / page_render), adjust the draft or the operations, and "
                          "commit again; or transaction_abort.";
        }
        done(out);
        pump();
    };

    // Validate the operations first: nothing is applied unless all of them make sense
    if (!ops.is_array()) {
        fail("ops must be a list", false);
        return;
    }
    std::vector<std::string> touched = t.base;
    bool hasDraw = false;
    for (const auto& op: ops) {
        const std::string kind = op.is_object() ? op.value("op", "") : "";
        if (kind == "draw") {
            hasDraw = true;
        } else if (kind == "delete" || kind == "restyle" || kind == "move") {
            auto ids = idsOf(op);
            if (ids.empty()) {
                fail("'" + kind + "' needs 'ids'", false);
                return;
            }
            touched.insert(touched.end(), ids.begin(), ids.end());
        } else {
            fail("Unknown operation '" + kind + "' (draw, delete, restyle, move)", false);
            return;
        }
    }
    const auto& draft = api::Drafts::get().find(ctrl, t.draft);
    const bool draftHasContent = draft.layer && draft.layer->getElementsView().size() > 0;
    if (draftHasContent && !hasDraw) {
        ops.push_back({{"op", "draw"}});  // the draft's content is always part of the commit
    }
    if (auto why = conflict(t, touched)) {
        fail("Conflict: " + *why, true);
        return;
    }

    // Play the list in order; everything goes into one undo step
    struct Run {
        std::unique_ptr<TransactionUndoAction> group;  ///< collects every undo action of the playback
        json ops;
        size_t next = 0;
        json created = json::array();
        json skipped = json::array();
        std::function<void(json)> done;
        Transaction t;
        size_t actions = 0;
    };
    auto run = std::make_shared<Run>();
    run->group = std::make_unique<TransactionUndoAction>("AI: " + t.label);
    run->ops = std::move(ops);
    run->done = std::move(done);
    run->t = t;
    auto sink = [run](std::unique_ptr<UndoAction> a) {
        run->group->addAction(std::move(a));
        run->actions++;
    };
    auto alive = server.aliveToken();

    auto step = std::make_shared<std::function<void()>>();
    *step = [this, ctrl, run, sink, step, alive]() {
        if (!*alive) {
            return;
        }
        while (run->next < run->ops.size()) {
            const json op = run->ops[run->next++];
            const std::string kind = op.value("op", "");
            try {
                if (kind == "draw") {
                    std::vector<ElementPtr> elements;
                    {
                        auto& d = api::Drafts::get().find(ctrl, run->t.draft);
                        std::unique_lock lock(*ctrl->getDocument());
                        elements = d.layer->clearNoFree();
                    }
                    if (elements.empty()) {
                        continue;
                    }
                    api::DrawTarget target;
                    target.page = run->t.page;
                    target.layer = server.getConfig().defaultLayer;  // the user's layer setting
                    target.undoSink = sink;
                    api::AnimationOptions anim;
                    anim.enabled = op.value("animate", server.getConfig().animate);
                    anim.speed = std::clamp(op.value("speed", 1.0), 0.05, 100.0);
                    api::DrawApi(ctrl).insert(target, std::move(elements), anim,
                                              [run, step, alive](const api::DrawResult& r) {
                                                  for (const Element* e: r.elements) {
                                                      run->created.push_back(api::ElementIds::get().idOf(e));
                                                  }
                                                  if (*alive) {
                                                      (*step)();  // continue after the stylus-like playback
                                                  }
                                              });
                    return;
                }
                // Operations on existing elements: ids that vanished during the playback are skipped
                std::vector<std::string> ids;
                for (const auto& id: idsOf(op)) {
                    try {
                        api::locateId(ctrl->getDocument(), id);
                        ids.push_back(id);
                    } catch (const std::exception&) {
                        run->skipped.push_back(id);
                    }
                }
                if (ids.empty()) {
                    continue;
                }
                api::EditApi edit(ctrl, sink);
                const auto g = edit.resolve(ids);
                if (kind == "delete") {
                    edit.remove(g);
                } else if (kind == "move") {
                    edit.move(g, op.value("dx", 0.0), op.value("dy", 0.0));
                } else if (kind == "restyle") {
                    api::Restyle style;
                    if (op.contains("color")) {
                        style.color = tools::parseColor(op["color"]);
                    }
                    if (op.contains("width")) {
                        style.width = op["width"].get<double>();
                    }
                    if (op.contains("fill")) {
                        style.fill = op["fill"].get<int>();
                    }
                    edit.restyle(g, style);
                }
            } catch (const std::exception& e) {
                run->skipped.push_back(kind + ": " + e.what());
            }
        }
        // done: one undo step for the whole transaction
        const std::string text = run->group->getText();
        if (run->actions > 0) {
            ctrl->getUndoRedoHandler()->addUndoAction(std::move(run->group));
        }
        const std::string id = run->t.id;
        finish(id, "done", "");
        playing = false;
        run->done({{"committed", true},
                   {"transaction", id},
                   {"created", run->created},
                   {"skipped", run->skipped},
                   {"undo", "one undo step: " + text}});
        pump();
    };
    (*step)();
}

}  // namespace xoj::mcp
