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

namespace xoj::mcp {

namespace {
constexpr const char* AI_LAYER_HINT = "AI";

/// Index (1-based) of the "AI" layer on a page, or 0
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
    buildStrip();
    timer = g_timeout_add_seconds(2, &McpUi::onTick, this);
    update();
}

McpUi::~McpUi() {
    if (timer) {
        g_source_remove(timer);
    }
    if (strip) {
        gtk_widget_destroy(strip);
    }
    GtkWidget* win = server.getControl()->getWindow() ? server.getControl()->getWindow()->getWindow() : nullptr;
    if (win) {
        for (const char* name: {"mcp-paused", "mcp-ai-accept", "mcp-ai-clear", "mcp-ai-toggle", "mcp-copy-command"}) {
            g_action_map_remove_action(G_ACTION_MAP(win), name);
        }
    }
    api::AgentGate::setPaused(false);
}

gboolean McpUi::onTick(gpointer self) {
    static_cast<McpUi*>(self)->update();
    return G_SOURCE_CONTINUE;
}

void McpUi::installActions() {
    Control* ctrl = server.getControl();
    GtkWidget* win = ctrl->getWindow()->getWindow();
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
                               const size_t idx = aiLayerIndex(page, ui->server.getConfig().defaultLayer);
                               if (idx) {
                                   const auto layer = static_cast<Layer::Index>(idx);
                                   c->getLayerController()->setLayerVisible(layer, !page->isLayerVisible(layer));
                               }
                           }},
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
    const size_t idx = aiLayerIndex(page, server.getConfig().defaultLayer);
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

}  // namespace xoj::mcp
