/*
 * Xournal++ (xournalai)
 *
 * Shared pieces of the rendering tools (page_render, blocks_render, draft rendering)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <vector>

#include "api/RenderApi.h"
#include "mcp/Args.h"
#include "mcp/Registry.h"
#include "mcp/Schema.h"

class Control;

namespace xoj::mcp {
class McpServer;
}

namespace xoj::mcp::tools {

/// Adds dpi, max_px, background, grid, grid_step and save to a schema property list
void addRenderSchemaProperties(std::vector<schema::Property>& props);

/// Reads the options added by addRenderSchemaProperties (plus "highlight" ids, if present)
api::RenderOptions renderOptionsFromArgs(Control* ctrl, const Args& args);

/// Image content + metadata (region, scale, file path if saved)
ToolResult renderResult(McpServer& server, const api::RenderedImage& img, size_t pageIndex, bool save,
                        const std::string& stem);

}  // namespace xoj::mcp::tools
