#include "McpUi.h"

#include <algorithm>  // for find, remove
#include <map>
#include <shared_mutex>

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

#include "api/DocumentApi.h"  // for currentPageIndex
#include "api/ElementIds.h"   // for ElementIds
#include "api/EventHub.h"     // for EventHub
#include "assistant/AiToolbar.h"
#include "assistant/Ask.h"                       // for AskController
#include "assistant/AskPopover.h"                // for AskPopover
#include "assistant/Markers.h"                   // for findMarkers          // for AiToolbar
#include "assistant/SpeechToText.h"              // for SpeechToText
#include "assistant/ThinkingOverlay.h"           // for ThinkingOverlay
#include "control/tools/EditSelection.h"         // for EditSelection
#include "gui/inputdevices/PenButtonObserver.h"  // for setPenButtonObserver
#include "gui/inputdevices/StrokeInterceptor.h"  // for setStrokeInterceptor
#include "model/Element.h"                       // for Element
#include "model/Stroke.h"                        // for Stroke

#include "McpConfig.h"
#include "McpHttpServer.h"
#include "McpServer.h"
#include "NotesStore.h"
#include "PathText.h"
#include "Transactions.h"

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
    server.serving().setListener([this] {
        onServingChanged();
        update();
    });
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
    buildToolbar();
    buildThinking();
    if (auto* hub = server.getEvents()) {
        hub->addListener([this](const api::DocEvent& e) { noteUserStroke(e); });
    }
#ifdef ENABLE_AI_TERMINAL
    buildTerminal();
#endif
#ifdef ENABLE_STT
    if (server.getConfig().assistant.speech) {
        speechToText = std::make_unique<assistant::SpeechToText>(server.getConfig().assistant.speechModel);
        // Load the model soon (not during startup), so the first press of the pen button is answered at once
        speechWarmUp = g_timeout_add_seconds(
                3,
                [](gpointer self) -> gboolean {
                    auto* ui = static_cast<McpUi*>(self);
                    ui->speechWarmUp = 0;
                    ui->speechToText->warmUp();
                    return G_SOURCE_REMOVE;
                },
                this);
        ask = std::make_unique<assistant::AskController>(
                speechToText.get(), [this](const assistant::AskCapture& c) { onAsk(c); },
                [this](const std::string& status) {
                    askStatusText = status;
                    if (askPopover && askPopover->visible()) {
                        askPopover->setStatus(status);
                    } else if (thinkingOverlay && (status == "nothing heard" || status.rfind("speech:", 0) == 0)) {
                        thinkingOverlay->flashRecording(status == "nothing heard" ? "Didn't catch that" : status);
                    }
                    update();
                },
                [this] { return lassoArmed || (askPopover && askPopover->visible()); },
                [this](const std::string& text) {
                    if (askPopover && askPopover->visible()) {
                        askPopover->appendText(text);
                    } else {
                        pendingDictation += (pendingDictation.empty() ? "" : " ") + text;  // for the lasso's popover
                    }
                });
        // The recording indicator: on the instant listening starts, next to the pen; level bars move with the voice
        speechToText->setListener([this](assistant::SpeechToText::State s) {
            if (!thinkingOverlay) {
                return;
            }
            using S = assistant::SpeechToText::State;
            using R = assistant::ThinkingOverlay::Recording;
            thinkingOverlay->setRecording(s == S::Listening    ? R::Listening :
                                          s == S::Transcribing ? R::Transcribing :
                                                                 R::Off,
                                          recordAnchor);
        });
        speechToText->setLevelListener([this](float rms) {
            if (thinkingOverlay) {
                thinkingOverlay->pushLevel(rms);
            }
        });
        xoj::input::setPenButtonObserver([this](const xoj::input::PenButtonEvent& e) {
            if (e.kind == xoj::input::PenButtonEvent::Down) {
                const size_t page = e.page ? this->server.getControl()->getDocument()->indexOf(e.page) : npos;
                recordAnchor = page == npos ? std::nullopt :
                                              std::optional<std::pair<size_t, xoj::util::Point<double>>>(
                                                      std::pair(page, xoj::util::Point<double>(e.x, e.y)));
            }
            if (micHeld) {
                return;  // the popover's microphone button is recording
            }
            ask->onPen(e);  // a lasso drawn meanwhile → a new ask; else, with an ask open, dictation into it
        });
        if (aiToolbar) {
            aiToolbar->setCommandOpener([this] { openAskForSelection(); });  // Command… = Ask, with dictation
        }
    }
#endif
    timer = g_timeout_add_seconds(2, &McpUi::onTick, this);
    update();
}

