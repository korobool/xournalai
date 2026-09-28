/*
 * Xournal++ (xournalai)
 *
 * Registration of all MCP tools, prompts and resources. Each group lives in its own file in this directory.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

namespace xoj::mcp {
class McpServer;
}

namespace xoj::mcp::tools {

void registerAll(McpServer& server);

void registerStatusTools(McpServer& server);
void registerReadTools(McpServer& server);
void registerRenderTools(McpServer& server);
void registerLayoutTools(McpServer& server);
void registerGuideTools(McpServer& server);
void registerFileTools(McpServer& server);
void registerExportTools(McpServer& server);
void registerDrawTools(McpServer& server);
void registerContentTools(McpServer& server);
void registerSvgTools(McpServer& server);
void registerPlacementTools(McpServer& server);
void registerPenTools(McpServer& server);
void registerDraftTools(McpServer& server);
void registerEditTools(McpServer& server);
void registerImportTools(McpServer& server);
void registerActionTools(McpServer& server);
void registerStructureTools(McpServer& server);
void registerControlTools(McpServer& server);
void registerUiTools(McpServer& server);

}  // namespace xoj::mcp::tools
