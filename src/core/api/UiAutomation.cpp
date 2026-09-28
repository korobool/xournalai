#include "UiAutomation.h"

#include <algorithm>      // for remove, max, stable_sort
#include <cstdio>         // for snprintf
#include <unordered_map>  // for unordered_map

namespace xoj::api::ui {

namespace {

struct Registry {
    std::unordered_map<GtkWidget*, std::string> ids;
    std::unordered_map<std::string, GtkWidget*> widgets;
    unsigned next = 1;
};

Registry& registry() {
    static Registry r;
    return r;
}

void forget(gpointer, GObject* dead) {
    auto& r = registry();
    auto* w = reinterpret_cast<GtkWidget*>(dead);
    if (auto it = r.ids.find(w); it != r.ids.end()) {
        r.widgets.erase(it->second);
        r.ids.erase(it);
    }
}

std::string pngOf(cairo_surface_t* surface) {
    std::string out;
    cairo_surface_write_to_png_stream(
            surface,
            [](void* closure, const unsigned char* data, unsigned int length) {
                static_cast<std::string*>(closure)->append(reinterpret_cast<const char*>(data), length);
                return CAIRO_STATUS_SUCCESS;
            },
            &out);
    return out;
}

std::string role(GtkWidget* w) {
    if (GTK_IS_RADIO_BUTTON(w) || GTK_IS_RADIO_MENU_ITEM(w)) {
        return "radio";
    }
    if (GTK_IS_CHECK_BUTTON(w) || GTK_IS_CHECK_MENU_ITEM(w)) {
        return "check";
    }
    if (GTK_IS_TOGGLE_BUTTON(w) || GTK_IS_TOGGLE_TOOL_BUTTON(w)) {
        return "toggle";
    }
    if (GTK_IS_SWITCH(w)) {
        return "switch";
    }
    if (GTK_IS_COLOR_BUTTON(w)) {
        return "color";
    }
    if (GTK_IS_FONT_BUTTON(w)) {
        return "font";
    }
    if (GTK_IS_BUTTON(w) || GTK_IS_TOOL_BUTTON(w)) {
        return "button";
    }
    if (GTK_IS_SPIN_BUTTON(w)) {
        return "spin";
    }
    if (GTK_IS_ENTRY(w)) {
        return "entry";
    }
    if (GTK_IS_TEXT_VIEW(w)) {
        return "text";
    }
    if (GTK_IS_SCALE(w)) {
        return "scale";
    }
    if (GTK_IS_COMBO_BOX(w)) {
        return "combo";
    }
    if (GTK_IS_MENU_ITEM(w)) {
        return "menu_item";
    }
    if (GTK_IS_TREE_VIEW(w)) {
        return "list";
    }
    if (GTK_IS_LIST_BOX(w)) {
        return "list";
    }
    if (GTK_IS_LIST_BOX_ROW(w)) {
        return "row";
    }
    if (GTK_IS_NOTEBOOK(w)) {
        return "tabs";
    }
    if (GTK_IS_FILE_CHOOSER(w)) {
        return "file_chooser";
    }
    if (GTK_IS_LABEL(w)) {
        return "label";
    }
    if (GTK_IS_IMAGE(w)) {
        return "image";
    }
    if (GTK_IS_DRAWING_AREA(w)) {
        return "canvas";
    }
    return "container";
}

std::optional<std::string> value(GtkWidget* w) {
    char buf[64];
    if (GTK_IS_TOGGLE_BUTTON(w)) {
        return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)) ? "true" : "false";
    }
    if (GTK_IS_TOGGLE_TOOL_BUTTON(w)) {
        return gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(w)) ? "true" : "false";
    }
    if (GTK_IS_CHECK_MENU_ITEM(w)) {
        return gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)) ? "true" : "false";
    }
    if (GTK_IS_SWITCH(w)) {
        return gtk_switch_get_active(GTK_SWITCH(w)) ? "true" : "false";
    }
    if (GTK_IS_SPIN_BUTTON(w)) {
        std::snprintf(buf, sizeof(buf), "%g", gtk_spin_button_get_value(GTK_SPIN_BUTTON(w)));
        return buf;
    }
    if (GTK_IS_ENTRY(w)) {
        return gtk_entry_get_text(GTK_ENTRY(w));
    }
    if (GTK_IS_RANGE(w)) {
        std::snprintf(buf, sizeof(buf), "%g", gtk_range_get_value(GTK_RANGE(w)));
        return buf;
    }
    if (GTK_IS_COMBO_BOX_TEXT(w)) {
        gchar* t = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(w));
        std::string s = t ? t : "";
        g_free(t);
        return s;
    }
    if (GTK_IS_COMBO_BOX(w)) {
        std::snprintf(buf, sizeof(buf), "%d", gtk_combo_box_get_active(GTK_COMBO_BOX(w)));
        return std::string("index ") + buf;
    }
    if (GTK_IS_COLOR_CHOOSER(w)) {
        GdkRGBA c;
        gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(w), &c);
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", static_cast<int>(c.red * 255), static_cast<int>(c.green * 255),
                      static_cast<int>(c.blue * 255));
        return buf;
    }
    if (GTK_IS_TEXT_VIEW(w)) {
        GtkTextBuffer* b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(w));
        GtkTextIter s, e;
        gtk_text_buffer_get_bounds(b, &s, &e);
        gchar* t = gtk_text_buffer_get_text(b, &s, &e, FALSE);
        std::string out = t ? t : "";
        g_free(t);
        return out;
    }
    if (GTK_IS_NOTEBOOK(w)) {
        const int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w));
        GtkWidget* child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w), p);
        const char* label = child ? gtk_notebook_get_tab_label_text(GTK_NOTEBOOK(w), child) : nullptr;
        return label ? label : std::to_string(p);
    }
    return std::nullopt;
}

