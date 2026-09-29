#include "McpUi.h"

#include "api/AgentGate.h"                  // for AgentGate
#include "control/Control.h"                // for Control
#include "control/layer/LayerController.h"  // for LayerController
#include "control/zoom/ZoomControl.h"       // for ZoomControl
#include "gui/MainWindow.h"                 // for MainWindow
#include "gui/PageView.h"                   // for XojPageView
#include "gui/XournalView.h"                // for XournalView
#include "gui/widgets/XournalWidget.h"      // for GtkXournal
#include "model/Document.h"                 // for Document
#include "model/Layer.h"                    // for Layer
#include "model/XojPage.h"                  // for XojPage

#include "McpConfig.h"
#include "McpHttpServer.h"
#include "McpServer.h"
#include "NotesStore.h"
#include "PathText.h"

namespace xoj::mcp {

namespace {
constexpr const char* AI_LAYER_HINT = "AI";
constexpr const char* MENU_LABEL = "_AI Agent";

/// The agent's own layer the AI Agent menu acts on: the configured default layer if it is a named layer, else "AI"
std::string agentLayerName(const McpConfig& cfg) {
    const std::string& l = cfg.defaultLayer;
    return l.empty() || l == "current" || l.rfind('#', 0) == 0 ? std::string("AI") : l;
}

/// Index (1-based) of the layer called `name` on a page, or 0
size_t aiLayerIndex(const PageRef& page, const std::string& name) {
    const auto& layers = page->getLayers();
    for (size_t i = 0; i < layers.size(); i++) {
        if (layers[i]->hasName() && layers[i]->getName() == name) {
            return i + 1;
        }
    }
    return 0;
}
}  // namespace

McpUi::McpUi(McpServer& server): server(server) {
    installActions();
    // The main menu is populated after the server starts (Control::initWindow runs before MainWindow::populate)
    // (high priority: before the first agent request is served)
    menuIdle = g_idle_add_full(
            G_PRIORITY_HIGH,
            [](gpointer self) -> gboolean {
                auto* ui = static_cast<McpUi*>(self);
                ui->menuIdle = 0;
                ui->buildMenu();
                return G_SOURCE_REMOVE;
            },
            this, nullptr);
    buildStrip();
    timer = g_timeout_add_seconds(2, &McpUi::onTick, this);
    update();
}

McpUi::~McpUi() {
    if (timer) {
        g_source_remove(timer);
    }
    if (menuIdle) {
        g_source_remove(menuIdle);
    }
    // Only what still exists: when the application quits, the main window (and all of this) is already gone
    if (settings) {
        GtkWidget* s = settings;
        unwatch(settings);
        gtk_widget_destroy(s);
    }
    removeMenu();
    unwatch(label);
    unwatch(pauseButton);
    if (strip) {
        GtkWidget* s = strip;
        unwatch(strip);
        gtk_widget_destroy(s);
    }
    if (window) {
        for (const char* name:
             {"mcp-paused", "mcp-ai-accept", "mcp-ai-clear", "mcp-ai-toggle", "mcp-copy-command", "mcp-settings"}) {
            g_action_map_remove_action(G_ACTION_MAP(window), name);
        }
        unwatch(window);
    }
    api::AgentGate::setPaused(false);
}

gboolean McpUi::onTick(gpointer self) {
    auto* ui = static_cast<McpUi*>(self);
    if (ui->server.getNotes()) {
        ui->server.getNotes()->sync();  // notes of an untitled document are written once the user saves it
    }
    ui->update();
    return G_SOURCE_CONTINUE;
}

void McpUi::installActions() {
    Control* ctrl = server.getControl();
    GtkWidget* win = ctrl->getWindow()->getWindow();
    window = win;
    watch(window);
    auto add = [&](GSimpleAction* a) {
        g_action_map_add_action(G_ACTION_MAP(win), G_ACTION(a));
        g_object_unref(a);
    };
    GSimpleAction* pause = g_simple_action_new_stateful("mcp-paused", nullptr, g_variant_new_boolean(false));
    g_signal_connect(pause, "change-state", G_CALLBACK(+[](GSimpleAction* a, GVariant* v, gpointer self) {
                         g_simple_action_set_state(a, v);
                         api::AgentGate::setPaused(g_variant_get_boolean(v));
                         static_cast<McpUi*>(self)->update();
                     }),
                     this);
    add(pause);

    struct Simple {
        const char* name;
        void (*fn)(McpUi*);
    };
    for (Simple s: {Simple{"mcp-ai-accept", [](McpUi* ui) { ui->clearLayer(true); }},
                    Simple{"mcp-ai-clear", [](McpUi* ui) { ui->clearLayer(false); }},
                    Simple{"mcp-ai-toggle",
                           [](McpUi* ui) {
                               Control* c = ui->server.getControl();
                               PageRef page = c->getCurrentPage();
                               const size_t idx = aiLayerIndex(page, agentLayerName(ui->server.getConfig()));
                               if (idx) {
                                   const auto layer = static_cast<Layer::Index>(idx);
                                   c->getLayerController()->setLayerVisible(layer, !page->isLayerVisible(layer));
                               }
                           }},
                    Simple{"mcp-settings", [](McpUi* ui) { ui->showSettings(); }},
                    Simple{"mcp-copy-command", [](McpUi* ui) {
                               const McpConfig& cfg = ui->server.getConfig();
                               const std::string cmd = "claude mcp add --transport http xournalai " + cfg.url() +
                                                       " --header \"Authorization: Bearer " + cfg.token + "\"";
                               gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), cmd.c_str(), -1);
                           }}}) {
        using Binding = std::pair<McpUi*, void (*)(McpUi*)>;
        GSimpleAction* a = g_simple_action_new(s.name, nullptr);
        g_signal_connect_data(
                a, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer data) {
                    auto* b = static_cast<Binding*>(data);
                    b->second(b->first);
                }),
                new Binding(this, s.fn), +[](gpointer data, GClosure*) { delete static_cast<Binding*>(data); },
                GConnectFlags(0));
        add(a);
    }
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(win));
    const char* pauseAccel[] = {"<Ctrl><Alt>Escape", nullptr};
    gtk_application_set_accels_for_action(app, "win.mcp-paused", pauseAccel);
}

