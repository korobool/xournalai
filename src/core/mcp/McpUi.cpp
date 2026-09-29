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

#ifdef ENABLE_AI_TERMINAL
#include "api/EventHub.h"  // for EventHub
#include "assistant/Companion.h"
#include "assistant/EventPump.h"
#include "assistant/terminal/TerminalDock.h"
#endif


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
    server.serving().setListener([this] { update(); });
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
#ifdef ENABLE_AI_TERMINAL
    buildTerminal();
#endif
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
#ifdef ENABLE_AI_TERMINAL
    eventPump.reset();
    dock.reset();
#endif
    unwatch(label);
    unwatch(pauseButton);
    if (strip) {
        GtkWidget* s = strip;
        unwatch(strip);
        gtk_widget_destroy(s);
    }
    if (window) {
        for (const char* name: {"mcp-paused", "mcp-ai-accept", "mcp-ai-clear", "mcp-ai-toggle", "mcp-copy-command",
                                "mcp-settings", "ai-terminal", "ai-terminal-open"}) {
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
                         const bool paused = g_variant_get_boolean(v);
                         if (paused != api::AgentGate::paused()) {
                             g_message("AI agent %s by the user", paused ? "paused" : "resumed");  // for diagnosis
                         }
                         api::AgentGate::setPaused(paused);
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
#ifdef ENABLE_AI_TERMINAL
    GSimpleAction* term = g_simple_action_new("ai-terminal", nullptr);
    g_signal_connect(term, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer self) {
                         auto* ui = static_cast<McpUi*>(self);
                         if (ui->dock) {
                             ui->dock->toggle();
                         }
                     }),
                     this);
    add(term);
    GSimpleAction* open = g_simple_action_new("ai-terminal-open", G_VARIANT_TYPE_STRING);
    g_signal_connect(open, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant* v, gpointer self) {
                         static_cast<McpUi*>(self)->openTerminal(g_variant_get_string(v, nullptr));
                     }),
                     this);
    add(open);
#endif
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(win));
    const char* pauseAccel[] = {"<Ctrl><Alt>Escape", nullptr};
    gtk_application_set_accels_for_action(app, "win.mcp-paused", pauseAccel);
#ifdef ENABLE_AI_TERMINAL
    const char* termAccel[] = {"<Ctrl>grave", nullptr};
    gtk_application_set_accels_for_action(app, "win.ai-terminal", termAccel);
#endif
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
#ifdef ENABLE_AI_TERMINAL
    {
        GMenu* s = g_menu_new();
        GMenuItem* item = g_menu_item_new("Show or hide AI _terminal", "win.ai-terminal");
        g_menu_item_set_attribute(item, "accel", "s", "<Ctrl>grave");
        g_menu_append_item(s, item);
        g_object_unref(item);
        g_menu_append(s, "New Claude Code tab", "win.ai-terminal-open::claude");
        g_menu_append(s, "New Codex tab", "win.ai-terminal-open::codex");
        g_menu_append(s, "New OpenCode tab", "win.ai-terminal-open::opencode");
        g_menu_append(s, "New shell tab", "win.ai-terminal-open::shell");
        g_menu_append_section(sub, nullptr, G_MENU_MODEL(s));
        g_object_unref(s);
    }
