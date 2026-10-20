#include "KeyboardFocus.h"

#include <set>  // for set

namespace xoj::gui {

namespace {
std::set<guint16> heldKeys;  ///< hardware keycodes
gint64 lastPressUs = 0;
/// Longer than the keyboard's delay before it repeats (GNOME: 500 ms), so that a held key stays held
constexpr gint64 HOLD_US = 800 * 1000;

bool isTextInput(GtkWidget* w) {
    if (GTK_IS_SPIN_BUTTON(w)) {
        return false;  // (typing a page number: the canvas should have the keys afterwards)
    }
    if (GTK_IS_ENTRY(w) || GTK_IS_TEXT_VIEW(w)) {
        return true;
    }
    // (by name: the terminal is optional, and only the AI terminal links VTE; the type exists once one was made)
    const GType terminal = g_type_from_name("VteTerminal");
    return terminal && g_type_is_a(G_OBJECT_TYPE(w), terminal);
}
}  // namespace

GtkWidget* textInputWithKeyboard(GtkWidget* widget) {
    GtkWidget* top = widget ? gtk_widget_get_toplevel(widget) : nullptr;
    if (!top || !GTK_IS_WINDOW(top)) {
        return nullptr;
    }
    GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(top));
    if (!focus || focus == widget || !gtk_widget_is_visible(focus) || !gtk_widget_is_sensitive(focus)) {
        return nullptr;
    }
    return isTextInput(focus) ? focus : nullptr;
}

void noteKeyEvent(const GdkEvent* event) {
    switch (event->type) {
        case GDK_KEY_PRESS:
            if (!event->key.is_modifier) {
                heldKeys.insert(event->key.hardware_keycode);
                lastPressUs = g_get_monotonic_time();
            }
            break;
        case GDK_KEY_RELEASE:
            heldKeys.erase(event->key.hardware_keycode);
            break;
        case GDK_FOCUS_CHANGE:
            if (!event->focus_change.in) {
                heldKeys.clear();  // (its releases go to another window)
            }
            break;
        default:
            break;
    }
}

bool keyHeld() { return !heldKeys.empty() && g_get_monotonic_time() - lastPressUs < HOLD_US; }

}  // namespace xoj::gui
