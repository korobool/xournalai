// Tool: thinking

#include "assistant/ThinkingOverlay.h"  // for ThinkingOverlay
#include "control/Control.h"            // for Control
#include "mcp/McpProtocol.h"  // for callingWorker
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
            "your own (e.g. Auto-improve), and keep the status short (\"typesetting formula…\"), in the app's interface "
            "language (English, as the rest of its interface). A worker session's "
            "zone (one in a tab of the AI terminal) is its own: it is labelled with the worker's name. A zone you open "
            "lasts until you end it (done/fail), whatever turns end meanwhile (or the owner stops it, or after 10 "
            "minutes; a worker's when it is closed): always end it. "
            "color (e.g. the topic's \"#8b3a62\"): the zone's outline and veil in that colour. op=say (text; page and "
            "region optional): a short answer to the owner as a tip by that region (or at the top of the canvas) for a "
            "few seconds, e.g. in voice control's dialogue.";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("start | update | done | fail | say", {"start", "update", "done", "fail", "say"})},
             {"id", schema::integer("Zone id (from start) for update/done/fail")},
             {"page", schema::integer("start: page (1-based); default: current page")},
             {"region", schema::array("start: area [x, y, width, height] in page points (default: the page)",
                                      schema::number("coordinate"))},
             {"text", schema::string("A short status, e.g. \"typesetting formula…\"")},
             {"color", schema::string("start: the zone's colour, e.g. \"#8b3a62\" (default: teal)")}},
            {"op"});
    t.readOnly = true;  // shows state only; never changes the document
    t.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "id", "page", "region", "text", "color"});
        McpUi* ui = srv->getUi();
        assistant::ThinkingOverlay* ov = ui ? ui->thinking() : nullptr;
        if (!ov) {
            throw ToolError("This window can't show thinking zones");
        }
        const std::string op = args.choice("op", {"start", "update", "done", "fail", "say"}, "");
        const std::string text = args.str("text", "");
        using S = assistant::ThinkingOverlay::State;
        // a worker's zone says whose it is
        auto labelled = [](const std::string& owner, const std::string& t) {
            return owner.empty() || t.empty() || t.rfind("⚙ " + owner + " · ", 0) == 0 ? t : "⚙ " + owner + " · " + t;
        };
        if (op == "say") {
            // a short answer to the owner (voice control's dialogue): a tip by the region, or at the top
            if (text.empty()) {
                throw ToolError("say: the text to show");
            }
            std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> where;
            if (auto region = parseRegion(args)) {
                where = std::make_pair(resolvePageIndex(ctrl, args), *region);
            }
            ov->flashReply(labelled(callingWorker(), text), where);
            return ToolResult::structured({{"shown", true}});
        }
        if (op == "start") {
            const size_t page = resolvePageIndex(ctrl, args);
            auto region = parseRegion(args);
            std::optional<uint32_t> rgb;
            if (args.has("color")) {
                rgb = static_cast<uint32_t>(parseColor(args.raw("color"))) & 0xffffffU;
            }
            const std::string& owner = callingWorker();
            const int id = ov->add(page, region ? *region : ui->pageArea(page),
                                   labelled(owner, text.empty() ? "working…" : text), S::Thinking, owner, rgb);
            ov->setKept(id);  // (until the agent ends it: its turn may end long before, e.g. in voice control)
            return ToolResult::structured({{"id", id}});
        }
        const auto id = static_cast<int>(args.integer("id", 0, 1, 1 << 30));
        ov->set(id, op == "update" ? S::Thinking : (op == "done" ? S::Done : S::Failed), labelled(ov->ownerOf(id), text));
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
                json zone = {{"id", z.id},
                             {"state", assistant::ThinkingOverlay::name(z.state)},
                             {"page", z.page + 1},
                             {"text", z.text}};
                if (!z.owner.empty()) {
                    zone["worker"] = z.owner;
                }
                if (!z.detail.empty()) {
                    zone["detail"] = z.detail;  // (what the app sees the worker doing)
                }
                zones.push_back(std::move(zone));
            }
        }
        return ToolResult::structured({{"zones", zones}});
    };
    server.getRegistry().addTool(std::move(list));
}

}  // namespace xoj::mcp::tools
