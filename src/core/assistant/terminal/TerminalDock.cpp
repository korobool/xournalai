#include "TerminalDock.h"

#include <csignal>  // for kill, SIGHUP

#include <algorithm>  // for clamp

#include <vte/vte.h>

#include "gui/ScreenKeyboard.h"  // for sessionInputMethod
#include "util/PathUtil.h"       // for getConfigFile

namespace xoj::assistant {

struct TerminalDock::Tab {
    TerminalDock* owner = nullptr;
    TerminalSpec spec;
    GtkWidget* page = nullptr;  ///< weak: destroyed with the window
    VteTerminal* term = nullptr;
    GtkWidget* label = nullptr;
    GPid pid = -1;
    bool running = false;
};

namespace {
/// The user's login shell (the app started from the desktop doesn't have the terminal's PATH: nvm, ~/.local/bin, …)
std::string loginShell() {
    const char* s = g_getenv("SHELL");
    return s && *s ? s : "/bin/bash";
}

constexpr double MIN_OPACITY = 0.2;

/// The see-through look: no notebook background under the floating terminal, solid controls
void installCss() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(
            css,
            ".xoai-term-floating notebook, .xoai-term-floating notebook > stack { background-color: transparent; }",
            -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

bool isToggleKey(GdkEventKey* e) {
    return (e->state & GDK_CONTROL_MASK) && (e->keyval == GDK_KEY_grave || e->keyval == GDK_KEY_dead_grave);
}
}  // namespace

TerminalDock::TerminalDock(GtkWidget* mainBox, GtkWidget* contentArea) {
    // Put the content area and the dock into a vertical split, at the content area's place in the main box
    gboolean expand = TRUE, fill = TRUE;
    guint padding = 0;
    GtkPackType packType = GTK_PACK_START;
    gtk_box_query_child_packing(GTK_BOX(mainBox), contentArea, &expand, &fill, &padding, &packType);
    int position = 0;
    GList* children = gtk_container_get_children(GTK_CONTAINER(mainBox));
    position = g_list_index(children, contentArea);
    g_list_free(children);

    g_object_ref(contentArea);
    gtk_container_remove(GTK_CONTAINER(mainBox), contentArea);
    paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_buildable_set_name(GTK_BUILDABLE(paned), "aiTerminalPaned");
    gtk_paned_pack1(GTK_PANED(paned), contentArea, TRUE, FALSE);
    g_object_unref(contentArea);

    dock = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_buildable_set_name(GTK_BUILDABLE(dock), "aiTerminalDock");
    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    buildHeader();
    gtk_box_pack_start(GTK_BOX(dock), notebook, TRUE, TRUE, 0);
    gtk_paned_pack2(GTK_PANED(paned), dock, FALSE, FALSE);

    // The floating dock lies over the split (over the page), at the bottom
    overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(overlay), paned);
    // The floating dock gets exactly the height dragged to (the overlay would give it the terminal's natural height)
    g_signal_connect(overlay, "get-child-position",
                     G_CALLBACK(+[](GtkOverlay* o, GtkWidget* child, GdkRectangle* r, gpointer d) -> gboolean {
                         auto* self = static_cast<TerminalDock*>(d);
                         if (child != self->dock) {
                             return FALSE;
                         }
                         const int w = gtk_widget_get_allocated_width(GTK_WIDGET(o));
                         const int room = gtk_widget_get_allocated_height(GTK_WIDGET(o));
                         int minH = 0;
                         gtk_widget_get_preferred_height(child, &minH, nullptr);
                         const int h = std::max(minH, std::min(self->lastHeight, std::max(80, room - 40)));
                         r->x = 0;
                         r->y = room - h;
                         r->width = w;
                         r->height = h;
                         return TRUE;
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(mainBox), overlay, expand, fill, padding);
    gtk_box_set_child_packing(GTK_BOX(mainBox), overlay, expand, fill, padding, packType);
    gtk_box_reorder_child(GTK_BOX(mainBox), overlay, position);
    gtk_widget_show(overlay);
    gtk_widget_show(paned);
    gtk_widget_show(contentArea);
    gtk_widget_show_all(dock);
    gtk_widget_hide(dock);  // collapsed until shown

    g_object_add_weak_pointer(G_OBJECT(overlay), reinterpret_cast<gpointer*>(&overlay));
    g_object_add_weak_pointer(G_OBJECT(paned), reinterpret_cast<gpointer*>(&paned));
    g_object_add_weak_pointer(G_OBJECT(dock), reinterpret_cast<gpointer*>(&dock));
    g_object_add_weak_pointer(G_OBJECT(notebook), reinterpret_cast<gpointer*>(&notebook));

    loadView();
    const bool f = floating;
    floating = !f;  // (so that setFloating moves it)
    setFloating(f);
    // Automatic colours follow the page under the terminal (switching pages, changing its background)
    themeTimer = g_timeout_add(500, +[](gpointer d) -> gboolean {
        static_cast<TerminalDock*>(d)->updateTheme();
        return G_SOURCE_CONTINUE;
    }, this);
}

TerminalDock::~TerminalDock() {
    if (themeTimer) {
        g_source_remove(themeTimer);
    }
    for (Tab* t: tabs) {
        if (t->running && t->pid > 0) {
            kill(t->pid, SIGHUP);  // like closing a terminal window
        }
        if (t->page) {
            g_object_remove_weak_pointer(G_OBJECT(t->page), reinterpret_cast<gpointer*>(&t->page));
        }
        delete t;
    }
    if (pageCss) {
        gtk_style_context_remove_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(pageCss));
        g_object_unref(pageCss);
    }
    for (GtkWidget** w: {&overlay, &paned, &dock, &notebook}) {
        if (*w) {
            g_object_remove_weak_pointer(G_OBJECT(*w), reinterpret_cast<gpointer*>(w));
        }
    }
}

void TerminalDock::buildHeader() {
    installCss();
    gtk_style_context_add_class(gtk_widget_get_style_context(dock), "xoai-term-dock");

    // One row: the title and the tabs on the left, the controls on the right. The title and the row's empty space
    // are the handle: drag them up or down to make the terminal taller or shorter
    grip = gtk_event_box_new();
    gtk_buildable_set_name(GTK_BUILDABLE(grip), "aiTerminalHandle");
    gtk_widget_add_events(grip, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(grip, "realize", G_CALLBACK(+[](GtkWidget* w, gpointer) {
                         GdkCursor* c = gdk_cursor_new_from_name(gtk_widget_get_display(w), "ns-resize");
                         gdk_window_set_cursor(gtk_widget_get_window(w), c);
                         if (c) {
                             g_object_unref(c);
                         }
                     }),
                     nullptr);
    GtkWidget* title = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(title), "<b>AI terminal</b>");
    g_object_set(title, "margin-start", 8, "margin-end", 8, nullptr);
    gtk_widget_set_tooltip_text(grip, "Drag up or down to resize the terminal");
    gtk_container_add(GTK_CONTAINER(grip), title);
    gtk_widget_show_all(grip);
    gtk_notebook_set_action_widget(GTK_NOTEBOOK(notebook), grip, GTK_PACK_START);

    auto press = +[](GtkWidget* w, GdkEventButton* e, gpointer d) -> gboolean {
        auto* self = static_cast<TerminalDock*>(d);
        if (e->type != GDK_BUTTON_PRESS || e->button != 1 || (w == self->notebook && !self->onEmptyHeader(e))) {
            return FALSE;
        }
        self->dragging = true;
        self->gripStartY = e->y_root;
        self->gripStartHeight = gtk_widget_get_allocated_height(self->dock);
        return TRUE;
    };
    auto motion = +[](GtkWidget*, GdkEventMotion* e, gpointer d) -> gboolean {
        auto* self = static_cast<TerminalDock*>(d);
        if (!self->dragging) {
            return FALSE;
        }
        if (!(e->state & GDK_BUTTON1_MASK)) {  // released where we didn't hear it: stop following the pointer
            self->endDrag();
            return FALSE;
        }
        self->dragTo(e->y_root);
        return TRUE;
    };
    auto release = +[](GtkWidget*, GdkEventButton* e, gpointer d) -> gboolean {
        auto* self = static_cast<TerminalDock*>(d);
        if (!self->dragging || e->button != 1) {
            return FALSE;
        }
        self->dragTo(e->y_root);
        self->endDrag();
        return TRUE;
    };
    for (GtkWidget* w: {grip, notebook}) {
        gtk_widget_add_events(w, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
        g_signal_connect(w, "button-press-event", G_CALLBACK(press), this);
        g_signal_connect(w, "motion-notify-event", G_CALLBACK(motion), this);
        g_signal_connect(w, "button-release-event", G_CALLBACK(release), this);
        g_signal_connect(w, "grab-broken-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer d) -> gboolean {
                             static_cast<TerminalDock*>(d)->endDrag();
                             return FALSE;
                         }),
                         this);
    }

    GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    g_object_set(header, "margin-start", 6, "margin-end", 4, nullptr);
    controls = header;
    gtk_buildable_set_name(GTK_BUILDABLE(header), "aiTerminalControls");
    auto button = [&](const char* icon, const char* tip, GCallback cb) {
        GtkWidget* b = gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(b, tip);
        gtk_widget_set_can_focus(b, FALSE);
        g_signal_connect(b, "clicked", cb, this);
        gtk_box_pack_end(GTK_BOX(header), b, FALSE, FALSE, 0);
        return b;
    };
    button("window-close-symbolic", "Hide the terminal (processes keep running) - Ctrl+`",
           G_CALLBACK(+[](GtkButton*, gpointer d) { static_cast<TerminalDock*>(d)->hide(); }));
    button("view-refresh-symbolic", "Restart the program in this tab", G_CALLBACK(+[](GtkButton*, gpointer d) {
               auto* self = static_cast<TerminalDock*>(d);
               self->restartTab(self->currentTab());
           }));
    GtkWidget* plus = gtk_menu_button_new();
    gtk_button_set_image(GTK_BUTTON(plus), gtk_image_new_from_icon_name("list-add-symbolic", GTK_ICON_SIZE_MENU));
    gtk_button_set_relief(GTK_BUTTON(plus), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(plus, "Open a new tab: Claude, Codex, OpenCode, a shell or your own command");
    gtk_buildable_set_name(GTK_BUILDABLE(plus), "aiTerminalNew");
    plusButton = plus;
    gtk_widget_set_can_focus(plus, FALSE);
    newTabMenu = gtk_menu_new();
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(plus), newTabMenu);
    gtk_box_pack_end(GTK_BOX(header), plus, FALSE, FALSE, 0);