void McpUi::clearLayer(bool merge) {
    Control* ctrl = server.getControl();
    PageRef page = ctrl->getCurrentPage();
    const size_t idx = aiLayerIndex(page, agentLayerName(server.getConfig()));
    if (!idx) {
        return;
    }
    const auto selected = page->getSelectedLayerId();
    LayerController* lc = ctrl->getLayerController();
    lc->switchToLay(static_cast<Layer::Index>(idx), false, true);
    if (merge && idx > 1) {
        lc->mergeCurrentLayerDown();
    } else if (!merge && page->getLayerCount() > 1) {
        lc->deleteCurrentLayer();
    }
    lc->switchToLay(std::min<Layer::Index>(selected, page->getLayerCount()), false, false);
}

/// The "AI Agent" main menu, inserted before "Help" (not in mainmenubar.xml: it only exists with the MCP server)
void McpUi::buildMenu() {
    if (!window) {
        return;
    }
    GMenuModel* model = server.getControl()->getWindow()->getMenuModel();
    if (!model || !G_IS_MENU(model)) {
        return;
    }
    menubar = model;
    watch(menubar);
    GMenu* sub = g_menu_new();
    auto section = [&](std::initializer_list<std::pair<const char*, const char*>> items, const char* accel = nullptr) {
        GMenu* s = g_menu_new();
        for (const auto& [label, action]: items) {
            GMenuItem* item = g_menu_item_new(label, action);
            if (accel) {
                g_menu_item_set_attribute(item, "accel", "s", accel);
            }
            g_menu_append_item(s, item);
            g_object_unref(item);
        }
        g_menu_append_section(sub, nullptr, G_MENU_MODEL(s));
        g_object_unref(s);
    };
    section({{"Pause AI agent", "win.mcp-paused"}}, "<Ctrl><Alt>Escape");
    section({{"Accept AI layer (merge down)", "win.mcp-ai-accept"},
             {"Show or hide AI layer", "win.mcp-ai-toggle"},
             {"Clear AI layer", "win.mcp-ai-clear"}});
    section({{"Copy agent connect command", "win.mcp-copy-command"}, {"AI Agent _Settings…", "win.mcp-settings"}});
    const int n = g_menu_model_get_n_items(model);
    g_menu_insert_submenu(G_MENU(model), std::max(0, n - 1), MENU_LABEL, G_MENU_MODEL(sub));
    g_object_unref(sub);
}

