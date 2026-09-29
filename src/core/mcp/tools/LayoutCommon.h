/*
 * xournalai (based on Xournal++)
 *
 * Layout analysis of a page, shared by layout_analyze and blocks_render
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "api/DocumentApi.h"
#include "api/Geometry.h"
#include "api/Layout.h"

class Control;

namespace xoj::mcp::tools {

struct PageLayout {
    std::vector<api::ElementLocation> elements;  ///< visible elements; LayoutItem::index points in here
    api::Layout layout;
};

/// Analyzes the visible content of a page (caller holds the document lock)
PageLayout analyzePage(Control* ctrl, size_t pageIndex, std::optional<size_t> layer,
                       std::optional<xoj::util::Rectangle<double>> region);

/// "b1", "b2", ... (1-based)
std::string blockId(size_t index);
/// Parses "b3" into 2; throws ToolError if malformed or out of range
size_t parseBlockId(const std::string& id, size_t count);

}  // namespace xoj::mcp::tools