McpUi::~McpUi() {
    if (timer) {
        g_source_remove(timer);
    }
    if (speechWarmUp) {
        g_source_remove(speechWarmUp);
    }
    if (ask) {
        xoj::input::setPenButtonObserver(nullptr);
    }
    if (menuIdle) {
        g_source_remove(menuIdle);
    }
    if (markerTimer) {
        g_source_remove(markerTimer);
    }
    // Only what still exists: when the application quits, the main window (and all of this) is already gone
    if (settings) {
        GtkWidget* s = settings;
        unwatch(settings);
        gtk_widget_destroy(s);
    }
    removeMenu();
    aiToolbar.reset();
    thinkingOverlay.reset();
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
        for (const char* name:
             {"mcp-paused", "mcp-ai-accept", "mcp-ai-clear", "mcp-ai-toggle", "mcp-copy-command", "mcp-settings",
              "ai-terminal", "ai-terminal-open", "ai-act", "ai-toolbar", "ai-stop", "ai-auto-improve", "ai-ask",
              "ai-rule-formulas", "ai-rule-text", "ai-rule-diagrams", "ai-rule-colours"}) {
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
    // AI toolbar actions (also reachable from the menu and by tests): "improve", …, "command:<text>"
    GSimpleAction* act = g_simple_action_new("ai-act", G_VARIANT_TYPE_STRING);
    g_signal_connect(act, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant* v, gpointer self) {
                         const std::string arg = g_variant_get_string(v, nullptr);
                         const auto colon = arg.find(':');
                         static_cast<McpUi*>(self)->aiAction(arg.substr(0, colon),
                                                             colon == std::string::npos ? "" : arg.substr(colon + 1));
                     }),
                     this);
    add(act);
    GSimpleAction* tb = g_simple_action_new_stateful("ai-toolbar", nullptr, g_variant_new_boolean(true));
    g_signal_connect(tb, "change-state", G_CALLBACK(+[](GSimpleAction* a, GVariant* v, gpointer self) {
                         g_simple_action_set_state(a, v);
                         auto* ui = static_cast<McpUi*>(self);
                         if (ui->aiToolbar) {
                             ui->aiToolbar->setVisible(g_variant_get_boolean(v));
                         }
                     }),
                     this);
    add(tb);
    // Ask: arm the AI lasso (the next pen / mouse stroke circles the area)
    GSimpleAction* askAct = g_simple_action_new_stateful("ai-ask", nullptr, g_variant_new_boolean(false));
    g_signal_connect(askAct, "change-state", G_CALLBACK(+[](GSimpleAction*, GVariant* v, gpointer self) {
                         static_cast<McpUi*>(self)->armLasso(g_variant_get_boolean(v));
                     }),
                     this);
    add(askAct);
    GSimpleAction* stop = g_simple_action_new("ai-stop", nullptr);
    g_signal_connect(stop, "activate", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer self) {
                         static_cast<McpUi*>(self)->stopWork(0);
                     }),
                     this);
    add(stop);
    // Auto-improve: on = improve everything the user writes; off = markers and commands only (remembered)
    const auto& assistantCfg = server.getConfig().assistant;
    GSimpleAction* autoImp =
            g_simple_action_new_stateful("ai-auto-improve", nullptr, g_variant_new_boolean(assistantCfg.autoImprove));
    g_signal_connect(autoImp, "change-state", G_CALLBACK(+[](GSimpleAction* a, GVariant* v, gpointer self) {
                         g_simple_action_set_state(a, v);
                         auto* ui = static_cast<McpUi*>(self);
                         const bool on = g_variant_get_boolean(v);
                         McpConfig cfg = McpConfig::load();
                         cfg.assistant.autoImprove = on;
                         cfg.save();
#ifdef ENABLE_AI_TERMINAL
                         if (ui->eventPump) {
                             ui->eventPump->setAutoImprove(on);
                         }
#endif
                         ui->update();
                     }),
                     this);
    add(autoImp);
    for (const char* rule: {"formulas", "text", "diagrams", "colours"}) {
        const bool enabled =
                std::find(assistantCfg.rules.begin(), assistantCfg.rules.end(), rule) != assistantCfg.rules.end();
        GSimpleAction* ra = g_simple_action_new_stateful((std::string("ai-rule-") + rule).c_str(), nullptr,
                                                         g_variant_new_boolean(enabled));
        g_object_set_data_full(G_OBJECT(ra), "rule", g_strdup(rule), g_free);
        g_signal_connect(ra, "change-state", G_CALLBACK(+[](GSimpleAction* a, GVariant* v, gpointer self) {
                             g_simple_action_set_state(a, v);
                             auto* ui = static_cast<McpUi*>(self);
                             const std::string name = static_cast<const char*>(g_object_get_data(G_OBJECT(a), "rule"));
                             McpConfig cfg = McpConfig::load();
                             auto& rules = cfg.assistant.rules;
                             rules.erase(std::remove(rules.begin(), rules.end(), name), rules.end());
                             if (g_variant_get_boolean(v)) {
                                 rules.push_back(name);
                             }
                             cfg.save();
#ifdef ENABLE_AI_TERMINAL
                             if (ui->eventPump) {
                                 ui->eventPump->setRules(rules);
                             }
#endif
                             ui->update();
                         }),
                         this);
        add(ra);
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
    const char* autoAccel[] = {"<Ctrl><Alt>i", nullptr};
    gtk_application_set_accels_for_action(app, "win.ai-auto-improve", autoAccel);
    const char* askAccel[] = {"<Ctrl><Alt>a", nullptr};
    gtk_application_set_accels_for_action(app, "win.ai-ask", askAccel);
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
    section({{"Show AI toolbar", "win.ai-toolbar"}, {"Stop current AI work", "win.ai-stop"}});
    {
        GMenu* s = g_menu_new();
        GMenuItem* item = g_menu_item_new("Auto-_improve", "win.ai-auto-improve");
        g_menu_item_set_attribute(item, "accel", "s", "<Ctrl><Alt>i");
        g_menu_append_item(s, item);
        g_object_unref(item);
        GMenu* rules = g_menu_new();
        g_menu_append(rules, "Formulas → LaTeX", "win.ai-rule-formulas");
        g_menu_append(rules, "Text in my handwriting", "win.ai-rule-text");
        g_menu_append(rules, "Diagrams redrawn", "win.ai-rule-diagrams");
        g_menu_append(rules, "Consistent colours", "win.ai-rule-colours");
        g_menu_append_submenu(s, "Auto-improve rules", G_MENU_MODEL(rules));
        g_object_unref(rules);
        g_menu_append_section(sub, nullptr, G_MENU_MODEL(s));
        g_object_unref(s);
    }
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

void McpUi::onAsk(const assistant::AskCapture& c) {
    const size_t page = server.getControl()->getDocument()->indexOf(c.page);
    lastAsk = {{"page", page == npos ? json(nullptr) : json(page + 1)},
               {"area", {c.area.x, c.area.y, c.area.width, c.area.height}},
               {"lasso_points", c.lasso.size()},
               {"text", c.text}};
    showAsk(c, c.text);
    update();
}

std::optional<GdkRectangle> McpUi::canvasRect(size_t page, const xoj::util::Rectangle<double>& area) const {
    Control* ctrl = server.getControl();
    XournalView* xv = ctrl->getWindow()->getXournal();
    XojPageView* view = xv->getViewFor(page);
    GtkWidget* widget = xv->getWidget();
    if (!view || !gtk_widget_get_mapped(widget)) {
        return std::nullopt;
    }
    GtkXournal* xw = GTK_XOURNAL(widget);
    const double zoom = ctrl->getZoomControl()->getZoom();
    const auto pos = view->getPixelPosition();
    GdkRectangle r{static_cast<int>(pos.x + area.x * zoom - gtk_adjustment_get_value(xw->hadjustment)),
                   static_cast<int>(pos.y + area.y * zoom - gtk_adjustment_get_value(xw->vadjustment)),
                   std::max(1, static_cast<int>(area.width * zoom)), std::max(1, static_cast<int>(area.height * zoom))};
    // Keep it on the canvas (the popover points at the visible part)
    const int cw = gtk_widget_get_allocated_width(widget), ch = gtk_widget_get_allocated_height(widget);
    const int x1 = std::clamp(r.x, 0, cw - 1), y1 = std::clamp(r.y, 0, ch - 1);
    const int x2 = std::clamp(r.x + r.width, x1 + 1, cw), y2 = std::clamp(r.y + r.height, y1 + 1, ch);
    return GdkRectangle{x1, y1, x2 - x1, y2 - y1};
}

void McpUi::showAsk(const assistant::AskCapture& c, const std::string& text) {
    Control* ctrl = server.getControl();
    GtkWidget* canvas = ctrl->getWindow()->getXournal()->getWidget();
    if (!askPopover) {
        askPopover = std::make_unique<assistant::AskPopover>(
                canvas, [this](const std::string& command, const std::string& t) { submitAsk(command, t); },
                [this](bool pressed) {
                    if (!speechToText) {
                        askPopover->setStatus("Speech is off (settings: assistant.speech)");
                        return;
                    }
                    dictate(pressed);
                });
    }
    pendingAsk = std::make_unique<assistant::AskCapture>(c);
    const size_t page = ctrl->getDocument()->indexOf(c.page);
    auto rect = page == npos ? std::nullopt : canvasRect(page, c.area);
    if (!rect) {
        rect = GdkRectangle{gtk_widget_get_allocated_width(canvas) / 2, gtk_widget_get_allocated_height(canvas) / 3, 1,
                            1};
    }
    askPopover->show(*rect, text);
}

void McpUi::dictate(bool pressed) {
    if (!speechToText) {
        return;
    }
    if (pressed) {
        micHeld = true;
        if (pendingAsk && pendingAsk->page) {
            const size_t page = server.getControl()->getDocument()->indexOf(pendingAsk->page);
            if (page != npos) {
                recordAnchor = std::pair(page, xoj::util::Point<double>(pendingAsk->area.x + pendingAsk->area.width,
                                                                        pendingAsk->area.y));
            }
        }
        speechToText->start();
        askStatusText = "listening…";
        if (askPopover && askPopover->visible()) {
            askPopover->setStatus("listening…");
        }
        update();
        return;
    }
    if (!micHeld) {
        return;
    }
    micHeld = false;
    askStatusText = "transcribing…";
    if (askPopover && askPopover->visible()) {
        askPopover->setStatus("transcribing…");
    }
    update();
    speechToText->stop([this](const std::string& t, bool silent, const std::string& error) {
        askStatusText = error.empty() ? "" : "speech: " + error;
        update();
        const std::string said = silent ? "" : t;  // (the popover's microphone button)
        if (askPopover && askPopover->visible()) {
            askPopover->setStatus(error.empty() ? (silent ? "nothing heard" : "") : "speech: " + error);
            if (!said.empty()) {
                askPopover->appendText(said);
            }
        } else if (!said.empty()) {
            pendingDictation += (pendingDictation.empty() ? "" : " ") + said;  // the popover opens after the lasso
        }
    });
}

void McpUi::armLasso(bool on) {
    lassoArmed = on;
    lassoPoints.clear();
    lassoPage.reset();
    if (thinkingOverlay) {
        thinkingOverlay->setLasso(0, {});
    }
    GtkWidget* canvas = server.getControl()->getWindow()->getXournal()->getWidget();
    if (GdkWindow* w = gtk_widget_get_window(canvas)) {
        GdkCursor* cursor = on ? gdk_cursor_new_from_name(gdk_window_get_display(w), "crosshair") : nullptr;
        gdk_window_set_cursor(w, cursor);
        if (cursor) {
            g_object_unref(cursor);
        }
    }
    if (window) {
        if (GAction* a = g_action_map_lookup_action(G_ACTION_MAP(window), "ai-ask")) {
            g_simple_action_set_state(G_SIMPLE_ACTION(a), g_variant_new_boolean(on));
        }
    }
    askStatusText = on ? "circle the area (pen or mouse)…" : "";
    update();
    if (!on) {
        xoj::input::setStrokeInterceptor(nullptr);
        return;
    }
    if (askPopover && askPopover->visible()) {
        askPopover->close();
    }
    pendingDictation.clear();
    xoj::input::setStrokeInterceptor([this](const xoj::input::InterceptedStroke& s) {
        using K = xoj::input::InterceptedStroke;
        if (s.kind == K::Down) {
            lassoPage = s.page;
            lassoPoints.clear();
        }
        if (s.page && s.page == lassoPage && (s.kind == K::Down || s.kind == K::Move || s.kind == K::Up)) {
            lassoPoints.push_back({s.x, s.y});
            if (thinkingOverlay) {
                thinkingOverlay->setLasso(server.getControl()->getDocument()->indexOf(lassoPage), lassoPoints);
            }
        }
        if (s.kind == K::Up) {
            assistant::AskCapture c;
            c.page = lassoPage;
            c.lasso = lassoPoints;
            if (c.page) {
                double x1 = lassoPoints[0].x, y1 = lassoPoints[0].y, x2 = x1, y2 = y1;
                for (const auto& p: lassoPoints) {
                    x1 = std::min(x1, p.x), y1 = std::min(y1, p.y), x2 = std::max(x2, p.x), y2 = std::max(y2, p.y);
                }
                if (lassoPoints.size() < 3 || (x2 - x1 < 4 && y2 - y1 < 4)) {
                    // A tap: an area around it
                    const double w = c.page->getWidth(), h = c.page->getHeight();
                    const double aw = std::min(200.0, w), ah = std::min(120.0, h);
                    x1 = std::clamp(x1 - aw / 2, 0.0, w - aw), y1 = std::clamp(y1 - ah / 2, 0.0, h - ah);
                    x2 = x1 + aw, y2 = y1 + ah;
                    c.lasso.clear();
                }
                c.area = {x1, y1, x2 - x1, y2 - y1};
            }
            const std::string said = pendingDictation;
            armLasso(false);
            if (c.page) {
                showAsk(c, said);
            }
        }
        return true;  // the lasso is not ink
    });
}

void McpUi::openAskForSelection() {
    Control* ctrl = server.getControl();
    assistant::AskCapture c;
    const size_t page = api::currentPageIndex(ctrl);
    c.page = ctrl->getDocument()->getPage(page);
    if (EditSelection* sel = ctrl->getWindow()->getXournal()->getSelection()) {
        c.area = {sel->getXOnView(), sel->getYOnView(), sel->getWidth(), sel->getHeight()};
    } else if (std::unique_ptr<xoj::util::Rectangle<double>> vis(ctrl->getWindow()->getXournal()->getVisibleRect(page));
               vis) {
        c.area = *vis;
    } else {
        c.area = pageArea(page);
    }
    showAsk(c, "");
}

void McpUi::submitAsk(const std::string& command, const std::string& text) {
    if (!pendingAsk) {
        return;
    }
    const assistant::AskCapture c = *pendingAsk;
    pendingAsk.reset();
    Control* ctrl = server.getControl();
    const size_t page = ctrl->getDocument()->indexOf(c.page);
    if (page == npos) {
        return;  // the page is gone
    }
    // What it is about: the selection (a lasso with the selection tool takes the elements out of the page), plus
    // the elements inside the lasso (or the area)
    std::vector<std::string> ids;
    if (EditSelection* sel = ctrl->getWindow()->getXournal()->getSelection()) {
        for (const Element* e: sel->getElementsView()) {
            ids.push_back(api::ElementIds::get().idOf(e));
        }
    }
    auto inside = [&](double x, double y) {
        if (c.lasso.size() < 3) {
            return x >= c.area.x && x <= c.area.x + c.area.width && y >= c.area.y && y <= c.area.y + c.area.height;
        }
        bool in = false;
        for (size_t i = 0, j = c.lasso.size() - 1; i < c.lasso.size(); j = i++) {
            const auto& a = c.lasso[i];
            const auto& b = c.lasso[j];
            if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) {
                in = !in;
            }
        }
        return in;
    };
    {
        std::shared_lock lock(*ctrl->getDocument());
        for (const auto& loc: api::elementsOnPage(c.page, page, std::nullopt, c.area, true)) {
            const auto& bb = loc.element->getBoundingBox();
            if (inside(bb.x + bb.width / 2, bb.y + bb.height / 2)) {
                const std::string id = api::ElementIds::get().idOf(loc.element);
                if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
                    ids.push_back(id);
                }
            }
        }
    }
    auto r = [](double v) { return std::to_string(std::lround(v)); };
    std::string desc = "Ask" + (command.empty() ? std::string() : " [" + command + "]") + ": \"" + text + "\" — ";
    desc += ids.empty() ? std::string("about this area") : "about " + std::to_string(ids.size()) + " element(s)";
    desc += " on page " + std::to_string(page + 1) + " at [" + r(c.area.x) + "," + r(c.area.y) + "," + r(c.area.width) +
            "," + r(c.area.height) + "]" + (c.lasso.size() >= 3 ? " (circled)" : "");
    if (!ids.empty()) {
        std::string list;
        for (size_t i = 0; i < ids.size() && i < 12; i++) {
            list += (i ? "," : "") + ids[i];
        }
        desc += " (ids " + list + (ids.size() > 12 ? ",…" : "") + ")";
    }
    int zone = 0;
    if (thinkingOverlay) {
        zone = thinkingOverlay->add(page, c.area, command.empty() ? "working on it…" : command + "…",
                                    assistant::ThinkingOverlay::State::Queued);
        desc += " [zone " + std::to_string(zone) + "]";
    }
    if (auto* hub = server.getEvents()) {
        hub->pushIntent(page, ids, c.area, desc);
    }
