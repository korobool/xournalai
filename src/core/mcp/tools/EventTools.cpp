// Tool: changes_get

#include "api/DocumentApi.h"  // for currentPageIndex
#include "api/EventHub.h"     // for EventHub
#include "api/PenEngine.h"    // for userInputActive
#include "api/RenderApi.h"    // for renderPage
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/OffUi.h"  // for runOffUi
#include "mcp/Schema.h"

#include "EventCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

json eventJson(const api::DocEvent& e) {
    json j = {{"seq", e.seq}, {"type", e.type}, {"origin", e.origin}, {"page", e.page + 1}};
    if (!e.ids.empty()) {
        j["ids"] = e.ids;
    }
    if (e.area.width > 0 || e.area.height > 0) {
        j["area"] = bboxJson(e.area.x, e.area.y, e.area.width, e.area.height);
    }
    if (!e.description.empty()) {
        j["step"] = e.description;
    }
    return j;
}

void registerEventTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec changes;
    changes.name = "changes_get";
    changes.title = "Document changes";
    changes.description =
            "What changed in the document since 'since' (a cursor from a previous call; 0 = everything kept): "
            "elements added / removed / changed (with ids, page and affected area), pages inserted / deleted, "
            "document replaced - each marked origin user or agent. Returns the new cursor and whether the user is "
            "drawing right now. To wait for the user instead of polling, use wait_for_user.";
    changes.inputSchema =
            schema::object({{"since", schema::withDefault(schema::integer("Cursor from the previous call"), 0)},
                            {"max", schema::withDefault(schema::integer("Maximum events"), 200)},
                            {"origin", schema::enumeration("Only events from", {"user", "agent"})}});
    changes.readOnly = true;
    changes.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"since", "max", "origin"});
        auto* hub = srv->getEvents();
        if (!hub) {
            throw ToolError("Change tracking is not running");
        }
        hub->flush();
        const auto since = static_cast<uint64_t>(args.integer("since", 0, 0, INT64_MAX));
        const std::string origin = args.str("origin", "");
        json list = json::array();
        for (const auto& e: hub->since(since, static_cast<size_t>(args.integer("max", 200, 1, 5000)))) {
            if (origin.empty() || e.origin == origin) {
                list.push_back(eventJson(e));
            }
        }
        return ToolResult::structured({{"cursor", hub->lastSeq()},
                                       {"events", std::move(list)},
                                       {"user_drawing", api::userInputActive(ctrl)}});
    };
    server.getRegistry().addTool(std::move(changes));

    ToolSpec wait;
    wait.name = "wait_for_user";
    wait.title = "Wait for the user";
    wait.description =
            "Waits until the user has changed something (drawn, erased, moved, typed ...) and then paused for "
            "idle_ms, and returns what changed, the affected area, an image of it and the user's current tool and "
            "page - so you can react to what they are drawing. Loop: wait_for_user → respond (in chat or by "
            "drawing) → wait_for_user(since=<cursor>). Returns timed_out=true if nothing happened. Keep "
            "timeout_s below your client's tool-call timeout.";
    wait.inputSchema = schema::object(
            {{"since", schema::integer("Only changes after this cursor (default: from now)")},
             {"idle_ms", schema::withDefault(schema::integer("Pause after the last change before returning"), 1500)},
             {"timeout_s", schema::withDefault(schema::integer("Give up after this many seconds"), 120)},
             {"page", schema::integer("Only changes on this page (1-based)")},
             {"region", schema::array("Only changes intersecting [x, y, width, height]", schema::number("coordinate"))},
             {"render", schema::withDefault(schema::boolean("Include an image of the changed area"), true)}});
    wait.readOnly = true;
    wait.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"since", "idle_ms", "timeout_s", "page", "region", "render"});
        auto* hub = srv->getEvents();
        if (!hub) {
            throw ToolError("Change tracking is not running");
        }
        hub->flush();
        struct State {
            uint64_t cursor;
            gint64 idleUs, deadlineUs;
            std::optional<size_t> page;
            std::optional<xoj::util::Rectangle<double>> region;
            bool render;
            Responder respond;
        };
        auto* st = new State{
                args.has("since") ? static_cast<uint64_t>(args.integer("since", 0, 0, INT64_MAX)) : hub->lastSeq(),
                args.integer("idle_ms", 1500, 100, 60000) * 1000,
                g_get_monotonic_time() + args.integer("timeout_s", 120, 1, 3600) * 1000000,
                std::nullopt,
                parseRegion(args),
                args.boolean("render", true),
                respond};
        if (args.has("page")) {
            st->page = static_cast<size_t>(args.integer("page", 1, 1, 100000) - 1);
        }
        struct Ctx {
            Control* ctrl;
            McpServer* srv;
            State* st;
            std::shared_ptr<bool> alive;
        };
        g_timeout_add(
                100,
                +[](gpointer data) -> gboolean {
                    auto* c = static_cast<Ctx*>(data);
                    if (!*c->alive) {  // the application is shutting down
                        delete c->st;
                        delete c;
                        return G_SOURCE_REMOVE;
                    }
                    auto* hub = c->srv->getEvents();
                    State& s = *c->st;
                    const gint64 now = g_get_monotonic_time();
                    if (!hub) {  // server stopped
                        s.respond(ToolResult::error("The MCP server stopped"));
                        delete c->st;
                        delete c;
                        return G_SOURCE_REMOVE;
                    }
                    hub->flush();
                    std::vector<api::DocEvent> events;
                    for (const auto& e: hub->since(s.cursor, 1000)) {
                        bool ok = e.origin == "user" && (!s.page || e.page == *s.page);
                        if (ok && s.region && (e.area.width > 0 || e.area.height > 0)) {
                            const auto& r = *s.region;
                            ok = e.area.x <= r.x + r.width && r.x <= e.area.x + e.area.width &&
                                 e.area.y <= r.y + r.height && r.y <= e.area.y + e.area.height;
                        }
                        if (ok) {
                            events.push_back(e);
                        }
                    }
                    const bool settled = !events.empty() && !api::userInputActive(c->ctrl) &&
                                         now - hub->lastUserChangeUs() >= s.idleUs;
                    if (!settled && now < s.deadlineUs) {
                        return G_SOURCE_CONTINUE;
                    }
                    json out = {{"cursor", hub->lastSeq()}, {"timed_out", events.empty()}};
                    json list = json::array();
                    std::optional<xoj::util::Rectangle<double>> area;
                    size_t page = events.empty() ? api::currentPageIndex(c->ctrl) : events.back().page;
                    for (const auto& e: events) {
                        list.push_back(eventJson(e));
                        if (e.page == page && (e.area.width > 0 || e.area.height > 0)) {
                            if (!area) {
                                area = e.area;
                            } else {
                                const double x1 = std::min(area->x, e.area.x), y1 = std::min(area->y, e.area.y);
                                const double x2 = std::max(area->x + area->width, e.area.x + e.area.width);
                                const double y2 = std::max(area->y + area->height, e.area.y + e.area.height);
                                area = xoj::util::Rectangle<double>(x1, y1, x2 - x1, y2 - y1);
                            }
                        }
                    }
                    out["events"] = std::move(list);
                    out["page"] = page + 1;
                    out["current_page"] = api::currentPageIndex(c->ctrl) + 1;
                    if (area) {
                        out["area"] = bboxJson(area->x, area->y, area->width, area->height);
                    }
                    if (area && s.render) {
                        // The picture of what changed is rendered off the UI thread: the user may be writing again
                        api::RenderOptions o;
                        o.page = page;
                        o.region = xoj::util::Rectangle<double>(area->x - 30, area->y - 30, area->width + 60,
                                                                area->height + 60);
                        o.dpi = 150;
                        o.maxPixels = 1200;
                        runOffUi(
                                c->ctrl,
                                [doc = c->ctrl->getDocument(), o, out]() mutable {
                                    try {
                                        const auto img = api::renderPage(doc, o);
                                        out["image_region"] = bboxJson(img.region.x, img.region.y, img.region.width,
                                                                       img.region.height);
                                        ToolResult result = ToolResult::structured(out);
                                        result.addImage(base64Encode(img.png), "image/png");
                                        return result;
                                    } catch (const std::exception&) {
                                        return ToolResult::structured(out);
                                    }
                                },
                                s.respond);
                    } else {
                        s.respond(ToolResult::structured(out));
                    }
                    delete c->st;
                    delete c;
                    return G_SOURCE_REMOVE;
                },
                new Ctx{ctrl, srv, st, srv->aliveToken()});
    };
    server.getRegistry().addTool(std::move(wait));
}

}  // namespace xoj::mcp::tools
