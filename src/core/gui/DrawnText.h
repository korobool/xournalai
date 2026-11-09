/*
 * xournalai (based on Xournal++)
 *
 * A one-line text that draws itself (a GtkDrawingArea), for text that changes often (a running time, a countdown).
 * A GtkLabel given new text re-lays out the window, and GTK then repaints the whole window: thousands of X requests
 * each second while recording (measured 2026-11-09). This repaints only its own pixels; it asks for more room only
 * when a longer text comes. Its text is readable by the UI inspector (object data "xoai-text"), like a label's.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include <gtk/gtk.h>

namespace xoj::gui {

/// `xalign`: 0 left, 0.5 centred. `ellipsize`: it takes the width it is given and cuts the text with "…" (for a
/// line that fills a row); else it asks for the width of its longest text so far. The CSS class "xoai-drawn-text"
/// styles it (colour, font), as a label would be.
GtkWidget* drawnTextNew(const std::string& text, double xalign = 0.0, bool ellipsize = false);
void drawnTextSet(GtkWidget* widget, const std::string& text);
std::string drawnTextGet(GtkWidget* widget);

}  // namespace xoj::gui
