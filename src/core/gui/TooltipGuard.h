/*
 * xournalai (based on Xournal++)
 *
 * No tooltips for the pen or a finger. A GTK 3 tooltip asks the X server where the pointer is (XIQueryPointer) and
 * waits for the answer on the UI thread; with a Wacom pen stuck "in proximity" that answer never came and the whole
 * app froze (2026-10-05). The guard keeps hover motion of pens and touchscreens away from GTK outside the canvas,
 * so no tooltip timer starts; taps, drags and everything on the canvas pass as before. The mouse keeps its tooltips.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gtk/gtk.h>

namespace xoj::gui {

/// Whether an event would only feed GTK's tooltips: hover (no button held) by a pen, eraser or touchscreen, not over
/// the canvas
bool isTooltipHover(GdkEventType type, GdkInputSource source, GdkModifierType state, bool overCanvas);

/// Installs the guard for the window whose drawing area is `canvas`
void installTooltipGuard(GtkWidget* canvas);

}  // namespace xoj::gui
