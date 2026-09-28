// Tool: changes_get

#include "api/EventHub.h"     // for EventHub
#include "api/PenEngine.h"    // for userInputActive
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
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
}

}  // namespace xoj::mcp::tools
