/*
 * Xournal++ (xournalai)
 *
 * Shared helpers for MCP tool implementations
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>     // for size_t
#include <functional>  // for function
#include <optional>    // for optional
#include <string>      // for string

#include "mcp/Args.h"
#include "mcp/Json.h"
#include "mcp/Registry.h"
#include "model/PageRef.h"   // for PageRef
#include "util/Color.h"      // for Color
#include "util/Rectangle.h"  // for Rectangle

class Control;
class Layer;

namespace xoj::mcp::tools {

/// "#rrggbb" (or "#rrggbbaa" when not opaque)
std::string colorToHex(Color c);

/// Parses "#rgb", "#rrggbb", "#rrggbbaa", "rgb(r,g,b)", a CSS color name ("red", "teal", ...) or an integer 0xRRGGBB
Color parseColor(const json& value, const std::string& what = "color");

/**
 * @brief Resolves a 1-based page argument (or "current"/absent) to a 0-based page index.
 * Throws ToolError if the page does not exist.
 */
size_t resolvePageIndex(Control* ctrl, const Args& args, const std::string& key = "page");

/// Name shown for a layer in the UI ("Layer 3" if unnamed). `index` is 1-based.
std::string layerDisplayName(const Layer* layer, size_t index);

/// Compact page summary: size, background, layers, element counts
json pageSummary(const PageRef& page, size_t index);

/// Reads an optional [x, y, width, height] argument (page points)
std::optional<xoj::util::Rectangle<double>> parseRegion(const Args& args, const std::string& key = "region");

/**
 * @brief Calls `then(true)` once `condition` holds, polling on the main loop every `intervalMs`, or `then(false)`
 * after `timeoutMs`. For state that the application updates asynchronously (scrolling, layout, dialogs).
 */
void whenReady(std::function<bool()> condition, std::function<void(bool)> then, unsigned timeoutMs = 2000,
               unsigned intervalMs = 40);

/// Throws ToolError unless a main window and document exist
void requireDocument(Control* ctrl);

}  // namespace xoj::mcp::tools