#ifdef ENABLE_AI_TERMINAL
    if (eventPump) {
        eventPump->addIntent(desc, zone);
    }
#endif
    lastAsk["submitted"] = {{"command", command}, {"text", text}, {"zone", zone}, {"ids", ids}, {"desc", desc}};
    update();
}

json McpUi::askStatus() const {
    if (!ask) {
        return {{"state", "off"}};
    }
    json j = {{"state", ask->listening() ? "listening" : (askStatusText.empty() ? "idle" : askStatusText)}};
    if (thinkingOverlay) {
        using R = assistant::ThinkingOverlay::Recording;
        const auto r = thinkingOverlay->recording();
        j["recording"] = r == R::Listening ? "listening" : r == R::Transcribing ? "transcribing" : "off";
        j["recording_levels"] = thinkingOverlay->levelCount();
    }
    if (!lastAsk.is_null()) {
        j["last"] = lastAsk;
    }
    return j;
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
                if (sv.subagents() > 0) {
                    text += " (" + std::to_string(sv.subagents()) + " subagent" + (sv.subagents() == 1 ? "" : "s") +
                            " working)";
                }
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
        if (thinkingOverlay && thinkingOverlay->active() > 0) {
            text += "  |  " + std::to_string(thinkingOverlay->active()) + " in progress";
        }
        if (!askStatusText.empty()) {
            text += "  |  Ask: " + askStatusText;
        }
        if (eventPump) {
            text += std::string("  |  Auto-improve ") + (eventPump->autoImprove() ? "ON" : "off");
            if (!eventPump->status().empty()) {
                text += "  |  " + eventPump->status();
            }
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
    env.delivered = [this](const std::vector<int>& zones, bool edits, size_t page,
                           const xoj::util::Rectangle<double>& area) {
        if (!thinkingOverlay) {
            return;
        }
        std::vector<int> started = zones;
        for (int z: zones) {
            thinkingOverlay->set(z, assistant::ThinkingOverlay::State::Thinking);
        }
        if (edits && (area.width > 0 || area.height > 0)) {
            started.push_back(thinkingOverlay->add(page, area, "improving what you wrote…",
                                                   assistant::ThinkingOverlay::State::Thinking));
        }
        // Without hooks (Codex) nothing tells us when it's done: the zones end by themselves after a while
        if (!server.serving().hooksSeen() && !started.empty()) {
            struct Later {
                McpUi* ui;
                std::vector<int> zones;
                std::shared_ptr<bool> alive;
            };
            g_timeout_add_seconds_full(
                    G_PRIORITY_DEFAULT, 20,
                    [](gpointer d) -> gboolean {
                        auto* l = static_cast<Later*>(d);
                        if (*l->alive && l->ui->thinkingOverlay) {
                            for (int z: l->zones) {
                                l->ui->thinkingOverlay->set(z, assistant::ThinkingOverlay::State::Done);
                            }
                        }
                        return G_SOURCE_REMOVE;
                    },
                    new Later{this, started, server.aliveToken()}, +[](gpointer d) { delete static_cast<Later*>(d); });
        }
    };
    const auto& a = server.getConfig().assistant;
    eventPump = std::make_unique<assistant::EventPump>(
            env, assistant::EventPump::Settings{a.autoImprove, a.wakeIdleMs, a.watchdogS, a.rules,
                                                std::clamp(a.maxParallel, 1, 5)});
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

void McpUi::buildToolbar() {
    GtkWidget* mainBox = server.getControl()->getWindow()->get("mainBox");
    GtkWidget* tbTop2 = server.getControl()->getWindow()->get("tbTop2");
    if (!mainBox || !GTK_IS_BOX(mainBox)) {
        return;
    }
    int position = 2;
    if (tbTop2) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(mainBox));
        const int i = g_list_index(children, tbTop2);
        g_list_free(children);
        if (i >= 0) {
            position = i + 1;
        }
    }
    aiToolbar = std::make_unique<assistant::AiToolbar>(
            mainBox, position, [this](const std::string& kind, const std::string& text) { aiAction(kind, text); });
}