#endif
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
    // Not focusable (a stray Space must not toggle it); on the left, away from the zoom controls
    gtk_widget_set_can_focus(pauseButton, FALSE);
    gtk_button_set_relief(GTK_BUTTON(pauseButton), GTK_RELIEF_NORMAL);
    gtk_buildable_set_name(GTK_BUILDABLE(pauseButton), "mcpPause");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(pauseButton), "win.mcp-paused");
    gtk_widget_set_tooltip_text(pauseButton, "Stop the AI agent immediately (Ctrl+Alt+Esc)");
    gtk_box_pack_start(GTK_BOX(strip), pauseButton, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(strip), label, TRUE, TRUE, 0);
    // Paused must be impossible to miss
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
                                    "#mcpStatusStrip.paused { background-color: #b45309; }"
                                    "#mcpStatusStrip.paused > label { color: #ffffff; font-weight: bold; }"
                                    "#mcpStatusStrip.paused > button label { font-weight: bold; }",
                                    -1, nullptr);
    gtk_widget_set_name(strip, "mcpStatusStrip");
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(strip), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
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
    if (!server.waitingReason().empty()) {
        text = "AI: waiting - " + server.waitingReason() + "; agents connect as soon as it is free";
    } else if (!http || !http->isListening()) {
        text = "AI: off";
    } else if (api::AgentGate::paused()) {
        text = "AI PAUSED - agents are blocked; your pen still works (resume: button or Ctrl+Alt+Esc)";
    } else {
        const size_t sessions = http->sessionCount();
        text = "AI: " + (sessions == 0 ? std::string("listening on 127.0.0.1:") + std::to_string(cfg.port) :
                                         std::to_string(sessions) + " agent session" + (sessions == 1 ? "" : "s"));
        if (!currentTool.empty()) {
            text += "  |  working: " + currentTool;
        }
        using S = assistant::ServingState::State;
        const auto& sv = server.serving();
        const std::string who = server.getConfig().assistant.agent == "codex" ? "Codex" : "Claude";
        switch (sv.state()) {
            case S::Idle:
                text += "  |  " + who + ": idle";
                break;
            case S::Busy:
                text += "  |  " + who + ": thinking" + (sv.tool().empty() ? "" : " (" + sv.tool() + ")");
                break;
            case S::Waiting:
                text += "  |  " + who + ": waiting for you in the AI terminal";
                break;
            case S::NotRunning:
                break;
        }
#ifdef ENABLE_AI_TERMINAL
        if (eventPump && !eventPump->status().empty()) {
            text += "  |  " + eventPump->status();
        }
#endif
    }
    if (strip) {
        GtkStyleContext* ctx = gtk_widget_get_style_context(strip);
        if (api::AgentGate::paused()) {
            gtk_style_context_add_class(ctx, "paused");
        } else {
            gtk_style_context_remove_class(ctx, "paused");
        }
    }
    if (pauseButton) {
        gtk_button_set_label(GTK_BUTTON(pauseButton), api::AgentGate::paused() ? "Resume AI" : "Pause AI");
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

#ifdef ENABLE_AI_TERMINAL
namespace {
std::string selfExecutable() {
    std::string exe = "xournalpp";
    if (char* self = g_file_read_link("/proc/self/exe", nullptr)) {
        exe = self;
        g_free(self);
    }
    return exe;
}

/// Whether Claude Code has a stored conversation for `folder` (~/.claude/projects/<folder, non-alphanumerics as ->/)
bool hasClaudeHistory(const std::string& folder) {
    std::string key = folder;
    for (char& c: key) {
        if (!g_ascii_isalnum(c) && c != '-') {
            c = '-';
        }
    }
    const fs::path dir = fs::path(g_get_home_dir()) / ".claude" / "projects" / key;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return false;
    }
    for (const auto& e: fs::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".jsonl") {
            return true;
        }
    }
    return false;
}

/// Single-quotes a string for the shell
std::string shq(const std::string& s) {
    std::string out = "'";
    for (char c: s) {
        out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return out + "'";
}
}  // namespace

assistant::TerminalSpec McpUi::terminalSpec(const std::string& kind, bool serving) const {
    const McpConfig& cfg = server.getConfig();
    const bool bypass = cfg.assistant.permissionMode != "normal";
    const std::string folder = assistant::Companion::folder().string();
    assistant::TerminalSpec spec;
    spec.env = {"XOURNALAI_PORT=" + std::to_string(cfg.port)};
    if (kind == "claude") {
        // Runs in the companion folder: its CLAUDE.md, .mcp.json (this app) and settings; resumes its last session
        std::string command = std::string("claude") + (bypass ? " --dangerously-skip-permissions" : "");
        if (serving && hasClaudeHistory(folder)) {
            command += " --continue";  // resume the serving session's last conversation
        }
        spec = {serving ? "Claude · serving" : "Claude", command, folder, spec.env};
    } else if (kind == "codex") {
        const std::string mcp =
                " -c " + shq("mcp_servers.xournalai.command=\"" + selfExecutable() + "\"") + " -c " +
                shq("mcp_servers.xournalai.args=[\"--mcp-stdio\",\"--mcp-port=" + std::to_string(cfg.port) + "\"]");
        spec = {serving ? "Codex · serving" : "Codex",
                std::string("codex") + (bypass ? " --dangerously-bypass-approvals-and-sandbox" : "") + mcp, folder,
                spec.env};
    } else if (kind == "opencode") {
        spec = {"OpenCode", "opencode", folder, spec.env};
    } else {
        spec = {"Shell", "", "", spec.env};
    }
    if (serving && !cfg.assistant.command.empty()) {
        spec.command = cfg.assistant.command;  // advanced override (tests use a fake agent)
    }
    return spec;
}

