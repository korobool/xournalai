#include "DrawnText.h"

#include <algorithm>  // for max

namespace xoj::gui {

namespace {
constexpr const char* TEXT = "xoai-text";
constexpr const char* XALIGN = "xoai-text-xalign";
constexpr const char* ELLIPSIZE = "xoai-text-ellipsize";

/// asks for room for `text` if it needs more than it has (never less: no re-layout when the text gets shorter)
void fit(GtkWidget* w, const std::string& text) {
    PangoLayout* layout = gtk_widget_create_pango_layout(w, text.c_str());
    int width = 0;
    int height = 0;
    pango_layout_get_pixel_size(layout, &width, &height);
    g_object_unref(layout);
    int hasW = 0;
    int hasH = 0;
    gtk_widget_get_size_request(w, &hasW, &hasH);
    if (g_object_get_data(G_OBJECT(w), ELLIPSIZE)) {
        width = 0;  // (it takes the width it is given)
    }
    if (width + 2 > hasW || height > hasH) {
        gtk_widget_set_size_request(w, std::max(hasW, width + 2), std::max(hasH, height));
    }
}
}  // namespace

GtkWidget* drawnTextNew(const std::string& text, double xalign, bool ellipsize) {
    GtkWidget* w = gtk_drawing_area_new();
    if (ellipsize) {
        g_object_set_data(G_OBJECT(w), ELLIPSIZE, GINT_TO_POINTER(1));
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(w), "xoai-drawn-text");
    g_object_set_data_full(G_OBJECT(w), TEXT, g_strdup(text.c_str()), g_free);
    g_object_set_data_full(G_OBJECT(w), XALIGN, new double(xalign), +[](gpointer d) { delete static_cast<double*>(d); });
    g_signal_connect(w, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer) -> gboolean {
                         const auto* t = static_cast<const char*>(g_object_get_data(G_OBJECT(w), TEXT));
                         const double xalign = *static_cast<double*>(g_object_get_data(G_OBJECT(w), XALIGN));
                         PangoLayout* layout = gtk_widget_create_pango_layout(w, t ? t : "");
                         const int aw = gtk_widget_get_allocated_width(w);
                         if (g_object_get_data(G_OBJECT(w), ELLIPSIZE)) {
                             pango_layout_set_width(layout, aw * PANGO_SCALE);
                             pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
                         }
                         int width = 0;
                         int height = 0;
                         pango_layout_get_pixel_size(layout, &width, &height);
                         const int ah = gtk_widget_get_allocated_height(w);
                         gtk_render_layout(gtk_widget_get_style_context(w), cr, (aw - width) * xalign,
                                           (ah - height) / 2.0, layout);
                         g_object_unref(layout);
                         return TRUE;
                     }),
                     nullptr);
    // (its font is known once it is styled: then the room it needs)
    g_signal_connect(w, "style-updated", G_CALLBACK(+[](GtkWidget* w, gpointer) {
                         const auto* t = static_cast<const char*>(g_object_get_data(G_OBJECT(w), TEXT));
                         fit(w, t ? t : "");
                     }),
                     nullptr);
    fit(w, text);
    return w;
}

void drawnTextSet(GtkWidget* w, const std::string& text) {
    const auto* has = static_cast<const char*>(g_object_get_data(G_OBJECT(w), TEXT));
    if (has && text == has) {
        return;
    }
    g_object_set_data_full(G_OBJECT(w), TEXT, g_strdup(text.c_str()), g_free);
    fit(w, text);
    gtk_widget_queue_draw(w);  // (its own pixels only)
}

std::string drawnTextGet(GtkWidget* w) {
    const auto* t = static_cast<const char*>(g_object_get_data(G_OBJECT(w), TEXT));
    return t ? t : "";
}

}  // namespace xoj::gui