std::string McpUi::aiAction(const std::string& kind, const std::string& text) {
    Control* ctrl = server.getControl();
    auto* hub = server.getEvents();
    // What it applies to: the selection, else the last piece the user drew, else the current page
    size_t page = api::currentPageIndex(ctrl);
    std::vector<std::string> ids;
    xoj::util::Rectangle<double> area{0, 0, 0, 0};
    std::string target;
    auto r = [](double v) { return std::to_string(std::lround(v)); };
    if (EditSelection* sel = ctrl->getWindow()->getXournal()->getSelection()) {
        for (const Element* e: sel->getElementsView()) {
            ids.push_back(api::ElementIds::get().idOf(e));
        }
        area = {sel->getXOnView(), sel->getYOnView(), sel->getWidth(), sel->getHeight()};
        target = "the selection (" + std::to_string(ids.size()) + " element(s))";
    } else if (auto last = hub ? hub->lastUserAddition() : std::nullopt; last && kind != "revise") {
        page = last->page;
        ids = last->ids;
        area = last->area;
        target = "the last thing the user drew";
    } else {
        target = "the whole page";
    }
    std::string what;
    if (kind == "improve") {
        what = "*! improve the strokes (same object, pose and size; assist, don't redo)";
    } else if (kind == "illustrate") {
        what = "**! make a professional illustration of it";
    } else if (kind == "web") {
        what = "*w! search the web for it and write a short summary next to it";
    } else if (kind == "image") {
        what = "*r! place a real image of it (a PD/CC0 photo or a generated one)";
    } else if (kind == "command") {
        what = "*c! the user's command: \"" + text + "\"";
    } else if (kind == "revise") {
        what = "revise the whole page carefully (a full pass)";
    } else {
        return {};
    }
    std::string desc = "AI toolbar: " + what + " — on " + target + " on page " + std::to_string(page + 1);
    if (area.width > 0 || area.height > 0) {
        desc += " at [" + r(area.x) + "," + r(area.y) + "," + r(area.width) + "," + r(area.height) + "]";
    }
    if (!ids.empty()) {
        std::string list;
        for (size_t i = 0; i < ids.size() && i < 12; i++) {
            list += (i ? "," : "") + ids[i];
        }
        desc += " (ids " + list + (ids.size() > 12 ? ",…" : "") + ")";
    }
    int zone = 0;
    if (thinkingOverlay) {
        static const std::map<std::string, std::string> labels = {
                {"improve", "improving strokes…"}, {"illustrate", "illustrating…"}, {"web", "searching the web…"},
                {"image", "finding an image…"},    {"command", "working on it…"},   {"revise", "revising the page…"}};
        const auto zoneArea = (area.width > 0 || area.height > 0) ? area : pageArea(page);
        zone = thinkingOverlay->add(page, zoneArea, labels.count(kind) ? labels.at(kind) : "working…",
                                    assistant::ThinkingOverlay::State::Queued);
        desc += " [zone " + std::to_string(zone) + "]";
    }
    if (hub) {
        hub->pushIntent(page, ids, area, desc);
    }
#ifdef ENABLE_AI_TERMINAL
    if (eventPump) {
        eventPump->addIntent(desc, zone);
    }
#endif
    return desc;
}