void McpUi::buildTerminal() {
    Control* ctrl = server.getControl();
    GtkWidget* mainBox = ctrl->getWindow()->get("mainBox");
    GtkWidget* content = ctrl->getWindow()->get("mainContainerBox");
    if (!mainBox || !content || !GTK_IS_BOX(mainBox)) {
        return;
    }
    dock = std::make_unique<assistant::TerminalDock>(mainBox, content);
    dock->setExitHandler([this](int index, int) {
        if (index == servingTab) {
            server.serving().processExited();
        }
    });
    // The app watches, the serving session works: canvas events wake it
    assistant::EventPump::Env env;
    env.paused = [] { return api::AgentGate::paused(); };
    env.state = [this] { return server.serving().state(); };
    env.hooksSeen = [this] { return server.serving().hooksSeen(); };
    env.processRunning = [this] { return dock && servingTab >= 0 && dock->isRunning(servingTab); };
    env.lastTerminalInputUs = [this] { return dock ? dock->lastInputUs() : gint64{0}; };
    env.type = [this](const std::string& text) { return dock && dock->feed(servingTab, text); };
    env.changed = [this] { update(); };
    const auto& a = server.getConfig().assistant;
    eventPump = std::make_unique<assistant::EventPump>(
            env, assistant::EventPump::Settings{a.autoImprove, a.wakeIdleMs, a.watchdogS});
    if (auto* hub = server.getEvents()) {
        hub->addListener([this](const api::DocEvent& e) {
            if (eventPump) {
                eventPump->onDocEvent(e);
            }
        });
    }
    dock->setNewTabChoices({terminalSpec("claude", false), terminalSpec("codex", false),
                            terminalSpec("opencode", false), terminalSpec("shell", false)});
}

void McpUi::openTerminal(const std::string& kind) {
    if (dock) {
        dock->openTab(terminalSpec(kind, false), true);
    }
}

void McpUi::startServing() {
    if (!dock) {
        return;
    }
    const std::string agent = server.getConfig().assistant.agent == "codex" ? "codex" : "claude";
    const assistant::TerminalSpec spec = terminalSpec(agent, true);
    const int existing = dock->findTab(spec.title);
    if (existing >= 0) {
        if (!dock->isRunning(existing)) {
            dock->restartTab(existing);
        }
        return;
    }
    servingTab = dock->openTab(spec, true);  // shown: its first start may ask you something (e.g. folder trust)
    server.serving().processStarted();
}
#endif

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

    heading("Serving session (AI terminal)");
    check("mcpAutostart", "Start the serving session _when xournalai starts", cfg.assistant.autostart,
          "Opens the AI terminal with Claude Code (or Codex) in the companion folder when xournalai starts");
    check("mcpBypass", "S_kip permission prompts (bypass)", cfg.assistant.permissionMode != "normal",
          "--dangerously-skip-permissions for Claude Code, --dangerously-bypass-approvals-and-sandbox for Codex");
    GtkWidget* agent = named(gtk_combo_box_text_new(), "mcpAgent");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agent), "claude", "Claude Code");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(agent), "codex", "Codex");
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(agent), cfg.assistant.agent == "codex" ? "codex" : "claude");
    labelled("Serving a_gent", agent);

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
    next.assistant = cfg.assistant;  // keeps the advanced command override
    next.assistant.autostart = checked(dialog, "mcpAutostart");
    next.assistant.permissionMode = checked(dialog, "mcpBypass") ? "bypass" : "normal";
    if (const char* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(find(dialog, "mcpAgent")))) {
        next.assistant.agent = id;
    }
    next.save();
    // Apply now: agents reconnect (same port and token: their next request just starts a new session)
    server.restart();
}

}  // namespace xoj::mcp
