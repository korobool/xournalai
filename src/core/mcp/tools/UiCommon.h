/*
 * Xournal++ (xournalai)
 *
 * Shared helpers of the UI automation tools
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "api/UiAutomation.h"
#include "mcp/Args.h"
#include "mcp/Json.h"

class Control;

namespace xoj::mcp::tools {

/// Widget from an id argument, or (if allowed) the focused window
GtkWidget* resolveWidget(Control* ctrl, const Args& args, const std::string& key, bool defaultToFocused);
json widgetJson(const api::ui::WidgetInfo& w);
json windowsJson(Control* ctrl);

}  // namespace xoj::mcp::tools
