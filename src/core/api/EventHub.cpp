#include "EventHub.h"

#include <algorithm>     // for min, max
#include <shared_mutex>  // for shared_lock

#include "control/Control.h"  // for Control
#include "model/Document.h"   // for Document
#include "model/Element.h"    // for Element
#include "model/Layer.h"
#include "model/Stroke.h"          // for Stroke           // for Layer
#include "model/Text.h"            // for Text
#include "model/XojPage.h"         // for XojPage
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler

#include "ElementIds.h"

namespace xoj::api {

namespace {
constexpr size_t MAX_EVENTS = 5000;

/// A cheap fingerprint of what an element looks like (colour, width, fill, points, text), so that changes that
/// keep the bounding box (a recolour, a restyle) are seen too
size_t contentKey(const Element* e) {
    size_t h = std::hash<uint32_t>()(static_cast<uint32_t>(e->getColor()));
    auto mix = [&h](size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); };
    mix(static_cast<size_t>(e->getType()));
    if (const auto* s = dynamic_cast<const Stroke*>(e)) {
        mix(std::hash<double>()(s->getWidth()));
        mix(static_cast<size_t>(s->getFill() + 1));
        mix(s->getPointCount());
        if (s->getPointCount() > 0) {
            const auto& p = s->getPointVector();
            mix(std::hash<double>()(p.front().x + 3 * p.front().y + 7 * p.back().x + 11 * p.back().y +
                                    13 * p[p.size() / 2].z));
        }
    } else if (const auto* t = dynamic_cast<const Text*>(e)) {
        mix(std::hash<std::string>()(t->getText()));
    }
    return h;
}

xoj::util::Rectangle<double> unite(const xoj::util::Rectangle<double>& a, const xoj::util::Rectangle<double>& b) {
    if (a.width <= 0 && a.height <= 0) {
        return b;
    }
    const double x1 = std::min(a.x, b.x), y1 = std::min(a.y, b.y);
    const double x2 = std::max(a.x + a.width, b.x + b.width), y2 = std::max(a.y + a.height, b.y + b.height);
    return {x1, y1, x2 - x1, y2 - y1};
}
}  // namespace

EventHub::EventHub(Control* control): control(control) {
    control->getUndoRedoHandler()->addUndoRedoListener(this);
    registerListener(control);
    snapshotAll();
}

EventHub::~EventHub() {
    if (idleSource) {
        g_source_remove(idleSource);
    }
    control->getUndoRedoHandler()->removeUndoRedoListener(this);
    unregisterListener();
}

EventHub::Snapshot EventHub::take(const PageRef& page) const {
    Snapshot s;
    for (const Layer* l: page->getLayersView()) {
        for (const Element* e: l->getElementsView()) {
            s.boxes.emplace(e, e->getBoundingBox());
            s.keys.emplace(e, contentKey(e));
        }
    }
    return s;
}

void EventHub::snapshotAll() {
    pages.clear();
    Document* doc = control->getDocument();
    std::shared_lock lock(*doc);
    for (size_t i = 0; i < doc->getPageCount(); i++) {
        PageRef p = doc->getPage(i);
        pages[p.get()] = take(p);
    }
}

void EventHub::push(DocEvent e) {
    e.seq = ++seq;
    e.timeUs = g_get_monotonic_time();
    if (e.origin.empty()) {
        e.origin = agentDepth > 0 ? "agent" : "user";
    }
    if (e.origin == "user") {
        lastUserUs = e.timeUs;
    }
    log.push_back(e);
    while (log.size() > MAX_EVENTS) {
        log.pop_front();
    }
    if (listener) {
        listener(log.back());
    }
    for (const auto& l: extraListeners) {
        l(log.back());
    }
}

void EventHub::scheduleDiff(const PageRef& page) {
    const std::string origin = agentDepth > 0 ? "agent" : "user";
    for (auto& [p, o]: pending) {
        if (p == page) {
            if (origin == "agent") {
                o = origin;
            }
            return;
        }
    }
    pending.emplace_back(page, origin);
    if (!idleSource) {
        idleSource = g_idle_add(&EventHub::runPending, this);
    }
}