void McpUi::removeMenu() {
    GMenuModel* model = menubar;
    if (!model || !G_IS_MENU(model)) {
        return;
    }
    unwatch(menubar);
    for (int i = g_menu_model_get_n_items(model) - 1; i >= 0; i--) {
        gchar* label = nullptr;
        if (g_menu_model_get_item_attribute(model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label)) {
            const bool ours = std::string(label) == MENU_LABEL;
            g_free(label);
            if (ours) {
                g_menu_remove(G_MENU(model), i);
            }
        }
    }
}

void McpUi::buildStrip() {
    GtkWidget* mainBox = server.getControl()->getWindow()->get("mainBox");
    if (!mainBox || !GTK_IS_BOX(mainBox)) {
        return;
    }
    strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_buildable_set_name(GTK_BUILDABLE(strip), "mcpStatusStrip");
    g_object_set(strip, "margin-start", 8, "margin-end", 8, "margin-top", 1, "margin-bottom", 1, nullptr);
    label = gtk_label_new("");
    gtk_buildable_set_name(GTK_BUILDABLE(label), "mcpStatus");
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    pauseButton = gtk_toggle_button_new_with_label("Pause AI");
    gtk_buildable_set_name(GTK_BUILDABLE(pauseButton), "mcpPause");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(pauseButton), "win.mcp-paused");
    gtk_widget_set_tooltip_text(pauseButton, "Stop the AI agent immediately (Ctrl+Alt+Esc)");
    gtk_box_pack_start(GTK_BOX(strip), label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(strip), pauseButton, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(mainBox), strip, FALSE, FALSE, 0);
    gtk_widget_show_all(strip);
    watch(strip);
    watch(label);
    watch(pauseButton);
}

void McpUi::toolStarted(const std::string& name) {
    currentTool = name;
    running++;
    update();
}

void McpUi::toolFinished() {
    if (running > 0) {
        running--;
    }
    if (running == 0) {
        currentTool.clear();
    }
    update();
}

void McpUi::update() {
    if (!label) {
        return;
    }
    const McpConfig& cfg = server.getConfig();
    std::string text;
    auto* http = server.getHttpServer();
    if (!http || !http->isListening()) {
        text = "AI: off";
    } else if (api::AgentGate::paused()) {
        text = "AI: paused by you - agents cannot change anything";
    } else {
        const size_t sessions = http->sessionCount();
        text = "AI: " + (sessions == 0 ? std::string("listening on 127.0.0.1:") + std::to_string(cfg.port) :
                                         std::to_string(sessions) + " agent session" + (sessions == 1 ? "" : "s"));
        if (!currentTool.empty()) {
            text += "  |  working: " + currentTool;
        }
    }
    if (text != lastText) {
        gtk_label_set_text(GTK_LABEL(label), text.c_str());
        lastText = text;
    }
}

void McpUi::flash(size_t page, const xoj::util::Rectangle<double>& area) {
    if (!window) {
        return;
    }
    Control* ctrl = server.getControl();
    XournalView* xv = ctrl->getWindow()->getXournal();
    XojPageView* view = xv->getViewFor(page);
    if (!view) {
        return;
    }
    GtkWidget* widget = xv->getWidget();
    GtkXournal* xw = GTK_XOURNAL(widget);
    const double zoom = ctrl->getZoomControl()->getZoom();
    const auto pos = view->getPixelPosition();
    GdkRectangle target;
    target.x = static_cast<int>(pos.x + area.x * zoom - gtk_adjustment_get_value(xw->hadjustment));
    target.y = static_cast<int>(pos.y + area.y * zoom - gtk_adjustment_get_value(xw->vadjustment));
    target.width = std::max(1, static_cast<int>(area.width * zoom));
    target.height = std::max(1, static_cast<int>(area.height * zoom));
    // Only when the area is on screen
    if (target.x + target.width < 0 || target.y + target.height < 0 ||
        target.x > gtk_widget_get_allocated_width(widget) || target.y > gtk_widget_get_allocated_height(widget)) {
        return;
    }
    GtkWidget* popover = gtk_popover_new(widget);
    gtk_popover_set_modal(GTK_POPOVER(popover), FALSE);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &target);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_TOP);
    GtkWidget* l = gtk_label_new(AI_LAYER_HINT);
    g_object_set(l, "margin", 2, nullptr);
    gtk_container_add(GTK_CONTAINER(popover), l);
    gtk_widget_show_all(popover);
    g_object_ref(popover);
    g_timeout_add(
            1200,
            [](gpointer p) -> gboolean {
                gtk_widget_destroy(GTK_WIDGET(p));
                g_object_unref(p);
                return G_SOURCE_REMOVE;
            },
            popover);
}

