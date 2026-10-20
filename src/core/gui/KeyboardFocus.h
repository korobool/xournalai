/*
 * xournalai (based on Xournal++)
 *
 * Where the keyboard is. The app moves the keyboard to the canvas by itself after a save and whenever a page becomes
 * current (also through scrolling or an agent's work), and that took it from the AI terminal and the AI input fields
 * while the user typed or dictated there (2026-10-20: a space held for dictation then scrolled the canvas a screen
 * per repeat). Such moves leave the keyboard where the user types; the user's own clicks and strokes still take it.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <gtk/gtk.h>

namespace xoj::gui {

/// The widget of `widget`'s window that has the keyboard if it is a text input the user types into (the AI
/// terminal, an entry, a text view; not a spin button such as the page number), visible and sensitive; else nullptr
GtkWidget* textInputWithKeyboard(GtkWidget* widget);

/**
 * Follows the keys held down, from the app's event filter (all key events pass there; the main window's own key
 * handler gives some to the focused widget before its other handlers could see them). A key counts as held while
 * it repeats: pressed, not released, and its last press (or repeat) less than a moment ago, so a release that never
 * arrived cannot leave it held. Modifiers don't count.
 */
void noteKeyEvent(const GdkEvent* event);

/// Whether a key is held down now (e.g. space for push-to-talk dictation in the AI terminal)
bool keyHeld();

}  // namespace xoj::gui