    // Docked under the page (pressed) or floating over it
    pinButton = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(pinButton), gtk_image_new_from_icon_name("go-bottom-symbolic", GTK_ICON_SIZE_MENU));
    gtk_button_set_relief(GTK_BUTTON(pinButton), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(pinButton, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(pinButton), "aiTerminalPin");
    gtk_widget_set_tooltip_text(pinButton, "Dock the terminal under the page (pressed) or let it float over the page");
    g_signal_connect(pinButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
                         auto* self = static_cast<TerminalDock*>(d);
                         if (!self->syncing) {
                             self->setFloating(!gtk_toggle_button_get_active(b));
                         }
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(header), pinButton, FALSE, FALSE, 0);

    // Automatic colours: dark text on a light background over a light page
    autoButton = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(autoButton),
                         gtk_image_new_from_icon_name("display-brightness-symbolic", GTK_ICON_SIZE_MENU));
    gtk_button_set_relief(GTK_BUTTON(autoButton), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(autoButton, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(autoButton), "aiTerminalAutoColors");
    gtk_widget_set_tooltip_text(autoButton, "Automatic colours (pressed): dark text on a light background over a "
                                            "light page, light text on dark otherwise. For Claude Code, /theme light "
                                            "matches the light look");
    g_signal_connect(autoButton, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
                         auto* self = static_cast<TerminalDock*>(d);
                         if (!self->syncing) {
                             self->setAutoColors(gtk_toggle_button_get_active(b));
                         }
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(header), autoButton, FALSE, FALSE, 0);

    // How much of the page shows through the floating terminal (the text stays solid)
    opacityLabel = gtk_label_new("70%");
    gtk_label_set_width_chars(GTK_LABEL(opacityLabel), 4);
    gtk_box_pack_end(GTK_BOX(header), opacityLabel, FALSE, FALSE, 0);
    opacityScale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, MIN_OPACITY * 100, 100, 5);
    gtk_scale_set_draw_value(GTK_SCALE(opacityScale), FALSE);
    gtk_widget_set_size_request(opacityScale, 130, -1);
    gtk_widget_set_can_focus(opacityScale, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(opacityScale), "aiTerminalOpacity");
    gtk_widget_set_tooltip_text(opacityScale, "Background opacity of the floating terminal: lower shows more of the page");
    g_signal_connect(opacityScale, "value-changed", G_CALLBACK(+[](GtkRange* r, gpointer d) {
                         auto* self = static_cast<TerminalDock*>(d);
                         if (!self->syncing) {
                             self->setOpacity(gtk_range_get_value(r) / 100.0);
                         }
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(header), opacityScale, FALSE, FALSE, 0);
    gtk_widget_show_all(header);
    gtk_notebook_set_action_widget(GTK_NOTEBOOK(notebook), header, GTK_PACK_END);
}

namespace {
/// A widget's rectangle on the screen
bool screenRect(GtkWidget* w, GdkRectangle& r) {
    if (!w || !gtk_widget_get_mapped(w)) {
        return false;
    }
    GtkWidget* top = gtk_widget_get_toplevel(w);
    int x = 0, y = 0, ox = 0, oy = 0;
    if (!gtk_widget_get_window(top) || !gtk_widget_translate_coordinates(w, top, 0, 0, &x, &y)) {
        return false;
    }
    gdk_window_get_origin(gtk_widget_get_window(top), &ox, &oy);
    r.x = ox + x;
    r.y = oy + y;
    r.width = gtk_widget_get_allocated_width(w);
    r.height = gtk_widget_get_allocated_height(w);
    return true;
}
bool inside(const GdkRectangle& r, double x, double y, int slack) {
    return x >= r.x - slack && x < r.x + r.width + slack && y >= r.y - slack && y < r.y + r.height + slack;
}
}  // namespace

bool TerminalDock::onEmptyHeader(GdkEventButton* e) const {
    // In the row of tabs (as high as the controls), but not on a tab
    GdkRectangle row{}, nb{};
    if (!controls || !screenRect(controls, row) || !screenRect(notebook, nb)) {
        return false;
    }
    if (e->y_root < nb.y || e->y_root > row.y + row.height + 4) {
        return false;
    }
    for (Tab* t: tabs) {
        GdkRectangle tab{};
        if (t->page && screenRect(gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), t->page), tab) &&
            inside(tab, e->x_root, e->y_root, 10)) {
            return false;
        }
    }
    return true;
}

void TerminalDock::dragTo(double yRoot) {
    const int room = overlay ? gtk_widget_get_allocated_height(overlay) : 2000;
    lastHeight = std::clamp(gripStartHeight + static_cast<int>(gripStartY - yRoot), 80, std::max(80, room - 40));
    if (floating) {
        gtk_widget_queue_resize(dock);
    } else if (paned) {
        gtk_paned_set_position(GTK_PANED(paned), gtk_widget_get_allocated_height(paned) - lastHeight);
    }
}

void TerminalDock::endDrag() {
    if (dragging) {
        dragging = false;
        saveView();
    }
}

namespace {
GdkRGBA rgb(const char* spec) {
    GdkRGBA c;
    gdk_rgba_parse(&c, spec);
    return c;
}
}  // namespace

