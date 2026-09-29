// Tools: app_status, doc_info

#include <algorithm>     // for max, min
#include <shared_mutex>  // for shared_lock

#include "api/AgentGate.h"
#include "api/DocumentApi.h"      // for currentPageIndex
#include "control/Control.h"      // for Control
#include "control/ToolEnums.h"    // for toolTypeToString
#include "control/ToolHandler.h"  // for ToolHandler
#include "mcp/McpConfig.h"
#include "mcp/McpHttpServer.h"
#include "mcp/McpServer.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Document.h"        // for Document
#include "model/XojPage.h"         // for XojPage
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler

#include "ToolUtil.h"
#include "Tools.h"
#include "config.h"  // for PROJECT_VERSION, XOURNALAI_VERSION

namespace xoj::mcp::tools {

namespace {

json documentSummary(Control* ctrl) {
    Document* doc = ctrl->getDocument();
    std::shared_lock lock(*doc);
    const auto path = doc->getFilepath();
    const auto pdf = doc->getPdfFilepath();
    return {{"path", path.empty() ? json(nullptr) : json(toUtf8(path))},
            {"pdf_background", pdf.empty() ? json(nullptr) : json(toUtf8(pdf))},
            {"modified", ctrl->getUndoRedoHandler()->isChanged()},
            {"page_count", doc->getPageCount()},
            {"current_page", api::currentPageIndex(ctrl) + 1}};
}

json toolSummary(Control* ctrl) {
    ToolHandler* th = ctrl->getToolHandler();
    return {{"tool", std::string(toolTypeToString(th->getToolType()))},
            {"color", colorToHex(th->getColor())},
            {"thickness", th->getThickness()},
            {"size", std::string(toolSizeToString(th->getSize()))}};
}

}  // namespace

void registerStatusTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec status;
    status.name = "app_status";
    status.title = "Application status";
    status.description =
            "Returns xournalai's version, MCP connection info, granted permissions, a summary of the open document "
            "(path, modified flag, page count, current page) and the user's active tool. Call this first to "
            "orient yourself.";
    status.inputSchema = schema::object({});
    status.readOnly = true;
    status.idempotent = true;
    status.handler = [ctrl, srv](const json&) {
        requireDocument(ctrl);
        const McpConfig& cfg = srv->getConfig();
        json permissions = json::object();
        for (Tier t: {Tier::Read, Tier::Draw, Tier::Ui, Tier::Files, Tier::Destructive}) {
            permissions[tierName(t)] = cfg.allows(t);
        }
        json out = {{"app", "xournalai"},
                    {"version", XOURNALAI_VERSION},
                    {"based_on", std::string("Xournal++ ") + XOURNALPP_BASE_VERSION},
                    {"mcp",
                     {{"url", cfg.url()},
                      {"sessions", srv->getHttpServer() ? srv->getHttpServer()->sessionCount() : 0},
                      {"permissions", permissions},
                      {"paused_by_user", api::AgentGate::paused()},
                      {"paused_since", api::AgentGate::paused() ? json(api::AgentGate::pausedSince()) : json(nullptr)},
                      {"default_layer", cfg.defaultLayer},
                      {"export_dir", toUtf8(cfg.exportDir)}}},
                    {"document", documentSummary(ctrl)},
                    {"user_tool", toolSummary(ctrl)},
                    {"coordinates", "page points (1/72 inch), origin top-left of each page, pages numbered from 1"}};
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(status));

    ToolSpec info;
    info.name = "doc_info";
    info.title = "Document structure";
    info.description =
            "Describes the open document: file path, background PDF, modified flag, and per page its size in "
            "points, background (type, color, PDF page), layers (index from 1, name, visibility, element count) "
            "and the selected layer. Use 'from'/'to' to page through long documents.";
    info.inputSchema =
            schema::object({{"from", schema::withDefault(schema::integer("First page to describe (1-based)"), 1)},
                            {"to", schema::integer("Last page to describe (inclusive); default: from+49")}});
    info.readOnly = true;
    info.idempotent = true;
    info.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"from", "to"});
        Document* doc = ctrl->getDocument();
        json out = documentSummary(ctrl);
        std::shared_lock lock(*doc);
        const auto count = static_cast<int64_t>(doc->getPageCount());
        const int64_t from = args.integer("from", 1, 1, std::max<int64_t>(count, 1));
        const int64_t to = std::min(args.integer("to", from + 49), count);
        json pages = json::array();
        for (int64_t p = from; p <= to; p++) {
            pages.push_back(pageSummary(doc->getPage(static_cast<size_t>(p - 1)), static_cast<size_t>(p - 1)));
        }
        out["pages"] = std::move(pages);
        if (to < count) {
            out["more_pages"] = "Pages " + std::to_string(to + 1) + ".." + std::to_string(count) +
                                " omitted; call again with from=" + std::to_string(to + 1);
        }
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(info));
}

}  // namespace xoj::mcp::tools
