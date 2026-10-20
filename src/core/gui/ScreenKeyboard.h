/*
 * xournalai (based on Xournal++)
 *
 * The on-screen keyboard only when called. In tablet mode GNOME Shell (42, X11) opens its keyboard whenever a text
 * field of any app tells the input method (IBus) that it has the keyboard, and closes it when it lets go; the
 * window slides up and down with it. In xournalai the keyboard moves between the AI terminal, the AI input fields
 * and the canvas all the time, so the screen jumped (2026-10-20). In manual mode the app's text fields use GTK's own
 * input method, and GNOME never hears of them: its keyboard comes only when called (swipe up from the bottom edge)
 * and stays until closed. The owner's layouts are XKB ones (us, ru, ua, compose): typing, switching and compose
 * keep working; only IBus engines would not.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

namespace xoj::gui::screen_keyboard {

/// At the very start, before GTK and any thread: takes GTK_IM_MODULE out of the environment, where it would override
/// the app's choice, and remembers it (also for the app started again after a freeze). Not for runs without a window
/// (the MCP bridge, hooks)
void takeOverInputMethod(int argc, char** argv);

/// The input method the session asked for (GTK_IM_MODULE), for programs started from the app (the AI terminal);
/// nullptr if none
const char* sessionInputMethod();

/// Manual: GNOME's keyboard never opens by itself for the app. Else the session's input method, as before. Takes
/// effect on the next focus of a text field.
void setManual(bool manual);
bool isManual();

}  // namespace xoj::gui::screen_keyboard