void TerminalDock::applyColors(Tab* tab) const {
    if (!tab || !tab->term) {
        return;
    }
    if (light) {  // a palette made for a light background (Tango, darker brights)
        static const char* const names[16] = {"#2e3436", "#cc0000", "#3a7d00", "#9a6a00", "#204a87", "#75507b",
                                              "#06787a", "#5c6266", "#555753", "#a40000", "#2f6a00", "#7a5200",
                                              "#1c3f74", "#5c3566", "#045d5f", "#2e3436"};
        GdkRGBA palette[16];
        for (int i = 0; i < 16; i++) {
            palette[i] = rgb(names[i]);
        }
        const GdkRGBA fg = rgb("#1b1b1b"), bg = rgb("#ffffff");
        vte_terminal_set_colors(tab->term, &fg, &bg, palette, 16);
        vte_terminal_set_color_cursor(tab->term, &fg);
        vte_terminal_set_color_cursor_foreground(tab->term, &bg);
    } else {
        vte_terminal_set_colors(tab->term, nullptr, nullptr, nullptr, 0);  // VTE's own (light on black)
        vte_terminal_set_color_cursor(tab->term, nullptr);
        vte_terminal_set_color_cursor_foreground(tab->term, nullptr);
    }
    // Floating: VTE leaves its background undrawn (its own alpha isn't honoured inside the window); the tab's box
    // under it paints the see-through black instead
    vte_terminal_set_clear_background(tab->term, !floating);
}

void TerminalDock::applyPageCss() {
    if (!pageCss) {
        pageCss = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(pageCss),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
    }
    const std::string css = std::string(".xoai-term-page { background-color: rgba(") +
                            (light ? "255, 255, 255, " : "0, 0, 0, ") +
                            std::to_string(floating ? opacity : 1.0) + "); }";
    gtk_css_provider_load_from_data(pageCss, css.c_str(), -1, nullptr);
}

void TerminalDock::updateTheme() {
    double b = autoColors && pageBrightness ? pageBrightness() : -1;
    const bool want = b > 0.5;
    if (want != light) {
        light = want;
        for (Tab* t: tabs) {
            applyColors(t);
        }
        applyPageCss();
    }
}

void TerminalDock::setAutoColors(bool on) {
    autoColors = on;
    syncing = true;
    if (autoButton) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoButton), on);
    }
    syncing = false;
    updateTheme();
    saveView();
}

void TerminalDock::setOpacity(double value) {
    opacity = std::clamp(value, MIN_OPACITY, 1.0);
    syncing = true;
    if (opacityScale) {
        gtk_range_set_value(GTK_RANGE(opacityScale), opacity * 100);
    }
    syncing = false;
    if (opacityLabel) {
        gtk_label_set_text(GTK_LABEL(opacityLabel), (std::to_string(static_cast<int>(opacity * 100 + 0.5)) + "%").c_str());
    }
    applyPageCss();
    saveView();
}

void TerminalDock::setFloating(bool f) {
    if (!dock || !overlay || !paned) {
        return;
    }
    if (f != floating) {
        floating = f;
        GtkWidget* parent = gtk_widget_get_parent(dock);
        g_object_ref(dock);
        if (parent) {
            gtk_container_remove(GTK_CONTAINER(parent), dock);
        }
        if (floating) {
            gtk_widget_set_valign(dock, GTK_ALIGN_FILL);  // (placed at the bottom by get-child-position)
            gtk_widget_set_halign(dock, GTK_ALIGN_FILL);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), dock);
        } else {
            gtk_paned_pack2(GTK_PANED(paned), dock, FALSE, FALSE);
            const int total = gtk_widget_get_allocated_height(paned);
            if (total > lastHeight + 100) {
                gtk_paned_set_position(GTK_PANED(paned), total - lastHeight);
            }
        }
        g_object_unref(dock);
        GtkStyleContext* ctx = gtk_widget_get_style_context(dock);
        floating ? gtk_style_context_add_class(ctx, "xoai-term-floating")
                 : gtk_style_context_remove_class(ctx, "xoai-term-floating");
        for (Tab* t: tabs) {
            applyColors(t);
        }
        applyPageCss();
        saveView();
    }
    syncing = true;
    if (pinButton) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pinButton), !floating);
    }
    syncing = false;
    if (opacityScale) {
        gtk_widget_set_sensitive(opacityScale, floating);
    }
}

void TerminalDock::loadView() {
    GKeyFile* kf = g_key_file_new();
    const auto file = Util::getConfigFile("ai-terminal-view.ini");
    if (g_key_file_load_from_file(kf, file.string().c_str(), G_KEY_FILE_NONE, nullptr)) {
        GError* err = nullptr;
        const gboolean f = g_key_file_get_boolean(kf, "terminal", "floating", &err);
        if (!err) {
            floating = f;
        }
        g_clear_error(&err);
        const int o = g_key_file_get_integer(kf, "terminal", "opacity", &err);
        if (!err) {
            opacity = std::clamp(o / 100.0, MIN_OPACITY, 1.0);
        }
        g_clear_error(&err);
        const gboolean a = g_key_file_get_boolean(kf, "terminal", "auto_colors", &err);
        if (!err) {
            autoColors = a;
        }
        g_clear_error(&err);
        const int h = g_key_file_get_integer(kf, "terminal", "height", &err);
        if (!err && h >= 80) {
            lastHeight = h;
        }
        g_clear_error(&err);
    }
    g_key_file_free(kf);
    syncing = true;
    if (opacityScale) {
        gtk_range_set_value(GTK_RANGE(opacityScale), opacity * 100);
    }
    if (autoButton) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoButton), autoColors);
    }
    syncing = false;
    if (opacityLabel) {
        gtk_label_set_text(GTK_LABEL(opacityLabel), (std::to_string(static_cast<int>(opacity * 100 + 0.5)) + "%").c_str());
    }
    loaded = true;
}