void McpUi::noteUserStroke(const api::DocEvent& e) {
    if (e.origin != "user" || e.type != "element_added") {
        return;
    }
    const gint64 now = g_get_monotonic_time();
    for (const auto& id: e.ids) {
        recentStrokes.push_back({e.page, id, now});
    }
    // only the last ~20 s matter (a marker is written in one go)
    recentStrokes.erase(std::remove_if(recentStrokes.begin(), recentStrokes.end(),
                                       [now](const RecentStroke& r) { return now - r.timeUs > 20 * G_USEC_PER_SEC; }),
                        recentStrokes.end());
    if (markerTimer) {
        g_source_remove(markerTimer);
    }
    markerTimer = g_timeout_add(
            1200,
            [](gpointer self) -> gboolean {
                auto* ui = static_cast<McpUi*>(self);
                ui->markerTimer = 0;
                ui->scanMarkers();
                return G_SOURCE_REMOVE;
            },
            this);
}

void McpUi::scanMarkers() {
    Control* ctrl = server.getControl();
    Document* doc = ctrl->getDocument();
    std::map<size_t, std::vector<assistant::StrokeShape>> byPage;
    {
        std::shared_lock lock(*doc);
        for (const auto& r: recentStrokes) {
            if (std::find(markerIdsDone.begin(), markerIdsDone.end(), r.id) != markerIdsDone.end()) {
                continue;
            }
            try {
                const auto loc = api::locateId(doc, r.id);
                if (auto* s = dynamic_cast<Stroke*>(loc.element)) {
                    const auto& bb = s->getBoundingBox();
                    if (std::max(bb.width, bb.height) <= 40) {  // marker strokes are small
                        byPage[loc.page].push_back({r.id, s->getPointVector()});
                    }
                }
            } catch (const std::exception&) {
                // erased meanwhile
            }
        }
    }
    auto* hub = server.getEvents();
    auto rnd = [](double v) { return std::to_string(std::lround(v)); };
    for (const auto& [page, strokes]: byPage) {
        for (const auto& m: assistant::findMarkers(strokes)) {
            markerIdsDone.insert(markerIdsDone.end(), m.ids.begin(), m.ids.end());
            std::string ids;
            for (size_t i = 0; i < m.ids.size(); i++) {
                ids += (i ? "," : "") + m.ids[i];
            }
            const std::string where = "[" + rnd(m.area.x) + "," + rnd(m.area.y) + "," + rnd(m.area.width) + "," +
                                      rnd(m.area.height) + "]";
            std::string desc = "handwritten marker " + m.kind + " on page " + std::to_string(page + 1) + " at " +
                               where + " (marker strokes " + ids + ": delete them after acting)";
            if (m.kind == "*?!") {
                desc += "; read its letter (w, c, r, …) from page_render with region " + where;
            }
            desc += "; it applies to the drawing or text right next to it";
            int zone = 0;
            if (thinkingOverlay) {
                zone = thinkingOverlay->add(page, m.area, "marker " + m.kind + " seen…",
                                            assistant::ThinkingOverlay::State::Queued);
                desc += " [zone " + std::to_string(zone) + "]";
            }
            if (hub) {
                hub->pushIntent(page, m.ids, m.area, desc);
            }
#ifdef ENABLE_AI_TERMINAL
            if (eventPump) {
                eventPump->addIntent(desc, zone);
            }
#endif
        }
    }
    if (markerIdsDone.size() > 2000) {
        markerIdsDone.erase(markerIdsDone.begin(), markerIdsDone.begin() + 1000);
    }
}

