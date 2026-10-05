#include "TooltipGuard.h"

#include "gui/inputdevices/PenButtonObserver.h"  // for penBarrelGone

namespace xoj::gui {

namespace {
GtkWidget* guardedCanvas = nullptr;  ///< weak

bool overGuardedCanvas(GdkEvent* event) {
    GtkWidget* target = gtk_get_event_widget(event);
    return guardedCanvas && target && (target == guardedCanvas || gtk_widget_is_ancestor(target, guardedCanvas));
}

void filter(GdkEvent* event, gpointer) {
    const GdkEventType type = gdk_event_get_event_type(event);
    // A pen whose button is let go (or that leaves the tablet's range) off the canvas: the canvas would never know
    // and a pen-button Ask would keep listening
    if (type == GDK_BUTTON_RELEASE || type == GDK_PROXIMITY_OUT) {
        GdkDevice* source = gdk_event_get_source_device(event);
        const GdkInputSource s = source ? gdk_device_get_source(source) : GDK_SOURCE_MOUSE;
        guint button = 0;
        gdk_event_get_button(event, &button);
        if ((s == GDK_SOURCE_PEN || s == GDK_SOURCE_ERASER) && (type == GDK_PROXIMITY_OUT || button == 2) &&
            !overGuardedCanvas(event)) {
            xoj::input::penBarrelGone();
        }
    }
    if (type == GDK_MOTION_NOTIFY || type == GDK_ENTER_NOTIFY) {
        GdkDevice* source = gdk_event_get_source_device(event);
        GdkModifierType state{};
        gdk_event_get_state(event, &state);
        const bool overCanvas = overGuardedCanvas(event);
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