namespace {
GtkWidget* named(GtkWidget* w, const char* name) {
    gtk_buildable_set_name(GTK_BUILDABLE(w), name);
    return w;
}

GtkWidget* find(GtkWidget* root, const std::string& name) {
    if (GTK_IS_BUILDABLE(root)) {
        const char* n = gtk_buildable_get_name(GTK_BUILDABLE(root));
        if (n && name == n) {
            return root;
        }
    }
    GtkWidget* found = nullptr;
    if (GTK_IS_CONTAINER(root)) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(root));
        for (GList* c = children; c && !found; c = c->next) {
            found = find(GTK_WIDGET(c->data), name);
        }
        g_list_free(children);
    }
    return found;
}

bool checked(GtkWidget* dialog, const std::string& name) {
    return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(find(dialog, name)));
}
}  // namespace

void McpUi::showSettings() {
    if (settings) {
        gtk_window_present(GTK_WINDOW(settings));
        return;
    }
    if (!window) {
        return;
    }
    const McpConfig cfg = McpConfig::load();  // the file, not this run's command line overrides
    GtkWindow* parent = GTK_WINDOW(server.getControl()->getWindow()->getWindow());
    settings = gtk_dialog_new_with_buttons("AI Agent Settings", parent, GTK_DIALOG_DESTROY_WITH_PARENT, "_Cancel",
                                           GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_OK, nullptr);
    gtk_buildable_set_name(GTK_BUILDABLE(settings), api::AgentGate::SETTINGS_DIALOG);
    watch(settings);
    gtk_dialog_set_default_response(GTK_DIALOG(settings), GTK_RESPONSE_OK);

    GtkWidget* grid = gtk_grid_new();
    g_object_set(grid, "margin", 12, "row-spacing", 6, "column-spacing", 12, nullptr);
    int row = 0;
    auto heading = [&](const char* text) {
        GtkWidget* l = gtk_label_new(nullptr);
        gchar* markup = g_markup_printf_escaped("<b>%s</b>", text);
        gtk_label_set_markup(GTK_LABEL(l), markup);
        g_free(markup);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_widget_set_margin_top(l, row == 0 ? 0 : 8);
        gtk_grid_attach(GTK_GRID(grid), l, 0, row++, 3, 1);
    };
    auto check = [&](const char* name, const char* label, bool value, const char* hint) {
        GtkWidget* c = named(gtk_check_button_new_with_mnemonic(label), name);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c), value);
        gtk_widget_set_tooltip_text(c, hint);
        gtk_grid_attach(GTK_GRID(grid), c, 0, row++, 3, 1);
    };
    auto labelled = [&](const char* label, GtkWidget* w) {
        GtkWidget* l = gtk_label_new_with_mnemonic(label);
        gtk_label_set_xalign(GTK_LABEL(l), 0);
        gtk_label_set_mnemonic_widget(GTK_LABEL(l), w);
        gtk_grid_attach(GTK_GRID(grid), l, 0, row, 1, 1);
        gtk_widget_set_hexpand(w, TRUE);
        gtk_grid_attach(GTK_GRID(grid), w, 1, row++, 1, 1);
    };

    heading("Server");
    check("mcpEnabled", "_Let AI agents connect (MCP server on 127.0.0.1)", cfg.enabled,
          "Any MCP client (Claude Code, Gemini CLI, Codex, OpenCode, ...) can use xournalai as a tool");
    GtkWidget* port = named(gtk_spin_button_new_with_range(1024, 65535, 1), "mcpPort");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(port), cfg.port);
    labelled("_Port", port);
    GtkWidget* token = named(gtk_entry_new(), "mcpToken");
    gtk_entry_set_text(GTK_ENTRY(token), cfg.token.c_str());
    gtk_editable_set_editable(GTK_EDITABLE(token), FALSE);
    gtk_entry_set_visibility(GTK_ENTRY(token), FALSE);
    labelled("Access _token", token);
    GtkWidget* regenerate = gtk_button_new_with_mnemonic("_New token");
    gtk_widget_set_tooltip_text(regenerate, "Disconnects every agent until it is configured with the new token");
    g_signal_connect(regenerate, "clicked", G_CALLBACK(+[](GtkButton*, gpointer entry) {
                         gtk_entry_set_text(GTK_ENTRY(entry), McpConfig::generateToken().c_str());
                     }),
                     token);
    gtk_grid_attach(GTK_GRID(grid), regenerate, 2, row - 1, 1, 1);

    heading("What agents may do");
    const struct {
        Tier tier;
        const char* name;
        const char* label;
        const char* hint;
    } tiers[] = {{Tier::Read, "mcpTierRead", "_Read and render the document", "Inspect pages, elements and images"},
                 {Tier::Draw, "mcpTierDraw", "_Draw and edit content", "Create, move, restyle and delete elements"},
                 {Tier::Ui, "mcpTierUi", "Control the application (_menus, tools, dialogs)",
                  "Menus, tools, dialogs, view and navigation"},
                 {Tier::Files, "mcpTierFiles", "Open, save, import and _export files", "File operations"},
                 {Tier::Destructive, "mcpTierDestructive", "Destructive operations (discard unsaved work, _overwrite)",
                  "Close without saving, overwrite existing files, quit. Off by default."}};
    for (const auto& t: tiers) {
        check(t.name, t.label, cfg.allows(t.tier), t.hint);
    }

    heading("Defaults");
    GtkWidget* layer = named(gtk_entry_new(), "mcpLayer");
    gtk_entry_set_text(GTK_ENTRY(layer), cfg.defaultLayer.c_str());
    gtk_widget_set_tooltip_text(layer, "\"AI\" (a separate layer you can accept or clear), \"current\" or a layer "
                                       "name");
    labelled("Agent drawings go to la_yer", layer);
    check("mcpAnimate", "_Animate agent drawing", cfg.animate, "Strokes appear gradually, like handwriting");
    check("mcpBackups", "Keep a _backup copy before risky agent operations", cfg.backups,
          "Copies go to the backup folder in xournalpp's data directory");

    GtkWidget* note = gtk_label_new(nullptr);
    std::string noteText = "Settings are stored in " + toUtf8(McpConfig::path()) + ".";
    if (McpConfig::overrides().enabled || McpConfig::overrides().port) {
        noteText += " Command line options (--mcp, --no-mcp, --mcp-port) override them for this run.";
    }
    gtk_label_set_text(GTK_LABEL(note), noteText.c_str());
    gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(note), 60);
    gtk_label_set_xalign(GTK_LABEL(note), 0);
    gtk_widget_set_margin_top(note, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(note), "dim-label");
    gtk_grid_attach(GTK_GRID(grid), note, 0, row++, 3, 1);

    gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(settings))), grid);
    g_signal_connect(settings, "response", G_CALLBACK(+[](GtkDialog* d, gint response, gpointer self) {
                         auto* ui = static_cast<McpUi*>(self);
                         if (response == GTK_RESPONSE_OK) {
                             ui->applySettings(GTK_WIDGET(d));
                         }
                         ui->unwatch(ui->settings);
                         ui->settings = nullptr;
                         gtk_widget_destroy(GTK_WIDGET(d));
                     }),
                     this);
    gtk_widget_show_all(settings);
}