void TerminalDock::saveView() const {
    if (!loaded) {
        return;
    }
    GKeyFile* kf = g_key_file_new();
    g_key_file_set_boolean(kf, "terminal", "floating", floating);
    g_key_file_set_integer(kf, "terminal", "opacity", static_cast<int>(opacity * 100 + 0.5));
    g_key_file_set_integer(kf, "terminal", "height", lastHeight);
    g_key_file_set_boolean(kf, "terminal", "auto_colors", autoColors);
    g_key_file_save_to_file(kf, Util::getConfigFile("ai-terminal-view.ini").string().c_str(), nullptr);
    g_key_file_free(kf);
}

void TerminalDock::setNewTabChoices(std::vector<TerminalSpec> list) {
    choices = std::move(list);
    rebuildMenu();
}

void TerminalDock::setCustomCommands(std::filesystem::path file, std::vector<std::string> env) {
    commandsFile = std::move(file);
    customEnv = std::move(env);
    rebuildMenu();
}

namespace {
std::string shorten(const std::string& s, size_t n) {
    if (g_utf8_strlen(s.c_str(), -1) <= static_cast<glong>(n)) {
        return s;
    }
    gchar* cut = g_utf8_substring(s.c_str(), 0, static_cast<glong>(n) - 1);
    std::string out = std::string(cut) + "…";
    g_free(cut);
    return out;
}

std::string savedLabel(const SavedCommand& c) {
    const std::string title = terminal_commands::tabTitle(c);
    return c.command.empty() || c.command == title ? title : title + "  ·  " + shorten(c.command, 48);
}
}  // namespace

void TerminalDock::rebuildMenu() {
    if (!newTabMenu) {
        return;
    }
    GList* old = gtk_container_get_children(GTK_CONTAINER(newTabMenu));
    for (GList* l = old; l; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(old);
    for (size_t i = 0; i < choices.size(); i++) {
        GtkWidget* item = gtk_menu_item_new_with_label(choices[i].title.c_str());
        g_object_set_data(G_OBJECT(item), "choice", GSIZE_TO_POINTER(i));
        g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer d) {
                             auto* self = static_cast<TerminalDock*>(d);
                             const auto idx = GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(mi), "choice"));
                             if (idx < self->choices.size()) {
                                 self->openTab(self->choices[idx], true);
                             }
                         }),
                         this);
        gtk_menu_shell_append(GTK_MENU_SHELL(newTabMenu), item);
    }
    if (!commandsFile.empty()) {
        const auto saved = terminal_commands::load(commandsFile);
        if (!saved.empty()) {
            gtk_menu_shell_append(GTK_MENU_SHELL(newTabMenu), gtk_separator_menu_item_new());
        }
        for (size_t i = 0; i < saved.size(); i++) {
            GtkWidget* item = gtk_menu_item_new_with_label(savedLabel(saved[i]).c_str());
            gtk_widget_set_tooltip_text(item, saved[i].command.c_str());
            g_object_set_data(G_OBJECT(item), "saved", GSIZE_TO_POINTER(i));
            g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem* mi, gpointer d) {
                                 auto* self = static_cast<TerminalDock*>(d);
                                 const auto idx = GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(mi), "saved"));
                                 const auto list = terminal_commands::load(self->commandsFile);
                                 if (idx < list.size()) {
                                     self->openCustom(list[idx]);
                                 }
                             }),
                             this);
            gtk_menu_shell_append(GTK_MENU_SHELL(newTabMenu), item);
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(newTabMenu), gtk_separator_menu_item_new());
        GtkWidget* custom = gtk_menu_item_new_with_label("Custom command…");
        gtk_buildable_set_name(GTK_BUILDABLE(custom), "aiTerminalCustomItem");
        g_signal_connect(custom, "activate", G_CALLBACK(+[](GtkMenuItem*, gpointer d) {
                             // after the menu has closed: the popover points at the + button
                             g_idle_add(
                                     [](gpointer d) -> gboolean {
                                         static_cast<TerminalDock*>(d)->promptCustomCommand();
                                         return G_SOURCE_REMOVE;
                                     },
                                     d);
                         }),
                         this);
        gtk_menu_shell_append(GTK_MENU_SHELL(newTabMenu), custom);
    }
    gtk_widget_show_all(newTabMenu);
}

int TerminalDock::openCustom(const SavedCommand& cmd) {
    if (!commandsFile.empty()) {
        terminal_commands::save(commandsFile, terminal_commands::remember(terminal_commands::load(commandsFile), cmd));
        rebuildMenu();
    }
    return openTab(
            {terminal_commands::tabTitle(cmd), cmd.command, terminal_commands::expandFolder(cmd.folder), customEnv},
            true);
}

