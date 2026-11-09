// Tool: workers (worker sessions in tabs of the AI terminal, started by the serving session)

#include "config-features.h"  // for ENABLE_AI_TERMINAL
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"  // for McpUi
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerWorkerTools(McpServer& server) {
#ifdef ENABLE_AI_TERMINAL
    McpServer* srv = &server;
    ToolSpec t;
    t.name = "workers";
    t.title = "Worker sessions (tabs of the AI terminal)";
    t.description =
            "For the serving session (the coordinator): start a worker, a Claude Code session in its own tab of the AI "
            "terminal, for one heavy job (an illustration, a picture or scheme from the web, a mind map, an analysis). "
            "op=start (name, e.g. \"T3\"; brief: the whole task, self-contained, with the page and area to draw in and "
            "what to report; effort: low, medium, high (default), xhigh or max). The worker reads the brief as its "
            "first message, runs in <companion>/workers with its own CLAUDE.md, and may only use the xournalai tools, "
            "web search and fetch, Read and python3 (anything else is refused without asking). Up to "
            "assistant.max_parallel workers at once (with your subagents). op=list: the workers and whether they "
            "run. op=close (name, or all=true): ends a worker and closes its tab. Close them when their job is done "
            "(and all of them when live notes end). The owner sees every worker's tab.";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("Operation", {"start", "list", "close"})},
             {"name", schema::string("start/close: the worker's name (1-24 letters, digits, . _ -)")},
             {"brief", schema::string("start: the task, self-contained")},
             {"effort", schema::enumeration("start: effort level", {"low", "medium", "high", "xhigh", "max"})},
             {"all", schema::boolean("close: every worker")}},
            {"op"});
    t.tier = Tier::Ui;
    t.handler = [srv](const json& j) {
        Args args(j);
        args.rejectUnknown({"op", "name", "brief", "effort", "all"});
        const std::string op = args.choice("op", {"start", "list", "close"}, "");
        McpUi* ui = srv->getUi();
        if (!ui || !ui->terminal()) {
            throw ToolError("no AI terminal in this window");
        }
        if (op == "start") {
            std::string error;
            json r = ui->startWorker(args.str("name"), args.str("brief"), args.str("effort", "high"), error);
            if (!error.empty()) {
                throw ToolError(error);
            }
            return ToolResult::structured(std::move(r));
        }
        if (op == "close") {
            json closed = json::array();
            if (args.boolean("all", false)) {
                for (const auto& w: ui->workers()) {
                    if (ui->closeWorker(w["name"].get<std::string>())) {
                        closed.push_back(w["name"]);
                    }
                }
            } else if (ui->closeWorker(args.str("name"))) {
                closed.push_back(args.str("name"));
            } else {
                throw ToolError("no worker named \"" + args.str("name", "") + "\"");
            }
            return ToolResult::structured({{"closed", closed}, {"workers", ui->workers()}});
        }
        return ToolResult::structured({{"workers", ui->workers()}});
    };
    server.getRegistry().addTool(std::move(t));
#else
    (void)server;
#endif
}

}  // namespace xoj::mcp::tools