void collect(GtkWidget* w, GtkWidget* top, int depth, int maxDepth, bool interestingOnly,
             std::vector<WidgetInfo>& out) {
    const bool visible = gtk_widget_get_visible(w) && gtk_widget_get_child_visible(w);
    if (interestingOnly && !visible) {
        return;
    }
    WidgetInfo info;
    info.widget = w;
    info.type = G_OBJECT_TYPE_NAME(w);
    info.role = role(w);
    if (GTK_IS_BUILDABLE(w)) {
        const char* n = gtk_buildable_get_name(GTK_BUILDABLE(w));
        if (n && std::string(n).rfind("__", 0) != 0) {
            info.name = n;
        }
    }
    info.label = widgetText(w);
    if (gchar* tip = gtk_widget_get_tooltip_text(w)) {
        info.tooltip = tip;
        g_free(tip);
    }
    info.value = value(w);
    info.sensitive = gtk_widget_is_sensitive(w);
    info.visible = visible;
    info.depth = depth;
    info.interactive = info.role != "container" && info.role != "label" && info.role != "image";
    if (top) {
        gint x = 0, y = 0;
        gtk_widget_translate_coordinates(w, top, 0, 0, &x, &y);
        info.x = x;
        info.y = y;
    }
    GtkAllocation a;
    gtk_widget_get_allocation(w, &a);
    info.width = a.width;
    info.height = a.height;
    const bool boring = interestingOnly && info.role == "container" && info.label.empty() && info.name.empty() &&
                        info.tooltip.empty();
    if (!boring) {
        out.push_back(info);
    }
    if (maxDepth >= 0 && depth >= maxDepth) {
        return;
    }
    // Menus shown by menu items are separate windows; their items are reachable through the menu model instead
    if (GTK_IS_CONTAINER(w)) {
        struct Ctx {
            GtkWidget* top;
            int depth, maxDepth;
            bool interestingOnly;
            std::vector<WidgetInfo>* out;
        } ctx{top, boring ? depth : depth + 1, maxDepth, interestingOnly, &out};
        gtk_container_forall(
                GTK_CONTAINER(w),
                [](GtkWidget* child, gpointer data) {
                    auto* c = static_cast<Ctx*>(data);
                    collect(child, c->top, c->depth, c->maxDepth, c->interestingOnly, *c->out);
                },
                &ctx);
    }
}

}  // namespace