void TerminalDock::fillSavedList(GtkWidget* list) {
    GList* old = gtk_container_get_children(GTK_CONTAINER(list));
    for (GList* l = old; l; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(old);
    const auto saved = terminal_commands::load(commandsFile);
    if (saved.empty()) {
        return;
    }
    GtkWidget* heading = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(heading), "<small>Recent — tap to open again</small>");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(list), heading, FALSE, FALSE, 0);
    for (size_t i = 0; i < saved.size(); i++) {
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        GtkWidget* open = gtk_button_new_with_label(savedLabel(saved[i]).c_str());
        gtk_button_set_relief(GTK_BUTTON(open), GTK_RELIEF_NONE);
        gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(open)), GTK_ALIGN_START);
        gtk_widget_set_tooltip_text(open, saved[i].command.empty() ? "A shell" : saved[i].command.c_str());
        gtk_buildable_set_name(GTK_BUILDABLE(open), "aiTerminalSaved");
        GtkWidget* forget = gtk_button_new_from_icon_name("edit-delete-symbolic", GTK_ICON_SIZE_MENU);
        gtk_button_set_relief(GTK_BUTTON(forget), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(forget, "Forget this command");
        gtk_buildable_set_name(GTK_BUILDABLE(forget), "aiTerminalForget");
        for (GtkWidget* b: {open, forget}) {
            g_object_set_data(G_OBJECT(b), "saved", GSIZE_TO_POINTER(i));
            g_object_set_data(G_OBJECT(b), "list", list);
        }
        g_signal_connect(open, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             auto* self = static_cast<TerminalDock*>(d);
                             const auto idx = GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(b), "saved"));
                             const auto all = terminal_commands::load(self->commandsFile);
                             if (GtkWidget* pop = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_POPOVER)) {
                                 gtk_popover_popdown(GTK_POPOVER(pop));
                             }
                             if (idx < all.size()) {
                                 self->openCustom(all[idx]);
                             }
                         }),
                         this);
        g_signal_connect(forget, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             auto* self = static_cast<TerminalDock*>(d);
                             const auto idx = GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(b), "saved"));
                             auto* box = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(b), "list"));
                             auto all = terminal_commands::load(self->commandsFile);
                             if (idx < all.size()) {
                                 terminal_commands::save(self->commandsFile, terminal_commands::forget(all, all[idx]));
                             }
                             self->rebuildMenu();
                             // rebuilt after this click has finished (the button is part of the list)
                             g_idle_add(
                                     [](gpointer p) -> gboolean {
                                         auto* pair = static_cast<std::pair<TerminalDock*, GtkWidget*>*>(p);
                                         if (GTK_IS_WIDGET(pair->second)) {
                                             pair->first->fillSavedList(pair->second);
                                         }
                                         g_object_unref(pair->second);
                                         delete pair;
                                         return G_SOURCE_REMOVE;
                                     },
                                     new std::pair<TerminalDock*, GtkWidget*>(self, GTK_WIDGET(g_object_ref(box))));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(row), open, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(row), forget, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(list), row, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(list);
}

void TerminalDock::promptCustomCommand() {
    if (!plusButton || !dock) {
        return;
    }
    show();  // the popover points at the + button in the dock's header
    GtkWidget* popover = gtk_popover_new(plusButton);
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "aiTerminalCustomPopover");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    g_object_set(box, "margin", 10, nullptr);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    auto field = [&](int row, const char* label, const char* name, const char* placeholder, int width) {
        GtkWidget* l = gtk_label_new(label);
        gtk_widget_set_halign(l, GTK_ALIGN_END);
        GtkWidget* e = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(e), width);
        gtk_entry_set_placeholder_text(GTK_ENTRY(e), placeholder);
        gtk_buildable_set_name(GTK_BUILDABLE(e), name);
        gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), e, 1, row, 1, 1);
        return e;
    };
    GtkWidget* command = field(0, "Command", "aiTerminalCustomCommand",
                               "paste or type, e.g. opencode --config ~/work.json (empty: a shell)", 56);
    GtkWidget* title = field(1, "Tab name", "aiTerminalCustomTitle", "optional", 24);
    GtkWidget* folder = field(2, "Folder", "aiTerminalCustomFolder", "optional, e.g. ~/projects/x (default: home)", 40);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* hint = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(hint), "<small>Runs in your login shell; remembered for next time</small>");
    gtk_box_pack_start(GTK_BOX(buttons), hint, FALSE, FALSE, 0);
    GtkWidget* run = gtk_button_new_with_label("Open tab");
    gtk_style_context_add_class(gtk_widget_get_style_context(run), "suggested-action");
    gtk_buildable_set_name(GTK_BUILDABLE(run), "aiTerminalCustomOpen");
    gtk_box_pack_end(GTK_BOX(buttons), run, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), buttons, FALSE, FALSE, 0);

    GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), list, FALSE, FALSE, 0);
    fillSavedList(list);

    g_object_set_data(G_OBJECT(popover), "command", command);
    g_object_set_data(G_OBJECT(popover), "title", title);
    g_object_set_data(G_OBJECT(popover), "folder", folder);
    auto submit = +[](GtkWidget* w, gpointer d) {
        auto* self = static_cast<TerminalDock*>(d);
        GtkWidget* pop = gtk_widget_get_ancestor(w, GTK_TYPE_POPOVER);
        if (!pop) {
            return;
        }
        auto text = [&](const char* key) {
            std::string s = gtk_entry_get_text(GTK_ENTRY(g_object_get_data(G_OBJECT(pop), key)));
            const auto b = s.find_first_not_of(" \t\r\n");
            const auto e = s.find_last_not_of(" \t\r\n");
            return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
        };
        const SavedCommand cmd{text("title"), text("command"), text("folder")};
        gtk_popover_popdown(GTK_POPOVER(pop));
        self->openCustom(cmd);
    };
    for (GtkWidget* e: {command, title, folder}) {
        g_signal_connect(e, "activate", G_CALLBACK(submit), this);
    }
    g_signal_connect(run, "clicked", G_CALLBACK(submit), this);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
                         g_idle_add(
                                 [](gpointer w) -> gboolean {
                                     if (GTK_IS_WIDGET(w)) {
                                         gtk_widget_destroy(GTK_WIDGET(w));
                                     }
                                     g_object_unref(w);
                                     return G_SOURCE_REMOVE;
                                 },
                                 g_object_ref(p));
                     }),
                     nullptr);
    gtk_container_add(GTK_CONTAINER(popover), box);
    gtk_widget_show_all(popover);
    gtk_popover_popup(GTK_POPOVER(popover));
    gtk_widget_grab_focus(command);
}

