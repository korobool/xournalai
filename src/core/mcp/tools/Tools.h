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

}  // namespace xoj::mcp::tools