void McpUi::applySettings(GtkWidget* dialog) {
    const McpConfig cfg = McpConfig::load();
    McpConfig next;
    next.exportDir = cfg.exportDir;
    next.backupDir = cfg.backupDir;
    next.enabled = checked(dialog, "mcpEnabled");
    next.port = static_cast<uint16_t>(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(find(dialog, "mcpPort"))));
    next.token = gtk_entry_get_text(GTK_ENTRY(find(dialog, "mcpToken")));
    next.tiers.clear();
    const std::pair<Tier, const char*> tiers[] = {{Tier::Read, "mcpTierRead"},
                                                  {Tier::Draw, "mcpTierDraw"},
                                                  {Tier::Ui, "mcpTierUi"},
                                                  {Tier::Files, "mcpTierFiles"},
                                                  {Tier::Destructive, "mcpTierDestructive"}};
    for (const auto& [tier, name]: tiers) {
        if (checked(dialog, name)) {
            next.tiers.insert(tier);
        }
    }
    const std::string layer = gtk_entry_get_text(GTK_ENTRY(find(dialog, "mcpLayer")));
    next.defaultLayer = layer.empty() ? "current" : layer;
    next.animate = checked(dialog, "mcpAnimate");
    next.backups = checked(dialog, "mcpBackups");
    next.save();
    // Apply now: agents reconnect (same port and token: their next request just starts a new session)
    server.restart();
}

}  // namespace xoj::mcp