int TerminalDock::openTab(const TerminalSpec& spec, bool focus) {
    if (!notebook) {
        return -1;
    }
    auto* tab = new Tab;
    tab->owner = this;
    tab->spec = spec;
    GtkWidget* term = vte_terminal_new();
    tab->term = VTE_TERMINAL(term);
    vte_terminal_set_scrollback_lines(tab->term, 10000);
    vte_terminal_set_mouse_autohide(tab->term, TRUE);
    applyColors(tab);
    gtk_widget_set_vexpand(term, TRUE);
    gtk_widget_set_hexpand(term, TRUE);

    GtkWidget* page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(page), "xoai-term-page");
    gtk_box_pack_start(GTK_BOX(page), term, TRUE, TRUE, 0);
    GtkWidget* scrollbar =
            gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(term)));
    gtk_box_pack_end(GTK_BOX(page), scrollbar, FALSE, FALSE, 0);
    tab->page = page;
    g_object_add_weak_pointer(G_OBJECT(page), reinterpret_cast<gpointer*>(&tab->page));

    // The tab's label, with its close button
    GtkWidget* labelBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    tab->label = gtk_label_new(spec.title.c_str());
    gtk_box_pack_start(GTK_BOX(labelBox), tab->label, FALSE, FALSE, 0);
    GtkWidget* close = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(close, FALSE);
    gtk_widget_set_tooltip_text(close, "Close this tab (ends its program)");
    gtk_buildable_set_name(GTK_BUILDABLE(close), "aiTerminalCloseTab");
    g_signal_connect(close, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                         auto* tb = static_cast<Tab*>(d);
                         TerminalDock* owner = tb->owner;
                         for (size_t i = 0; i < owner->tabs.size(); i++) {
                             if (owner->tabs[i] == tb) {
                                 owner->closeTab(static_cast<int>(i));
                                 return;
                             }
                         }
                     }),
                     tab);
    gtk_box_pack_start(GTK_BOX(labelBox), close, FALSE, FALSE, 0);
    gtk_widget_show_all(labelBox);
    const int index = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, labelBox);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), page, TRUE);
    tabs.push_back(tab);

    // Ctrl+` hides the dock even while the terminal has the keyboard
    g_signal_connect(term, "key-press-event", G_CALLBACK(+[](GtkWidget*, GdkEventKey* e, gpointer d) -> gboolean {
                         auto* self = static_cast<TerminalDock*>(d);
                         if (isToggleKey(e)) {
                             self->hide();
                             return TRUE;
                         }
                         self->lastInput = g_get_monotonic_time();
                         return FALSE;
                     }),
                     this);
    g_signal_connect(
            term, "child-exited", G_CALLBACK(+[](VteTerminal* t, int status, gpointer d) {
                auto* tb = static_cast<Tab*>(d);
                tb->running = false;
                tb->pid = -1;
                const std::string msg =
                        "\r\n\x1b[2m[" + tb->spec.title +
                        " exited. Restart it with the ⟳ button above, or close the tab with its ×.]\x1b[0m\r\n";
                vte_terminal_feed(t, msg.c_str(), static_cast<gssize>(msg.size()));
                TerminalDock* owner = tb->owner;
                for (size_t i = 0; i < owner->tabs.size(); i++) {
                    if (owner->tabs[i] == tb && owner->onExit) {
                        owner->onExit(static_cast<int>(i), status);
                    }
                }
            }),
            tab);

    gtk_widget_show_all(page);
    spawn(tab);
    if (focus) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), index);  // (a notebook position, not our index)
        show();
        gtk_widget_grab_focus(term);
    }
    return static_cast<int>(tabs.size()) - 1;
}