gboolean EventHub::runPending(gpointer data) {
    auto* self = static_cast<EventHub*>(data);
    self->idleSource = 0;
    self->flush();
    return G_SOURCE_REMOVE;
}

void EventHub::flush() {
    if (idleSource) {
        g_source_remove(idleSource);
        idleSource = 0;
    }
    auto work = std::move(pending);
    pending.clear();
    for (const auto& [page, origin]: work) {
        diff(page, origin);
    }
}

void EventHub::diff(const PageRef& page, const std::string& origin) {
    Document* doc = control->getDocument();
    size_t index = npos;
    Snapshot now;
    {
        std::shared_lock lock(*doc);
        index = doc->indexOf(page);
        if (index == npos) {
            return;
        }
        now = take(page);
    }
    Snapshot& before = pages[page.get()];
    const std::string description = control->getUndoRedoHandler()->undoDescription();
    auto& ids = ElementIds::get();

    DocEvent added{0, 0, "element_added", origin, index, {}, {}, description};
    DocEvent removed{0, 0, "element_removed", origin, index, {}, {}, description};
    DocEvent changed{0, 0, "element_changed", origin, index, {}, {}, description};
    bool agentAdded = false;
    for (const auto& [e, box]: now.boxes) {
        auto it = before.boxes.find(e);
        if (it == before.boxes.end()) {
            added.ids.push_back(ids.idOf(e));
            added.area = unite(added.area, box);
            agentAdded = agentAdded || ids.origin(e).has_value();
        } else if (!(it->second == box) || before.keys[e] != now.keys[e]) {
            changed.ids.push_back(ids.idOf(e));
            changed.area = unite(unite(changed.area, it->second), box);
        }
    }
    for (const auto& [e, box]: before.boxes) {
        if (!now.boxes.count(e)) {
            // The element may be destroyed already: only report ids we handed out
            if (auto id = ids.existingId(e)) {
                removed.ids.push_back(*id);
            }
            removed.area = unite(removed.area, box);
        }
    }
    before = std::move(now);
    if (!added.ids.empty()) {
        if (agentAdded) {
            added.origin = "agent";
        }
        push(added);
    }
    if (!removed.ids.empty() || removed.area.width > 0 || removed.area.height > 0) {
        push(removed);
    }
    if (!changed.ids.empty()) {
        push(changed);
    }
}

void EventHub::undoRedoPageChanged(PageRef page) {
    if (page) {
        scheduleDiff(page);
    }
}

void EventHub::pageChanged(size_t page) {
    // Some operations (e.g. background changes) only notify the page; keep the snapshot fresh
    Document* doc = control->getDocument();
    PageRef p;
    {
        std::shared_lock lock(*doc);
        if (page >= doc->getPageCount()) {
            return;
        }
        p = doc->getPage(page);
    }
    scheduleDiff(p);
}

void EventHub::pageInserted(size_t page) {
    snapshotAll();
    push({0, 0, "page_inserted", "", page, {}, {}, {}});
}

void EventHub::pageDeleted(size_t page) {
    snapshotAll();
    push({0, 0, "page_deleted", "", page, {}, {}, {}});
}

void EventHub::documentChanged(DocumentChangeType type) {
    snapshotAll();
    if (type == DOCUMENT_CHANGE_COMPLETE || type == DOCUMENT_CHANGE_CLEARED) {
        push({0, 0, "document_replaced", "", 0, {}, {}, {}});
    }
}

std::vector<DocEvent> EventHub::since(uint64_t cursor, size_t max) const {
    std::vector<DocEvent> out;
    for (const auto& e: log) {
        if (e.seq > cursor) {
            out.push_back(e);
            if (out.size() >= max) {
                break;
            }
        }
    }
    return out;
}

}  // namespace xoj::api
