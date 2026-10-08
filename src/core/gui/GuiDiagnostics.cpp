#include "GuiDiagnostics.h"

#include <string>  // for string

#include <gtk/gtk.h>

#include "util/StallWatch.h"  // for stall::note

namespace {
gboolean onDestroy(GSignalInvocationHint*, guint n, const GValue* values, gpointer) {
    if (n < 1) {
        return TRUE;
    }
    GObject* obj = static_cast<GObject*>(g_value_get_object(&values[0]));
    if (!obj || !GTK_IS_POPOVER(obj)) {
        return TRUE;  // (keep the hook)
    }
    GtkWidget* w = GTK_WIDGET(obj);
    GtkWidget* rel = gtk_popover_get_relative_to(GTK_POPOVER(w));
    const char* name = GTK_IS_BUILDABLE(w) ? gtk_buildable_get_name(GTK_BUILDABLE(w)) : nullptr;
    std::string line = "popover destroyed: " + std::string(G_OBJECT_TYPE_NAME(obj)) + " \"" + (name ? name : "") +
                       "\" relative to " + (rel ? G_OBJECT_TYPE_NAME(rel) : "nothing") +
                       (gtk_widget_get_visible(w) ? ", still visible" : "") +
                       (gtk_widget_get_mapped(w) ? ", mapped" : "");
    xoj::util::stall::note(line);
    return TRUE;
}

GLogWriterOutput writer(GLogLevelFlags level, const GLogField* fields, gsize n, gpointer) {
    if (level & G_LOG_LEVEL_CRITICAL) {
        static int reported = 0;
        for (gsize i = 0; i < n && reported < 3; i++) {
            if (g_strcmp0(fields[i].key, "MESSAGE") == 0 && fields[i].length < 0 &&
                g_strstr_len(static_cast<const char*>(fields[i].value), -1, "GTK_IS_WIDGET")) {
                reported++;
                xoj::util::stall::note(std::string("GTK critical: ") + static_cast<const char*>(fields[i].value), true);
            }
        }
    }
    return g_log_writer_default(level, fields, n, nullptr);
}
}  // namespace

void installGuiDiagnostics() {
    g_signal_add_emission_hook(g_signal_lookup("destroy", GTK_TYPE_WIDGET), 0, onDestroy, nullptr, nullptr);
    g_log_set_writer_func(writer, nullptr, nullptr);
}