xoj::util::Rectangle<double> McpUi::pageArea(size_t page) const {
    Document* doc = server.getControl()->getDocument();
    std::shared_lock lock(*doc);
    if (page >= doc->getPageCount()) {
        return {0, 0, 0, 0};
    }
    PageRef p = doc->getPage(page);
    return {0, 0, p->getWidth(), p->getHeight()};
}

void McpUi::buildThinking() {
    GtkWidget* overlay = server.getControl()->getWindow()->get("mainOverlay");
    if (!overlay || !GTK_IS_OVERLAY(overlay)) {
        return;
    }
    auto mapper = [this, overlay](size_t page,
                                  const xoj::util::Rectangle<double>& area) -> std::optional<GdkRectangle> {
        if (!window) {
            return std::nullopt;
        }
        Control* ctrl = server.getControl();
        XournalView* xv = ctrl->getWindow()->getXournal();
        XojPageView* view = xv->getViewFor(page);
        GtkWidget* widget = xv->getWidget();
        if (!view || !gtk_widget_get_mapped(widget)) {
            return std::nullopt;
        }
        GtkXournal* xw = GTK_XOURNAL(widget);
        const double zoom = ctrl->getZoomControl()->getZoom();
        const auto pos = view->getPixelPosition();
        const double x = pos.x + area.x * zoom - gtk_adjustment_get_value(xw->hadjustment);
        const double y = pos.y + area.y * zoom - gtk_adjustment_get_value(xw->vadjustment);
        int ox = 0, oy = 0;
        if (!gtk_widget_translate_coordinates(widget, overlay, static_cast<int>(x), static_cast<int>(y), &ox, &oy)) {
            return std::nullopt;
        }
        GdkRectangle r{ox, oy, std::max(8, static_cast<int>(area.width * zoom)),
                       std::max(8, static_cast<int>(area.height * zoom))};
        // clipped to the canvas: zones scrolled out of view aren't drawn
        int cx = 0, cy = 0;
        gtk_widget_translate_coordinates(widget, overlay, 0, 0, &cx, &cy);
        const int cw = gtk_widget_get_allocated_width(widget), ch = gtk_widget_get_allocated_height(widget);
        if (r.x + r.width < cx || r.y + r.height < cy || r.x > cx + cw || r.y > cy + ch) {
            return std::nullopt;
        }
        return r;
    };
    auto bounds = [this, overlay]() -> std::optional<GdkRectangle> {
        if (!window) {
            return std::nullopt;
        }
        GtkWidget* widget = server.getControl()->getWindow()->getXournal()->getWidget();
        int cx = 0, cy = 0;
        if (!gtk_widget_get_mapped(widget) || !gtk_widget_translate_coordinates(widget, overlay, 0, 0, &cx, &cy)) {
            return std::nullopt;
        }
        return GdkRectangle{cx, cy, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget)};
    };
    thinkingOverlay =
            std::make_unique<assistant::ThinkingOverlay>(overlay, mapper, [this](int zone) { stopWork(zone); }, bounds);
    // Every transaction is visible: in its request's zone, or a new one over its area
    server.transactions().setBeginListener([this](const Transaction& t) {
        if (!thinkingOverlay) {
            return t.zone;
        }
        const auto zones = thinkingOverlay->zones();
        const bool known = std::any_of(zones.begin(), zones.end(), [&t](const auto& z) { return z.id == t.zone; });
        if (t.zone && known) {
            thinkingOverlay->set(t.zone, assistant::ThinkingOverlay::State::Thinking, t.label + "…");
            return t.zone;
        }
        return thinkingOverlay->add(t.page, t.region ? *t.region : pageArea(t.page), t.label + "…",
                                    assistant::ThinkingOverlay::State::Thinking);
    });
    server.transactions().setZoneListener([this](int zone, const std::string& state, const std::string& why) {
        if (thinkingOverlay) {
            thinkingOverlay->set(zone,
                                 state == "done" ? assistant::ThinkingOverlay::State::Done :
                                                   assistant::ThinkingOverlay::State::Failed,
                                 why);
        }
        update();
    });
}

