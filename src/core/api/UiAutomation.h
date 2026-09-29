/*
 * xournalai (based on Xournal++)
 *
 * UI automation: inspecting and operating the application's GTK interface like a user would
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include <gtk/gtk.h>

namespace xoj::api::ui {

/// Stable ids for widgets ("w12"); an id is retired when its widget is destroyed
std::string idOf(GtkWidget* w);
/// The live widget for an id, or nullptr
GtkWidget* lookup(const std::string& id);

struct WindowInfo {
    GtkWidget* window;
    std::string title;
    std::string kind;  ///< main | dialog | file_chooser | menu | popup | window
    bool modal = false;
    bool focused = false;
};

/// Visible toplevel windows, the focused one first
std::vector<WindowInfo> windows(GtkWidget* mainWindow);

struct WidgetInfo {
    GtkWidget* widget;
    std::string type;   ///< GType name, e.g. "GtkButton"
    std::string role;   ///< button, toggle, check, radio, entry, spin, scale, combo, label, menu_item, list, ...
    std::string name;   ///< GtkBuildable id from the .glade file, if any
    std::string label;  ///< visible text of the widget (label, button text, entry placeholder, ...)
    std::string tooltip;
    std::optional<std::string> value;  ///< current value as text (entry text, number, "true"/"false", ...)
    bool sensitive = true;
    bool visible = true;
    int x = 0, y = 0, width = 0, height = 0;  ///< allocation relative to its toplevel window
    int depth = 0;
    bool interactive = false;  ///< can be clicked or edited
};

/**
 * @brief Flattened widget tree of `root` in display order.
 * @param maxDepth limit the depth (-1 = unlimited)
 * @param interestingOnly skip invisible widgets and plain layout containers without text
 */
std::vector<WidgetInfo> inspect(GtkWidget* root, int maxDepth, bool interestingOnly);

/// Renders a widget (or window) to PNG
std::string screenshot(GtkWidget* widget);

/// Text shown by a widget (label text, button label, ...)
std::string widgetText(GtkWidget* w);

/// First descendant of `root` (depth-first, including internal children) of the given GType, or nullptr
GtkWidget* findDescendant(GtkWidget* root, GType type);

/// Menu item of a menu shell whose label equals `label` (mnemonics ignored, case-insensitive), or nullptr
GtkWidget* findMenuItem(GtkWidget* menuShell, const std::string& label);

}  // namespace xoj::api::ui
