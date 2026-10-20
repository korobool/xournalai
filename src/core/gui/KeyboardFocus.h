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

}  // namespace xoj::gui
