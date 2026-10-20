#include "KeyboardFocus.h"

namespace xoj::gui {

namespace {
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

}  // namespace xoj::gui