std::string idOf(GtkWidget* w) {
    auto& r = registry();
    if (auto it = r.ids.find(w); it != r.ids.end()) {
        return it->second;
    }
    std::string id = "w" + std::to_string(r.next++);
    r.ids.emplace(w, id);
    r.widgets.emplace(id, w);
    g_object_weak_ref(G_OBJECT(w), forget, nullptr);
    return id;
}

GtkWidget* lookup(const std::string& id) {
    auto& r = registry();
    auto it = r.widgets.find(id);
    return it == r.widgets.end() ? nullptr : it->second;
}

std::string widgetText(GtkWidget* w) {
    if (GTK_IS_LABEL(w)) {
        return gtk_label_get_text(GTK_LABEL(w));
    }
    if (GTK_IS_MENU_ITEM(w)) {
        const char* l = gtk_menu_item_get_label(GTK_MENU_ITEM(w));
        if (l) {
            std::string s = l;
            s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
            return s;
        }
    }
    if (GTK_IS_TOOL_BUTTON(w)) {
        const char* l = gtk_tool_button_get_label(GTK_TOOL_BUTTON(w));
        if (l) {
            return l;
        }
    }
    if (GTK_IS_BUTTON(w)) {
        const char* l = gtk_button_get_label(GTK_BUTTON(w));
        if (l) {
            std::string s = l;
            s.erase(std::remove(s.begin(), s.end(), '_'), s.end());
            return s;
        }
    }
    if (GTK_IS_ENTRY(w)) {
        const char* p = gtk_entry_get_placeholder_text(GTK_ENTRY(w));
        return p ? p : "";
    }
    if (GTK_IS_WINDOW(w)) {
        const char* t = gtk_window_get_title(GTK_WINDOW(w));
        return t ? t : "";
    }
    return {};
}

std::vector<WindowInfo> windows(GtkWidget* mainWindow) {
    std::vector<WindowInfo> out;
    GList* tops = gtk_window_list_toplevels();
    for (GList* l = tops; l; l = l->next) {
        auto* w = GTK_WIDGET(l->data);
        if (!gtk_widget_get_visible(w)) {
            continue;
        }
        WindowInfo info;
        info.window = w;
        const char* title = gtk_window_get_title(GTK_WINDOW(w));
        info.title = title ? title : "";
        info.modal = gtk_window_get_modal(GTK_WINDOW(w));
        info.focused = gtk_window_is_active(GTK_WINDOW(w));
        if (w == mainWindow) {
            info.kind = "main";
        } else if (GTK_IS_FILE_CHOOSER(w)) {
            info.kind = "file_chooser";
        } else if (GTK_IS_DIALOG(w) || GTK_IS_MESSAGE_DIALOG(w)) {
            info.kind = "dialog";
        } else if (gtk_window_get_window_type(GTK_WINDOW(w)) == GTK_WINDOW_POPUP) {
            GtkWidget* child = gtk_bin_get_child(GTK_BIN(w));
            info.kind = child && GTK_IS_MENU(child) ? "menu" : "popup";
        } else {
            info.kind = gtk_window_get_transient_for(GTK_WINDOW(w)) ? "dialog" : "window";
        }
        out.push_back(info);
    }
    g_list_free(tops);
    std::stable_sort(out.begin(), out.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return (a.kind != "main") > (b.kind != "main");  // dialogs and menus before the main window
    });
    return out;
}

std::vector<WidgetInfo> inspect(GtkWidget* root, int maxDepth, bool interestingOnly) {
    std::vector<WidgetInfo> out;
    GtkWidget* top = gtk_widget_get_toplevel(root);
    collect(root, top, 0, maxDepth, interestingOnly, out);
    return out;
}

std::string screenshot(GtkWidget* widget) {
    GtkAllocation a;
    gtk_widget_get_allocation(widget, &a);
    const int w = std::max(1, a.width), h = std::max(1, a.height);
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    gtk_widget_draw(widget, cr);
    cairo_destroy(cr);
    std::string png = pngOf(surface);
    cairo_surface_destroy(surface);
    return png;
}

}  // namespace xoj::api::ui
