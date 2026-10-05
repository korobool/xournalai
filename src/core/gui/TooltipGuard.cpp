#include "TooltipGuard.h"

namespace xoj::gui {

namespace {
GtkWidget* guardedCanvas = nullptr;  ///< weak

void filter(GdkEvent* event, gpointer) {
    const GdkEventType type = gdk_event_get_event_type(event);
    if (type == GDK_MOTION_NOTIFY || type == GDK_ENTER_NOTIFY) {
        GdkDevice* source = gdk_event_get_source_device(event);
        GdkModifierType state{};
        gdk_event_get_state(event, &state);
        GtkWidget* target = gtk_get_event_widget(event);
        const bool overCanvas =
                guardedCanvas && target && (target == guardedCanvas || gtk_widget_is_ancestor(target, guardedCanvas));
        if (source && isTooltipHover(type, gdk_device_get_source(source), state, overCanvas)) {
            return;  // (nothing else needs the hover of a pen over a button: no tooltip timer starts)
        }
    }
    gtk_main_do_event(event);
}
}  // namespace

bool isTooltipHover(GdkEventType type, GdkInputSource source, GdkModifierType state, bool overCanvas) {
    if (overCanvas || (type != GDK_MOTION_NOTIFY && type != GDK_ENTER_NOTIFY)) {
        return false;
    }
    if (source != GDK_SOURCE_PEN && source != GDK_SOURCE_ERASER && source != GDK_SOURCE_TOUCHSCREEN) {
        return false;
    }
    constexpr auto buttons =
            GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK | GDK_BUTTON5_MASK;
    return (state & buttons) == 0;  // (a drag, e.g. of a slider, still passes)
}

void installTooltipGuard(GtkWidget* canvas) {
    guardedCanvas = canvas;
    g_object_add_weak_pointer(G_OBJECT(canvas), reinterpret_cast<gpointer*>(&guardedCanvas));
    gdk_event_handler_set(filter, nullptr, nullptr);
}

}  // namespace xoj::gui
