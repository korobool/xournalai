// Tool: thinking

#include "assistant/ThinkingOverlay.h"  // for ThinkingOverlay
#include "control/Control.h"            // for Control
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerThinkingTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec t;
    t.name = "thinking";
    t.title = "Show where you are working";
    t.description =
            "Shows the user where you are working, so the app is never silent: op=start marks a region of a page "
            "with a grey veil, an animated outline, a spinner and your one-line status (returns an id); op=update "
            "changes the status; op=done (a green tick that fades) or op=fail (a red mark) ends it. Zones for "
            "toolbar actions and markers are created and closed by the app itself; use this for work you start on "
            "your own (e.g. Auto-improve), and keep the status short (\"typesetting formula…\").";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("start | update | done | fail", {"start", "update", "done", "fail"})},
             {"id", schema::integer("Zone id (from start) for update/done/fail")},
             {"page", schema::integer("start: page (1-based); default: current page")},
             {"region", schema::array("start: area [x, y, width, height] in page points (default: the page)",
                                      schema::number("coordinate"))},
             {"text", schema::string("A short status, e.g. \"typesetting formula…\"")}},
            {"op"});
    t.readOnly = true;  // shows state only; never changes the document
    t.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "id", "page", "region", "text"});
        McpUi* ui = srv->getUi();
        assistant::ThinkingOverlay* ov = ui ? ui->thinking() : nullptr;
        if (!ov) {
            throw ToolError("This window can't show thinking zones");
        }
        const std::string op = args.choice("op", {"start", "update", "done", "fail"}, "");
        const std::string text = args.str("text", "");
        using S = assistant::ThinkingOverlay::State;
        if (op == "start") {
            const size_t page = resolvePageIndex(ctrl, args);
            auto region = parseRegion(args);
            const int id =
                    ov->add(page, region ? *region : ui->pageArea(page), text.empty() ? "working…" : text, S::Thinking);
            return ToolResult::structured({{"id", id}});
        }
        const auto id = static_cast<int>(args.integer("id", 0, 1, 1 << 30));
        ov->set(id, op == "update" ? S::Thinking : (op == "done" ? S::Done : S::Failed), text);
        json zones = json::array();
        for (const auto& z: ov->zones()) {
            zones.push_back({{"id", z.id}, {"state", assistant::ThinkingOverlay::name(z.state)}, {"text", z.text}});
        }
        return ToolResult::structured({{"id", id}, {"zones", zones}});
    };
    server.getRegistry().addTool(std::move(t));

    ToolSpec list;
    list.name = "thinking_list";
    list.title = "Zones you are shown working in";
    list.description = "Lists the thinking zones on the canvas (id, state, page, status).";
    list.inputSchema = schema::object({});
    list.readOnly = true;
    list.idempotent = true;
    list.handler = [srv](const json&) {
        McpUi* ui = srv->getUi();
        json zones = json::array();
        if (ui && ui->thinking()) {
            for (const auto& z: ui->thinking()->zones()) {
                zones.push_back({{"id", z.id},
                                 {"state", assistant::ThinkingOverlay::name(z.state)},
                                 {"page", z.page + 1},
                                 {"text", z.text}});
            }
        }
        return ToolResult::structured({{"zones", zones}});
    };
    server.getRegistry().addTool(std::move(list));
}

}  // namespace xoj::mcp::tools