void McpUi::stopWork(int zone) {
    using Z = assistant::ThinkingOverlay::State;
    api::AgentGate::hurry();  // a playback in progress completes at once
    server.transactions().abortAll("stopped by the user");
#ifdef ENABLE_AI_TERMINAL
    // Esc interrupts Claude Code (and Codex) mid-turn, like pressing it in the terminal
    if (dock && servingTab >= 0 && server.serving().state() != assistant::ServingState::State::Idle) {
        dock->feed(servingTab, "\x1b");
    }
    if (eventPump) {
        eventPump->clear();
    }
#endif
    if (thinkingOverlay) {
        // The session works on one turn at a time: stopping any zone (zone != 0) stops that turn, so every zone in
        // progress ends, and queued requests are dropped with it
        (void)zone;
        for (Z st: {Z::Thinking, Z::Queued}) {
            for (int z: thinkingOverlay->inState(st)) {
                thinkingOverlay->set(z, Z::Failed, "stopped");
            }
        }
    }
    update();
}

void McpUi::onServingChanged() {
    using S = assistant::ServingState::State;
    const auto st = server.serving().state();
    // the session finished a turn: what it was thinking about is done
    // (not if it handed the work to a subagent: those zones end with the subagent's transaction, or its thinking
    // done/fail, or Stop — a background subagent works on long after the coordinator's turn)
    if (thinkingOverlay && (st == S::Idle || st == S::NotRunning) && lastServingState == static_cast<int>(S::Busy) &&
        (st == S::NotRunning || (server.serving().subagents() == 0 && !server.serving().delegated()))) {
        const auto owned = server.transactions().openZones();  // they end with their transaction
        for (int z: thinkingOverlay->inState(assistant::ThinkingOverlay::State::Thinking)) {
            if (std::find(owned.begin(), owned.end(), z) != owned.end()) {
                continue;
            }
            thinkingOverlay->set(z, st == S::Idle ? assistant::ThinkingOverlay::State::Done :
                                                    assistant::ThinkingOverlay::State::Failed);
        }
    }
    lastServingState = static_cast<int>(st);
}

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
    check("mcpChooseLayer", "Let agents c_hoose another layer when they ask for one", cfg.agentsChooseLayer,
          "Off: everything the AI draws goes to the layer above, whatever the agent asks");
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
    GtkWidget* par = named(gtk_spin_button_new_with_range(1, 5, 1), "mcpParallel");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(par), std::clamp(cfg.assistant.maxParallel, 1, 5));
    gtk_widget_set_tooltip_text(par, "How many subagents (and edit transactions) may work at the same time");
    labelled("Parallel s_ubagents", par);

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
    next.agentsChooseLayer = checked(dialog, "mcpChooseLayer");
    next.backups = checked(dialog, "mcpBackups");
    next.assistant = cfg.assistant;  // keeps the advanced command override
    next.assistant.autostart = checked(dialog, "mcpAutostart");
    next.assistant.permissionMode = checked(dialog, "mcpBypass") ? "bypass" : "normal";
    next.assistant.maxParallel = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(find(dialog, "mcpParallel")));
    if (const char* id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(find(dialog, "mcpAgent")))) {
        next.assistant.agent = id;
    }
    next.save();
    // Apply now: agents reconnect (same port and token: their next request just starts a new session)
    server.restart();
}

}  // namespace xoj::mcp