void TerminalDock::spawn(Tab* tab) {
    const std::string shell = loginShell();
    std::vector<std::string> args = {shell, "-l"};
    if (!tab->spec.command.empty()) {
        // -i so that ~/.bashrc (nvm, PATH additions) is read, like in a normal terminal
        const std::string& cmd = tab->spec.command;
        const bool compound = cmd.find("||") != std::string::npos || cmd.find("&&") != std::string::npos ||
                              cmd.find(';') != std::string::npos;
        args = {shell, "-l", "-i", "-c", compound ? cmd : "exec " + cmd};
    }
    std::vector<char*> argv;
    for (auto& a: args) {
        argv.push_back(a.data());
    }
    argv.push_back(nullptr);

    gchar** env = g_get_environ();
    if (const char* im = gui::screen_keyboard::sessionInputMethod()) {
        env = g_environ_setenv(env, "GTK_IM_MODULE", im, FALSE);  // (the app took it out for itself)
    }
    for (const auto& kv: tab->spec.env) {
        const auto eq = kv.find('=');
        if (eq != std::string::npos) {
            env = g_environ_setenv(env, kv.substr(0, eq).c_str(), kv.substr(eq + 1).c_str(), TRUE);
        }
    }
    const std::string cwd = tab->spec.workingDirectory.empty() ? g_get_home_dir() : tab->spec.workingDirectory;
    vte_terminal_spawn_async(
            tab->term, VTE_PTY_DEFAULT, cwd.c_str(), argv.data(), env, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, -1,
            nullptr,
            +[](VteTerminal* t, GPid pid, GError* error, gpointer d) {
                auto* tb = static_cast<Tab*>(d);
                if (error) {
                    const std::string msg = std::string("\r\n[could not start: ") + error->message + "]\r\n";
                    vte_terminal_feed(t, msg.c_str(), static_cast<gssize>(msg.size()));
                    tb->running = false;
                    return;
                }
                tb->pid = pid;
                tb->running = true;
            },
            tab);
    g_strfreev(env);
}

TerminalDock::Tab* TerminalDock::tabAt(int index) const {
    return index >= 0 && static_cast<size_t>(index) < tabs.size() && tabs[static_cast<size_t>(index)]->page ?
                   tabs[static_cast<size_t>(index)] :
                   nullptr;
}

void TerminalDock::restartTab(int index) {
    Tab* tab = tabAt(index);
    if (!tab) {
        return;
    }
    if (tab->running && tab->pid > 0) {
        kill(tab->pid, SIGHUP);
    }
    vte_terminal_reset(tab->term, TRUE, TRUE);
    spawn(tab);
}

void TerminalDock::closeTab(int index) {
    Tab* tab = tabAt(index);
    if (!tab || !notebook) {
        return;
    }
    const bool wasRunning = tab->running;
    g_signal_handlers_disconnect_by_data(tab->term, tab);  // no "exited" message into a terminal on its way out
    if (tab->running && tab->pid > 0) {
        kill(tab->pid, SIGHUP);  // like closing a terminal window
    }
    tab->running = false;
    tab->pid = -1;
    const int num = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab->page);
    if (num >= 0) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(notebook), num);  // destroys the page: tab->page becomes null
    }
    // (the Tab stays in the list, empty, so that the other tabs keep their indices)
    if (wasRunning && onExit) {
        onExit(index, -1);
    }
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) == 0) {
        hide();
    }
}

int TerminalDock::currentTab() const {
    if (!notebook) {
        return -1;
    }
    GtkWidget* page =
            gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook)));
    for (size_t i = 0; page && i < tabs.size(); i++) {
        if (tabs[i]->page == page) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int TerminalDock::openTabCount() const { return notebook ? gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook)) : 0; }

int TerminalDock::tabCount() const { return static_cast<int>(tabs.size()); }

int TerminalDock::findTab(const std::string& title) const {
    for (size_t i = 0; i < tabs.size(); i++) {
        if (tabs[i]->page && tabs[i]->spec.title == title) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool TerminalDock::isRunning(int index) const {
    Tab* tab = tabAt(index);
    return tab && tab->running;
}

bool TerminalDock::feed(int index, const std::string& text) {
    Tab* tab = tabAt(index);
    if (!tab || !tab->running) {
        return false;
    }
    vte_terminal_feed_child(tab->term, text.c_str(), static_cast<gssize>(text.size()));
    return true;
}

std::string TerminalDock::text(int index) const {
    Tab* tab = tabAt(index);
    if (!tab) {
        return {};
    }
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    char* t = vte_terminal_get_text(tab->term, nullptr, nullptr, nullptr);
    G_GNUC_END_IGNORE_DEPRECATIONS
    std::string out = t ? t : "";
    g_free(t);
    return out;
}

void TerminalDock::show() {
    if (!dock || !paned) {
        return;
    }
    gtk_widget_show(dock);
    const int total = gtk_widget_get_allocated_height(paned);
    if (floating) {
        gtk_widget_queue_resize(dock);
    } else if (total > lastHeight + 100) {
        gtk_paned_set_position(GTK_PANED(paned), total - lastHeight);
    }
    if (notebook) {
        if (Tab* tab = tabAt(currentTab())) {
            gtk_widget_grab_focus(GTK_WIDGET(tab->term));
        }
    }
}

void TerminalDock::hide() {
    if (!dock) {
        return;
    }
    if (gtk_widget_get_visible(dock)) {
        const int h = gtk_widget_get_allocated_height(dock);
        if (h > 60) {
            lastHeight = h;
        }
    }
    gtk_widget_hide(dock);  // the processes keep running
}

void TerminalDock::toggle() { isVisible() ? hide() : show(); }

bool TerminalDock::isVisible() const { return dock && gtk_widget_get_visible(dock); }

}  // namespace xoj::assistant
