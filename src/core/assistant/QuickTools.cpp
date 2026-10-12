#include "QuickTools.h"

#include <algorithm>  // for stable_sort
#include <array>      // for array
#include <cmath>      // for sqrt, cos, sin
#include <fstream>    // for ifstream, ofstream

#include "control/Control.h"                 // for Control
#include "control/ToolEnums.h"               // for ToolType, ToolSize
#include "control/ToolHandler.h"             // for ToolHandler
#include "control/settings/Settings.h"       // for Settings (touch drawing)
#include "control/actions/ActionDatabase.h"  // for ActionDatabase
#include "gui/MainWindow.h"
#include "gui/RecordingIndicator.h"  // for RecordingIndicator                  // for MainWindow
#include "gui/XournalView.h"         // for XournalView
#include "mcp/Json.h"
#include "util/GVariantTemplate.h"  // for getGVariantValue
#include "gui/toolbarMenubar/model/ColorPalette.h"  // for Palette

namespace xoj::assistant {

namespace quick_tools {

const std::vector<Entry>& catalog() {
    static const std::vector<Entry> all = {
            {"pen", "Pen", "xopp-tool-pencil", "Draw with the pen"},
            {"eraser", "Eraser", "xopp-tool-eraser", "Erase"},
            {"ask", "Ask", "audio-input-microphone-symbolic",
             "Ask the AI: circle an area, then say or type what you want"},
            {"select", "Select", "xopp-select-lasso", "Select a region (lasso)"},
            {"auto-improve", "Auto-improve", "starred-symbolic",
             "Auto-improve (Ctrl+Alt+I): off, the AI acts only on markers and requests; on, it improves what you write"},
            {"touch", "Touch", "xopp-touch-drawing",
             "Touch drawing: off, only the stylus draws (fingers scroll); on, fingers draw and mark too"},
            {"ai-highlighter", "AI highlight", "xopp-ai-highlighter",
             "Highlight what you mean while you say it; a pause opens Ask"},
            {"command", "Command", "mail-send-symbolic", "Tell the AI what to do with the selection (or the page)"},
            {"terminal", "Terminal", "utilities-terminal-symbolic", "Show or hide the AI terminal"},
            {"size", "Size", "xopp-thickness-medium", "Thickness: fine → medium → thick"},
            {"new-page", "New page", "xopp-page-add", "Insert a page after this one"},
            {"highlighter", "Highlighter", "xopp-tool-highlighter", "Highlight"},
            {"undo", "Undo", "xopp-edit-undo", "Undo"},
            {"redo", "Redo", "xopp-edit-redo", "Redo"},
            {"hand", "Hand", "xopp-hand", "Move the page around"},
            {"text", "Text", "xopp-tool-text", "Type text"},
            {"select-rect", "Rectangle", "xopp-select-rect", "Select a rectangle"},
            {"select-object", "Pointer", "xopp-object-select", "Select an object by tapping it"},
            {"size-0", "Very fine", "xopp-thickness-finer", "Thickness of the tool in use: very fine"},
            {"size-1", "Fine", "xopp-thickness-fine", "Thickness of the tool in use: fine"},
            {"size-2", "Medium", "xopp-thickness-medium", "Thickness of the tool in use: medium"},
            {"size-3", "Thick", "xopp-thickness-thick", "Thickness of the tool in use: thick"},
            {"size-4", "Very thick", "xopp-thickness-thicker", "Thickness of the tool in use: very thick"},
            {"shape-line", "Line", "xopp-draw-line", "Draw straight lines (pen)"},
            {"shape-rect", "Rectangle", "xopp-draw-rect", "Draw rectangles (pen)"},
            {"shape-ellipse", "Ellipse", "xopp-draw-ellipse", "Draw ellipses (pen)"},
            {"shape-arrow", "Arrow", "xopp-draw-arrow", "Draw arrows (pen)"},
            {"shape-recognizer", "Recognize", "xopp-shape-recognizer",
             "Shape recognizer: draw roughly, it becomes a clean shape (pen)"},
            {"zoom-out", "Zoom out", "zoom-out-symbolic", "Zoom out"},
            {"zoom-fit", "Fit", "zoom-fit-best-symbolic", "Fit the page to the window"},
            {"zoom-in", "Zoom in", "zoom-in-symbolic", "Zoom in"},
    };
    return all;
}

std::vector<std::string> ranked(const std::map<std::string, int>& counts, size_t n) {
    std::vector<std::string> ids;
    for (const auto& e: catalog()) {
        ids.emplace_back(e.id);
    }
    auto uses = [&](const std::string& id) {
        auto it = counts.find(id);
        return it == counts.end() ? 0 : it->second;
    };
    std::stable_sort(ids.begin(), ids.end(), [&](const auto& a, const auto& b) { return uses(a) > uses(b); });
    if (ids.size() > n) {
        ids.resize(n);
    }
    return ids;
}

std::map<std::string, int> load(const std::filesystem::path& file) {
    std::map<std::string, int> out;
    std::ifstream in(file);
    if (!in) {
        return out;
    }
    const auto j = xoj::mcp::json::parse(in, nullptr, false);
    if (j.is_object() && j.contains("uses") && j["uses"].is_object()) {
        for (const auto& [id, v]: j["uses"].items()) {
            if (v.is_number_integer()) {
                out[id] = v.get<int>();
            }
        }
    }
    return out;
}

bool save(const std::filesystem::path& file, const std::map<std::string, int>& counts) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    out << xoj::mcp::json{{"uses", counts}}.dump(2) << "\n";
    return static_cast<bool>(out);
}

}  // namespace quick_tools

namespace {
struct ToolId {
    const char* id;
    ToolType type;
};
constexpr ToolId TOOLS[] = {{"pen", TOOL_PEN},
                            {"eraser", TOOL_ERASER},
                            {"highlighter", TOOL_HIGHLIGHTER},
                            {"select", TOOL_SELECT_REGION},
                            {"select-rect", TOOL_SELECT_RECT},
                            {"select-object", TOOL_SELECT_OBJECT},
                            {"hand", TOOL_HAND},
                            {"text", TOOL_TEXT}};

const char* sizeName(ToolSize s) {
    switch (s) {
        case TOOL_SIZE_VERY_FINE:
        case TOOL_SIZE_FINE:
            return "fine";
        case TOOL_SIZE_MEDIUM:
            return "medium";
        default:
            return "thick";
    }
}

struct Counter {
    QuickTools* self;
    std::string id;  ///< empty: from the tool type of the parameter
};
}  // namespace

QuickTools::QuickTools(Control* ctrl, std::filesystem::path countsFile, std::function<void(const std::string&)> r):
        control(ctrl), file(std::move(countsFile)), run(std::move(r)), uses(quick_tools::load(file)) {
    if (auto it = uses.find("_toolsButton"); it != uses.end()) {
        wantButton = it->second != 0;
        uses.erase(it);
    }
    if (auto it = uses.find("_recordButton"); it != uses.end()) {
        wantRecord = it->second != 0;
        uses.erase(it);
    }
    if (auto it = uses.find("_fullscreenButton"); it != uses.end()) {
        wantFullscreen = it->second != 0;
        uses.erase(it);
    }
    if (auto it = uses.find("_focusButton"); it != uses.end()) {
        wantFocusButton = it->second != 0;
        uses.erase(it);
    }
    if (auto it = uses.find("_focus"); it != uses.end()) {
        focused = it->second != 0;
        uses.erase(it);
    }
    connectCounting();
}

QuickTools::~QuickTools() {
    for (auto& [obj, id]: handlers) {
        if (G_IS_OBJECT(obj) && g_signal_handler_is_connected(obj, id)) {
            g_signal_handler_disconnect(obj, id);
        }
        g_object_unref(obj);
    }
    closePopover();
    if (button) {
        g_object_remove_weak_pointer(G_OBJECT(button), reinterpret_cast<gpointer*>(&button));
    }
    if (recordPoll) {
        g_source_remove(recordPoll);
    }
    if (recordFace && faceTick) {
        gtk_widget_remove_tick_callback(recordFace, faceTick);
    }
    if (recordFace) {
        g_object_remove_weak_pointer(G_OBJECT(recordFace), reinterpret_cast<gpointer*>(&recordFace));
    }
    if (recordButton) {
        g_object_remove_weak_pointer(G_OBJECT(recordButton), reinterpret_cast<gpointer*>(&recordButton));
    }
    if (fullscreenButton) {
        g_object_remove_weak_pointer(G_OBJECT(fullscreenButton), reinterpret_cast<gpointer*>(&fullscreenButton));
    }
    if (focusButton) {
        g_object_remove_weak_pointer(G_OBJECT(focusButton), reinterpret_cast<gpointer*>(&focusButton));
    }
    if (toolbarColumn) {
        g_object_remove_weak_pointer(G_OBJECT(toolbarColumn), reinterpret_cast<gpointer*>(&toolbarColumn));
    }
    for (auto& [id, w]: tabletTiles) {
        if (w) {
            g_object_remove_weak_pointer(G_OBJECT(w), reinterpret_cast<gpointer*>(&w));
        }
    }
    if (colourButton) {
        g_object_remove_weak_pointer(G_OBJECT(colourButton), reinterpret_cast<gpointer*>(&colourButton));
    }
    if (tabletBox) {
        g_object_remove_weak_pointer(G_OBJECT(tabletBox), reinterpret_cast<gpointer*>(&tabletBox));
    }
    if (modeBar) {
        g_object_remove_weak_pointer(G_OBJECT(modeBar), reinterpret_cast<gpointer*>(&modeBar));
    }
    if (outerBox) {
        g_object_remove_weak_pointer(G_OBJECT(outerBox), reinterpret_cast<gpointer*>(&outerBox));
    }
}

void QuickTools::connectCounting() {
    // Every way of choosing a tool goes through the app's actions: count them there
    // (a stateful action is counted on "change-state": a handler on its "activate" would replace GTK's default one,
    // which is what turns an activation into a state change)
    auto connect = [&](GAction* action, std::string id) {
        if (!action) {
            return;
        }
        const gulong h = g_signal_connect_data(
                action, g_action_get_state_type(action) ? "change-state" : "activate",
                G_CALLBACK(+[](GSimpleAction*, GVariant* p, gpointer d) {
                    auto* c = static_cast<Counter*>(d);
                    if (c->self->performing ||
                        (p && g_variant_is_of_type(p, G_VARIANT_TYPE_BOOLEAN) && !g_variant_get_boolean(p))) {
                        return;  // (switching Ask off is not a use)
                    }
                    if (!c->id.empty()) {
                        c->self->count(c->id);
                        return;
                    }
                    if (p) {
                        const auto type = getGVariantValue<ToolType>(p);
                        for (const auto& t: TOOLS) {
                            if (t.type == type) {
                                c->self->count(t.id);
                            }
                        }
                    }
                }),
                new Counter{this, std::move(id)}, +[](gpointer d, GClosure*) { delete static_cast<Counter*>(d); },
                G_CONNECT_AFTER);
        handlers.emplace_back(g_object_ref(action), h);
    };
    ActionDatabase* db = control->getActionDatabase();
    connect(G_ACTION(db->getAction(Action::SELECT_TOOL).get()), "");
    connect(G_ACTION(db->getAction(Action::UNDO).get()), "undo");
    connect(G_ACTION(db->getAction(Action::REDO).get()), "redo");
    connect(G_ACTION(db->getAction(Action::NEW_PAGE_AFTER).get()), "new-page");
    connect(G_ACTION(db->getAction(Action::TOOL_SIZE).get()), "size");
    connect(G_ACTION(db->getAction(Action::TOOL_PEN_SIZE).get()), "size");
    if (auto* win = control->getWindow() ? control->getWindow()->getWindow() : nullptr) {
        connect(g_action_map_lookup_action(G_ACTION_MAP(win), "ai-ask"), "ask");
        connect(g_action_map_lookup_action(G_ACTION_MAP(win), "ai-highlighter"), "ai-highlighter");
        connect(g_action_map_lookup_action(G_ACTION_MAP(win), "ai-terminal"), "terminal");
    }
}

void QuickTools::count(const std::string& id) {
    if (!id.empty()) {
        uses[id]++;
    }
    auto all = uses;
    all["_toolsButton"] = wantButton ? 1 : 0;
    all["_recordButton"] = wantRecord ? 1 : 0;  // (whether the Tools button is shown rides along)
    all["_fullscreenButton"] = wantFullscreen ? 1 : 0;
    all["_focusButton"] = wantFocusButton ? 1 : 0;
    all["_focus"] = focused ? 1 : 0;
    quick_tools::save(file, all);
}

void QuickTools::activate(const std::string& id) {
    if (id != "command") {  // (a command is counted when it is sent)
        count(id);
    }
    performing = true;
    ActionDatabase* db = control->getActionDatabase();
    bool done = false;
    const bool isTool = std::any_of(std::begin(TOOLS), std::end(TOOLS), [&](const auto& t) { return id == t.id; }) ||
                        id.rfind("shape-", 0) == 0;
    if (isTool) {
        // A tool ends the AI highlighter and the lasso (also when it is the tool in use already)
        if (auto* win = control->getWindow() ? control->getWindow()->getWindow() : nullptr) {
            for (const char* name: {"ai-highlighter", "ai-ask"}) {
                if (GAction* a = g_action_map_lookup_action(G_ACTION_MAP(win), name)) {
                    GVariant* st = g_action_get_state(a);
                    if (st && g_variant_get_boolean(st)) {
                        g_action_change_state(a, g_variant_new_boolean(false));
                    }
                    if (st) {
                        g_variant_unref(st);
                    }
                }
            }
        }
    }
    for (const auto& t: TOOLS) {
        if (id == t.id) {
            db->fireActivateAction(Action::SELECT_TOOL, t.type);
            done = true;
        }
    }
    if (done) {
    } else if (id == "undo") {
        db->fireActivateAction(Action::UNDO);
    } else if (id == "redo") {
        db->fireActivateAction(Action::REDO);
    } else if (id == "new-page") {
        db->fireActivateAction(Action::NEW_PAGE_AFTER);
    } else if (id.rfind("shape-", 0) == 0) {
        // A shape is drawn with the pen: picks it, then the shape (a second tap on the shape in use: freehand again)
        const Action a = id == "shape-line"      ? Action::TOOL_DRAW_LINE :
                         id == "shape-rect"      ? Action::TOOL_DRAW_RECTANGLE :
                         id == "shape-ellipse"   ? Action::TOOL_DRAW_ELLIPSE :
                         id == "shape-arrow"     ? Action::TOOL_DRAW_ARROW :
                                                   Action::TOOL_DRAW_SHAPE_RECOGNIZER;
        if (control->getToolHandler()->getToolType() != TOOL_PEN) {
            db->fireActivateAction(Action::SELECT_TOOL, TOOL_PEN);
        }
        GVariant* st = g_action_get_state(G_ACTION(db->getAction(a).get()));
        const bool on = st && g_variant_get_boolean(st);
        if (st) {
            g_variant_unref(st);
        }
        db->fireChangeActionState(a, !on);
    } else if (id == "auto-improve") {
        // A switch: the same as the menu's and Ctrl+Alt+I (the AI highlighter / lasso stay on)
        if (auto* win = control->getWindow() ? control->getWindow()->getWindow() : nullptr) {
            if (GAction* a = g_action_map_lookup_action(G_ACTION_MAP(win), "ai-auto-improve")) {
                GVariant* st = g_action_get_state(a);
                const bool on = st && g_variant_get_boolean(st);
                if (st) {
                    g_variant_unref(st);
                }
                g_action_change_state(a, g_variant_new_boolean(!on));
            }
        }
    } else if (id == "touch") {
        // A switch, not a tool: the AI highlighter / lasso stay on
        db->fireChangeActionState(Action::TOGGLE_TOUCH_DRAWING, !control->getSettings()->getTouchDrawingEnabled());
    } else if (id == "zoom-in") {
        db->fireActivateAction(Action::ZOOM_IN);
    } else if (id == "zoom-out") {
        db->fireActivateAction(Action::ZOOM_OUT);
    } else if (id == "zoom-fit") {
        db->fireActivateAction(Action::ZOOM_FIT);
    } else if (id.rfind("color-", 0) == 0) {
        const auto& palette = control->getPalette();
        const size_t i = std::stoul(id.substr(6));
        if (i < palette.size()) {
            if (!control->getToolHandler()->hasCapability(TOOL_CAP_COLOR)) {
                db->fireActivateAction(Action::SELECT_TOOL, TOOL_PEN);  // a colour is a colour of something: the pen
            }
            db->fireChangeActionState(Action::TOOL_COLOR, palette.getColorAt(i).getColor());
        }
    } else if (id.rfind("size-", 0) == 0) {
        ToolHandler* th = control->getToolHandler();
        if (!th->hasCapability(TOOL_CAP_SIZE)) {
            db->fireActivateAction(Action::SELECT_TOOL, TOOL_PEN);  // a size is a size of something: the pen
        }
        db->fireActivateAction(Action::TOOL_SIZE, static_cast<ToolSize>(std::stoi(id.substr(5))));
    } else if (id == "size") {
        ToolHandler* th = control->getToolHandler();
        if (!th->hasCapability(TOOL_CAP_SIZE)) {
            db->fireActivateAction(Action::SELECT_TOOL, TOOL_PEN);  // a size is a size of something: the pen
        }
        const ToolSize now = th->getActiveTool()->getSize();
        const ToolSize next = now <= TOOL_SIZE_FINE   ? TOOL_SIZE_MEDIUM :
                              now == TOOL_SIZE_MEDIUM ? TOOL_SIZE_THICK :
                                                        TOOL_SIZE_FINE;
        db->fireActivateAction(Action::TOOL_SIZE, next);
    } else if (run) {
        run(id);
    }
    performing = false;
}

void QuickTools::closePopover() {
    if (popover) {
        GtkWidget* p = popover;
        g_object_remove_weak_pointer(G_OBJECT(p), reinterpret_cast<gpointer*>(&popover));
        popover = nullptr;
        gtk_widget_destroy(p);  // (an idle destroy already queued for it holds its own reference)
    }
}

namespace {
constexpr const char* CSS =
        ".xoai-tools-button { color: #ffffff; border: none; border-radius: 16px; padding: 6px 10px;"
        "  background-image: linear-gradient(160deg, #ff7a45, #e8366f 55%, #8b45e8);"
        "  box-shadow: 0 2px 6px alpha(#8b45e8, 0.45), inset 0 1px alpha(#ffffff, 0.35);"
        "  -gtk-icon-shadow: 0 1px 1px alpha(#000000, 0.3); transition: all 120ms ease-out; }"
        ".xoai-tools-button:hover { background-image: linear-gradient(160deg, #ff8f5e, #f04c80 55%, #9d5cf2);"
        "  box-shadow: 0 3px 10px alpha(#e8366f, 0.6), inset 0 1px alpha(#ffffff, 0.45); }"
        ".xoai-tools-button:active { background-image: linear-gradient(160deg, #e0602d, #c92159 55%, #7232c9);"
        "  box-shadow: inset 0 2px 4px alpha(#000000, 0.35); }"
        ".xoai-tools-button label { color: #ffffff; font-weight: bold; }"
        ".xoai-record-button { color: #ffffff; border: none; border-radius: 16px; padding: 6px 10px;"
        "  background-image: linear-gradient(160deg, #5a5f6b, #3a3e47);"
        "  box-shadow: 0 2px 6px alpha(#000000, 0.35), inset 0 1px alpha(#ffffff, 0.2); transition: all 120ms "
        "ease-out; }"
        ".xoai-record-button:hover { background-image: linear-gradient(160deg, #6a707d, #474c57); }"
        ".xoai-record-button:active { box-shadow: inset 0 2px 4px alpha(#000000, 0.45); }"
        ".xoai-record-button label { color: #ffffff; font-weight: bold; }"
        ".xoai-record-button image { color: #ff4a4a; }"
        ".xoai-record-button.xoai-recording { background-image: linear-gradient(160deg, #ff5a5a, #d81b3c);"
        "  box-shadow: 0 0 10px alpha(#ff3040, 0.75), inset 0 1px alpha(#ffffff, 0.35); }"
        ".xoai-record-button.xoai-recording image { color: #ffffff; }"
        ".xoai-fullscreen-button { color: #ffffff; border: none; border-radius: 16px; padding: 6px 10px;"
        "  background-image: linear-gradient(160deg, #2bb3c0, #2d6fe0);"
        "  box-shadow: 0 2px 6px alpha(#2d6fe0, 0.45), inset 0 1px alpha(#ffffff, 0.3); transition: all 120ms "
        "ease-out; }"
        ".xoai-fullscreen-button:hover { background-image: linear-gradient(160deg, #3cc6d3, #4583ef); }"
        ".xoai-fullscreen-button:active { box-shadow: inset 0 2px 4px alpha(#000000, 0.4); }"
        ".xoai-fullscreen-button label { color: #ffffff; font-weight: bold; }"
        ".xoai-focus-button { color: #ffffff; border: none; border-radius: 16px; padding: 6px 10px;"
        "  background-image: linear-gradient(160deg, #3fc37a, #1e9a8a);"
        "  box-shadow: 0 2px 6px alpha(#1e9a8a, 0.45), inset 0 1px alpha(#ffffff, 0.3); transition: all 120ms "
        "ease-out; }"
        ".xoai-focus-button:hover { background-image: linear-gradient(160deg, #52d48b, #28ad9c); }"
        ".xoai-focus-button:active { box-shadow: inset 0 2px 4px alpha(#000000, 0.4); }"
        ".xoai-focus-button.xoai-focused { background-image: linear-gradient(160deg, #f2b33d, #e07b28); }"
        ".xoai-focus-button label { color: #ffffff; font-weight: bold; }"
        ".xoai-quick-colour { min-width: 56px; min-height: 56px; padding: 2px; }"
        ".xoai-quick-tile.xoai-tablet-tile { min-width: 64px; min-height: 64px; padding: 4px; }"
        ".xoai-mode-bar { background-image: linear-gradient(90deg, alpha(#b57bff, 0.16), alpha(#b57bff, 0.04));"
        "  border-radius: 16px; }"
        ".xoai-mode-info { font-size: 1.15em; font-weight: bold; }"
        ".xoai-mode-done { color: #ffffff; border: none; border-radius: 16px; font-size: 1.3em; font-weight: bold;"
        "  background-image: linear-gradient(160deg, #b57bff, #7b3fe0);"
        "  box-shadow: 0 2px 8px alpha(#7b3fe0, 0.5), inset 0 1px alpha(#ffffff, 0.3); }"
        ".xoai-mode-done label { color: #ffffff; }"
        ".xoai-mode-discard { border-radius: 16px; font-size: 1.05em; }"
        ".xoai-quick-tile.xoai-tablet-tile.xoai-touch-switch { min-width: 100px; }"
        ".xoai-touch-switch label { font-size: 0.85em; }"
        ".xoai-touch-switch.xoai-touch-on { background-image: none; background-color: #f6d36b;"
        "  box-shadow: inset 0 -3px #d99a00; }"
        ".xoai-touch-switch.xoai-touch-on label { font-weight: bold; }"
        ".xoai-quick-heading { font-size: 0.8em; font-weight: bold; opacity: 0.6; margin-top: 4px; }"
        ".xoai-quick-tile { padding: 6px 4px; min-width: 84px; min-height: 76px; border-radius: 12px; }"
        ".xoai-quick-tile label { font-size: 0.92em; }"
        ".xoai-quick-tile.xoai-active { background-image: none; background-color: alpha(#3584e4, 0.22);"
        "  box-shadow: inset 0 -3px #3584e4; }";

void addCss() {
    static bool added = false;
    if (added) {
        return;
    }
    added = true;
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CSS, -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

/// The grid's groups, always in the same places (fingers learn places)
struct Group {
    const char* title;
    std::vector<const char*> ids;
};
const std::vector<Group>& groups() {
    static const std::vector<Group> all = {
            {"Select", {"select", "select-rect", "select-object"}},
            {"Draw", {"pen", "highlighter", "eraser", "text"}},
            {"Shapes", {"shape-line", "shape-rect", "shape-ellipse", "shape-arrow", "shape-recognizer"}},
            {"Size", {"size-0", "size-1", "size-2", "size-3", "size-4"}},
            {"Colour", {}},  // (the palette's colours)
            {"Page", {"undo", "redo", "new-page"}},
            {"View", {"touch", "zoom-out", "zoom-fit", "zoom-in"}},
            {"AI", {"ask", "ai-highlighter", "command", "auto-improve", "terminal"}},
    };
    return all;
}

GtkWidget* swatch(Color c, bool active) {
    // A round drop of the colour (a ring around the one in use)
    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, 40, 40);
    g_object_set_data(G_OBJECT(area), "rgb", GUINT_TO_POINTER(uint32_t(c)));
    g_object_set_data(G_OBJECT(area), "active", GINT_TO_POINTER(active));
    g_signal_connect(area, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer) -> gboolean {
                         const auto v = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(w), "rgb"));
                         const double cx = gtk_widget_get_allocated_width(w) / 2.0;
                         const double cy = gtk_widget_get_allocated_height(w) / 2.0;
                         cairo_arc(cr, cx, cy, 15, 0, 2 * G_PI);
                         cairo_set_source_rgb(cr, ((v >> 16) & 0xff) / 255.0, ((v >> 8) & 0xff) / 255.0,
                                              (v & 0xff) / 255.0);
                         cairo_fill_preserve(cr);
                         cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.6);
                         cairo_set_line_width(cr, 1);
                         cairo_stroke(cr);
                         if (g_object_get_data(G_OBJECT(w), "active")) {
                             cairo_arc(cr, cx, cy, 19, 0, 2 * G_PI);
                             cairo_set_source_rgb(cr, 0.21, 0.52, 0.89);
                             cairo_set_line_width(cr, 3);
                             cairo_stroke(cr);
                         }
                         return TRUE;
                     }),
                     nullptr);
    return area;
}

GtkWidget* tileContent(const char* icon, const std::string& label, int px) {
    GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_DND);
    gtk_image_set_pixel_size(GTK_IMAGE(image), px);
    gtk_box_pack_start(GTK_BOX(inner), image, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(inner), gtk_label_new(label.c_str()), FALSE, FALSE, 0);
    return inner;
}
}  // namespace

void QuickTools::open(GtkWidget* anchor) {
    closePopover();
    addCss();
    GtkWidget* relative = anchor;
    if (!relative && button && gtk_widget_get_mapped(button)) {
        relative = button;
    }
    GdkRectangle point{0, 0, 1, 1};
    const bool centred = !relative || !gtk_widget_get_mapped(relative);
    if (centred) {
        relative = control->getWindow()->getXournal()->getWidget();
        point = {gtk_widget_get_allocated_width(relative) / 2, gtk_widget_get_allocated_height(relative) / 3, 1, 1};
    }
    popover = gtk_popover_new(relative);
    g_object_add_weak_pointer(G_OBJECT(popover), reinterpret_cast<gpointer*>(&popover));
    if (centred) {
        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &point);
    }
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "aiQuickTools");

    // Two columns of groups, so that all of them fit on a laptop screen
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
    g_object_set(box, "margin", 10, nullptr);
    GtkWidget* columns[2] = {gtk_box_new(GTK_ORIENTATION_VERTICAL, 4), gtk_box_new(GTK_ORIENTATION_VERTICAL, 4)};
    gtk_box_pack_start(GTK_BOX(box), columns[0], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), columns[1], FALSE, FALSE, 0);
    ToolHandler* th = control->getToolHandler();
    const ToolType active = th->getToolType();
    const auto& cat = quick_tools::catalog();
    const size_t half = (groups().size() + 1) / 2;
    for (size_t gi = 0; gi < groups().size(); gi++) {
        const auto& g = groups()[gi];
        GtkWidget* column = columns[gi < half ? 0 : 1];
        GtkWidget* heading = gtk_label_new(g.title);
        gtk_widget_set_halign(heading, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(heading), "xoai-quick-heading");
        gtk_box_pack_start(GTK_BOX(column), heading, FALSE, FALSE, 0);
        GtkWidget* row = gtk_box_new(g.ids.empty() ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_set_homogeneous(GTK_BOX(row), FALSE);
        if (g.ids.empty()) {
            addColourTiles(row);
        }
        for (const char* id: g.ids) {
            const auto it =
                    std::find_if(cat.begin(), cat.end(), [&](const auto& e) { return std::string(id) == e.id; });
            std::string label = it->label;
            if (std::string(id) == "size" && th->hasCapability(TOOL_CAP_SIZE)) {
                label = std::string("Size: ") + sizeName(th->getActiveTool()->getSize());
            }
            GtkWidget* tile = gtk_button_new();
            gtk_container_add(GTK_CONTAINER(tile), tileContent(it->icon, label, 36));
            gtk_style_context_add_class(gtk_widget_get_style_context(tile), "xoai-quick-tile");
            gtk_widget_set_size_request(tile, 88, 80);
            gtk_widget_set_tooltip_text(tile, it->tip);
            gtk_widget_set_can_focus(tile, FALSE);
            gtk_buildable_set_name(GTK_BUILDABLE(tile), (std::string("aiQuick-") + id).c_str());
            bool on = false;
            for (const auto& t: TOOLS) {
                on = on || (std::string(id) == t.id && t.type == active);
            }
            if (std::string(id).rfind("size-", 0) == 0 && th->hasCapability(TOOL_CAP_SIZE)) {
                on = std::string(id) == "size-" + std::to_string(static_cast<int>(th->getActiveTool()->getSize()));
            }
            if (std::string(id) == "touch") {
                on = control->getSettings()->getTouchDrawingEnabled();
            }
            if (std::string(id) == "auto-improve") {
                if (auto* win = control->getWindow() ? control->getWindow()->getWindow() : nullptr) {
                    if (GAction* a = g_action_map_lookup_action(G_ACTION_MAP(win), "ai-auto-improve")) {
                        GVariant* st = g_action_get_state(a);
                        on = st && g_variant_get_boolean(st);
                        if (st) {
                            g_variant_unref(st);
                        }
                    }
                }
            }
            if (std::string(id).rfind("shape-", 0) == 0 && active == TOOL_PEN) {
                const DrawingType dt = th->getActiveTool()->getDrawingType();
                const std::string sid = id;
                on = (sid == "shape-line" && dt == DRAWING_TYPE_LINE) ||
                     (sid == "shape-rect" && dt == DRAWING_TYPE_RECTANGLE) ||
                     (sid == "shape-ellipse" && dt == DRAWING_TYPE_ELLIPSE) ||
                     (sid == "shape-arrow" && dt == DRAWING_TYPE_ARROW) ||
                     (sid == "shape-recognizer" && dt == DRAWING_TYPE_SHAPE_RECOGNIZER);
            }
            if (on) {
                gtk_style_context_add_class(gtk_widget_get_style_context(tile), "xoai-active");
            }
            g_object_set_data(G_OBJECT(tile), "tool", const_cast<char*>(it->id));
            g_signal_connect(tile, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                                 auto* self = static_cast<QuickTools*>(d);
                                 const std::string tool =
                                         static_cast<const char*>(g_object_get_data(G_OBJECT(b), "tool"));
                                 if (self->popover) {
                                     gtk_popover_popdown(GTK_POPOVER(self->popover));
                                 }
                                 self->activate(tool);
                             }),
                             this);
            gtk_box_pack_start(GTK_BOX(row), tile, FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(column), row, FALSE, FALSE, 0);
    }
    gtk_container_add(GTK_CONTAINER(popover), box);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
                         // destroyed after closing (idle: not inside its own signal); `popover` is a weak pointer
                         g_idle_add(
                                 [](gpointer w) -> gboolean {
                                     if (GTK_IS_WIDGET(w) && !gtk_widget_in_destruction(GTK_WIDGET(w))) {
                                         gtk_widget_destroy(GTK_WIDGET(w));
                                     }
                                     g_object_unref(w);
                                     return G_SOURCE_REMOVE;
                                 },
                                 g_object_ref(p));
                     }),
                     nullptr);
    gtk_widget_show_all(popover);
    gtk_popover_popup(GTK_POPOVER(popover));
}

void QuickTools::addColourTiles(GtkWidget* row) {
    const auto& palette = control->getPalette();
    ToolHandler* th = control->getToolHandler();
    const bool hasColour = th->hasCapability(TOOL_CAP_COLOR);
    const uint32_t now = hasColour ? uint32_t(th->getActiveTool()->getColor()) & 0xffffffU : 0x1000000U;
    GtkWidget* line = nullptr;
    for (size_t i = 0; i < std::min<size_t>(palette.size(), 10); i++) {
        if (i % 5 == 0) {  // (five to a line)
            line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
            gtk_box_pack_start(GTK_BOX(row), line, FALSE, FALSE, 0);
        }
        const Color c = palette.getColorAt(i).getColor();
        GtkWidget* tile = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(tile), swatch(c, (uint32_t(c) & 0xffffffU) == now));
        gtk_style_context_add_class(gtk_widget_get_style_context(tile), "xoai-quick-tile");
        gtk_style_context_add_class(gtk_widget_get_style_context(tile), "xoai-quick-colour");
        gtk_widget_set_size_request(tile, 56, 56);
        gtk_widget_set_tooltip_text(tile, palette.getColorAt(i).getName().c_str());
        gtk_widget_set_can_focus(tile, FALSE);
        const std::string id = "color-" + std::to_string(i);
        gtk_buildable_set_name(GTK_BUILDABLE(tile), ("aiQuick-" + id).c_str());
        g_object_set_data_full(G_OBJECT(tile), "tool", g_strdup(id.c_str()), g_free);
        g_signal_connect(tile, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             auto* self = static_cast<QuickTools*>(d);
                             const std::string tool = static_cast<const char*>(g_object_get_data(G_OBJECT(b), "tool"));
                             if (self->popover) {
                                 gtk_popover_popdown(GTK_POPOVER(self->popover));
                             }
                             self->activate(tool);
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(line), tile, FALSE, FALSE, 0);
    }
}

void QuickTools::placeButton(GtkWidget* mainBox, const std::vector<GtkWidget*>& rows) {
    // The toolbar rows go into a column, and the button stands to their right, as tall as all of them together
    std::vector<GtkWidget*> present;
    for (GtkWidget* r: rows) {
        if (r && gtk_widget_get_parent(r) == mainBox) {
            present.push_back(r);
        }
    }
    if (present.empty()) {
        return;
    }
    addCss();
    GList* children = gtk_container_get_children(GTK_CONTAINER(mainBox));
    const int position = g_list_index(children, present.front());
    g_list_free(children);
    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_buildable_set_name(GTK_BUILDABLE(outer), "aiToolbarsWithTools");
    GtkWidget* column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_buildable_set_name(GTK_BUILDABLE(column), "aiToolbarColumn");
    for (GtkWidget* r: present) {
        gboolean expand = FALSE, fill = TRUE;
        guint padding = 0;
        GtkPackType pack = GTK_PACK_START;
        gtk_box_query_child_packing(GTK_BOX(mainBox), r, &expand, &fill, &padding, &pack);
        g_object_ref(r);
        gtk_container_remove(GTK_CONTAINER(mainBox), r);
        gtk_box_pack_start(GTK_BOX(column), r, expand, fill, padding);
        g_object_unref(r);
    }
    gtk_box_pack_start(GTK_BOX(outer), column, TRUE, TRUE, 0);
    buildTablet(outer);
    // Full screen shows or hides the menu bar and toolbars itself: in Focus, they are hidden again afterwards
    {
        GAction* fs = fullscreenAction();
        const gulong h = g_signal_connect_data(
                fs, "change-state", G_CALLBACK(+[](GSimpleAction*, GVariant*, gpointer d) {
                    g_idle_add(
                            [](gpointer d) -> gboolean {
                                static_cast<QuickTools*>(d)->reapplyFocus();
                                return G_SOURCE_REMOVE;
                            },
                            d);
                }),
                this, nullptr, G_CONNECT_AFTER);
        handlers.emplace_back(g_object_ref(fs), h);
    }

    button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(button), tileContent("view-app-grid-symbolic", "Tools", 34));
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "xoai-tools-button");
    gtk_widget_set_size_request(button, 76, -1);
    g_object_set(button, "margin", 4, nullptr);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_widget_set_tooltip_text(button, "Tools (F8): big buttons for the pen and fingers");
    gtk_buildable_set_name(GTK_BUILDABLE(button), "aiQuickToolsButton");
    g_signal_connect(button, "clicked",
                     G_CALLBACK(+[](GtkButton* b, gpointer d) { static_cast<QuickTools*>(d)->open(GTK_WIDGET(b)); }),
                     this);
    gtk_box_pack_end(GTK_BOX(outer), button, FALSE, FALSE, 0);

    // Record (experimental): starts and stops audio recording, like the toolbar's record button
    recordButton = gtk_button_new();
    {
        // A drawn face that listens: a red dot at rest; while recording, rings and rays that follow the microphone
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        recordFace = gtk_drawing_area_new();
        gtk_widget_set_size_request(recordFace, 52, 52);
        g_signal_connect(recordFace, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer d) -> gboolean {
                             static_cast<QuickTools*>(d)->drawRecordFace(w, cr);
                             return TRUE;
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(inner), recordFace, TRUE, TRUE, 0);
        recordLabel = gtk_label_new("Record");
        gtk_box_pack_start(GTK_BOX(inner), recordLabel, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(recordButton), inner);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(recordButton), "xoai-record-button");
    gtk_widget_set_size_request(recordButton, 76, -1);
    g_object_set(recordButton, "margin", 4, "margin-end", 0, nullptr);
    gtk_widget_set_can_focus(recordButton, FALSE);
    gtk_widget_set_tooltip_text(recordButton, "Start or stop recording audio");
    gtk_buildable_set_name(GTK_BUILDABLE(recordButton), "aiRecordButton");
    g_signal_connect(recordButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                         auto* self = static_cast<QuickTools*>(d);
                         GAction* a =
                                 G_ACTION(self->control->getActionDatabase()->getAction(Action::AUDIO_RECORD).get());
                         GVariant* st = g_action_get_state(a);
                         const bool on = st && g_variant_get_boolean(st);
                         if (st) {
                             g_variant_unref(st);
                         }
                         g_action_change_state(a, g_variant_new_boolean(!on));
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(outer), recordButton, FALSE, FALSE, 0);

    // Full screen: the app's own full-screen toggle, big enough for a finger
    fullscreenButton = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(fullscreenButton), "xoai-fullscreen-button");
    gtk_widget_set_size_request(fullscreenButton, 76, -1);
    g_object_set(fullscreenButton, "margin", 4, "margin-end", 0, nullptr);
    gtk_widget_set_can_focus(fullscreenButton, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(fullscreenButton), "aiFullscreenButton");
    g_signal_connect(fullscreenButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                         auto* self = static_cast<QuickTools*>(d);
                         g_action_change_state(self->fullscreenAction(),
                                               g_variant_new_boolean(!self->isFullscreen()));
                         self->refreshFullscreen();
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(outer), fullscreenButton, FALSE, FALSE, 0);

    // Focus: hides the menu bar and the toolbars; only these big buttons stay (and bring them back)
    focusButton = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(focusButton), "xoai-focus-button");
    gtk_widget_set_size_request(focusButton, 76, -1);
    g_object_set(focusButton, "margin", 4, "margin-end", 0, nullptr);
    gtk_widget_set_can_focus(focusButton, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(focusButton), "aiFocusButton");
    g_signal_connect(focusButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                         auto* self = static_cast<QuickTools*>(d);
                         if (self->onFocus) {
                             self->onFocus(!self->focused);  // (through the action, so the menu's check follows)
                         } else {
                             self->setFocused(!self->focused);
                         }
                     }),
                     this);
    gtk_box_pack_end(GTK_BOX(outer), focusButton, FALSE, FALSE, 0);
    outerBox = outer;
    g_object_add_weak_pointer(G_OBJECT(outerBox), reinterpret_cast<gpointer*>(&outerBox));
    toolbarColumn = column;
    g_object_add_weak_pointer(G_OBJECT(toolbarColumn), reinterpret_cast<gpointer*>(&toolbarColumn));
    mainWindow = gtk_widget_get_toplevel(mainBox);


    gtk_box_pack_start(GTK_BOX(mainBox), outer, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(mainBox), outer, position);
    gtk_widget_show(column);
    gtk_widget_show_all(button);
    gtk_widget_set_no_show_all(button, TRUE);
    gtk_widget_set_visible(button, wantButton);
    gtk_widget_show(outer);
    g_object_add_weak_pointer(G_OBJECT(button), reinterpret_cast<gpointer*>(&button));
    gtk_widget_show_all(recordButton);
    gtk_widget_set_no_show_all(recordButton, TRUE);
    gtk_widget_set_visible(recordButton, wantRecord);
    g_object_add_weak_pointer(G_OBJECT(recordButton), reinterpret_cast<gpointer*>(&recordButton));
    g_object_add_weak_pointer(G_OBJECT(recordFace), reinterpret_cast<gpointer*>(&recordFace));
    refreshRecord();
    refreshFullscreen();
    gtk_widget_set_no_show_all(fullscreenButton, TRUE);
    gtk_widget_set_visible(fullscreenButton, wantFullscreen);
    g_object_add_weak_pointer(G_OBJECT(fullscreenButton), reinterpret_cast<gpointer*>(&fullscreenButton));
    gtk_widget_set_no_show_all(focusButton, TRUE);
    gtk_widget_set_visible(focusButton, wantFocusButton);
    g_object_add_weak_pointer(G_OBJECT(focusButton), reinterpret_cast<gpointer*>(&focusButton));
    const bool startFocused = focused;
    focused = false;
    setFocused(startFocused);
    // Started in Focus: the window shows its toolbars only after this; hide them again once it has
    for (guint ms: {300U, 1500U}) {
        g_timeout_add(
                ms,
                [](gpointer d) -> gboolean {
                    static_cast<QuickTools*>(d)->reapplyFocus();
                    return G_SOURCE_REMOVE;
                },
                this);
    }
    recordPoll = g_timeout_add(
            200,
            [](gpointer d) -> gboolean {
                auto* self = static_cast<QuickTools*>(d);
                if (self->recordButton && self->isRecording() != self->shownRecording) {
                    self->shownRecording = !self->shownRecording;
                    self->refreshRecord();
                }
                if (self->focused) {
                    self->refreshTablet();  // (the tool may have changed anywhere)
                }
                if (self->fullscreenButton && self->isFullscreen() != self->shownFullscreen) {
                    self->refreshFullscreen();  // (also left by F11 or the menu)
                }
                return G_SOURCE_CONTINUE;
            },
            this);
}

bool QuickTools::isRecording() const {
#ifdef ENABLE_AUDIO
    if (auto* ind = control->getRecordingIndicator()) {
        return ind->isRecording();
    }
#endif
    return false;
}

void QuickTools::refreshRecord() {
    if (!recordButton) {
        return;
    }
    const bool on = isRecording();
    GtkStyleContext* sc = gtk_widget_get_style_context(recordButton);
    on ? gtk_style_context_add_class(sc, "xoai-recording") : gtk_style_context_remove_class(sc, "xoai-recording");
    gtk_widget_set_tooltip_text(recordButton, on ? "Stop recording" : "Start recording audio");
    std::string text = "Record";
#ifdef ENABLE_AUDIO
    if (on) {
        const int64_t s = control->getRecordingIndicator()->recordingMs() / 1000;
        text = "Stop " + std::to_string(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + std::to_string(s % 60);
    }
#endif
    gtk_label_set_text(GTK_LABEL(recordLabel), text.c_str());
    if (on && !faceTick) {
        faceTick = gtk_widget_add_tick_callback(
                recordFace,
                [](GtkWidget* w, GdkFrameClock*, gpointer d) -> gboolean {
                    auto* self = static_cast<QuickTools*>(d);
                    if (!self->isRecording()) {
                        self->faceTick = 0;
                        self->refreshRecord();
                        gtk_widget_queue_draw(w);
                        return G_SOURCE_REMOVE;
                    }
                    gtk_widget_queue_draw(w);
                    const gint64 now = g_get_monotonic_time();
                    if (now - self->labelUs > 250000) {
                        self->labelUs = now;
                        self->refreshRecord();
                    }
                    return G_SOURCE_CONTINUE;
                },
                this, nullptr);
    }
    gtk_widget_queue_draw(recordFace);
}

void QuickTools::drawRecordFace(GtkWidget* w, cairo_t* cr) {
    const double cx = gtk_widget_get_allocated_width(w) / 2.0;
    const double cy = gtk_widget_get_allocated_height(w) / 2.0;
    if (!isRecording()) {
        smoothLevel = 0;
        cairo_arc(cr, cx, cy, 13, 0, 2 * G_PI);
        cairo_set_source_rgb(cr, 1.0, 0.29, 0.29);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        return;
    }
    std::array<float, 18> levels{};
#ifdef ENABLE_AUDIO
    levels = control->getRecordingIndicator()->recentLevels();
#endif
    // How loud now: the latest levels, made sensitive to a quiet voice (sqrt), rising fast and falling slowly
    const double now = std::sqrt(std::max({levels[17], levels[16], levels[15]}));
    smoothLevel = now > smoothLevel ? smoothLevel + (now - smoothLevel) * 0.6 : smoothLevel * 0.9 + now * 0.1;
    const double L = std::clamp(smoothLevel, 0.0, 1.0);
    const double t = static_cast<double>(g_get_monotonic_time()) / G_USEC_PER_SEC;

    // The voice ring: rays around the button, one per recent level, the newest at the top, going round
    const double inner = 13;
    for (size_t i = 0; i < levels.size(); i++) {
        const double a = -G_PI / 2 + 2 * G_PI * static_cast<double>(levels.size() - 1 - i) / levels.size() + t * 0.6;
        const double len = 2 + 11 * std::sqrt(std::clamp(static_cast<double>(levels[i]), 0.0, 1.0));
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_width(cr, 2.6);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.45 + 0.55 * std::min(1.0, len / 13));
        cairo_move_to(cr, cx + inner * std::cos(a), cy + inner * std::sin(a));
        cairo_line_to(cr, cx + (inner + len) * std::cos(a), cy + (inner + len) * std::sin(a));
        cairo_stroke(cr);
    }
    // The core breathes with the voice: silent, it stays small and still
    const double r = 9 + 3.5 * L;
    cairo_arc(cr, cx, cy, r + 2 + 4 * L, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.15 + 0.35 * L);
    cairo_fill(cr);
    cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_fill(cr);
    // and says how to stop: a red square
    const double q = r * 0.75;
    cairo_rectangle(cr, cx - q / 2, cy - q / 2, q, q);
    cairo_set_source_rgb(cr, 0.85, 0.11, 0.24);
    cairo_fill(cr);
}

GAction* QuickTools::fullscreenAction() const {
    return G_ACTION(control->getActionDatabase()->getAction(Action::FULLSCREEN).get());
}

bool QuickTools::isFullscreen() const {
    GVariant* st = g_action_get_state(fullscreenAction());
    const bool on = st && g_variant_get_boolean(st);
    if (st) {
        g_variant_unref(st);
    }
    return on;
}

void QuickTools::refreshFullscreen() {
    if (!fullscreenButton) {
        return;
    }
    shownFullscreen = isFullscreen();
    if (GtkWidget* old = gtk_bin_get_child(GTK_BIN(fullscreenButton))) {
        gtk_widget_destroy(old);
    }
    GtkWidget* content = shownFullscreen ? tileContent("view-restore-symbolic", "Window", 34) :
                                           tileContent("view-fullscreen-symbolic", "Full screen", 34);
    gtk_container_add(GTK_CONTAINER(fullscreenButton), content);
    gtk_widget_show_all(content);
    gtk_widget_set_tooltip_text(fullscreenButton, shownFullscreen ? "Leave full screen (F11)" : "Full screen (F11)");
}

namespace {
/// The tablet buttons shown in Focus, left to right by priority (the last ones give way first)
constexpr const char* TABLET[] = {"touch",  "auto-improve", "ai-highlighter", "ask",     "pen",
                                  "eraser", "select",       "command",        "terminal"};
constexpr size_t TABLET_FIRST = 2;    ///< the switches first (a gap after them: hard to hit by accident)
constexpr size_t TABLET_ACTIONS = 7;  ///< asking and drawing; a gap, then the AI actions

/// The switches among the tablet buttons: they say their state under the icon
bool isSwitch(const std::string& id) { return id == "touch" || id == "auto-improve"; }
constexpr int TABLET_W = 64;
}  // namespace

void QuickTools::buildTablet(GtkWidget* outer) {
    // Big icon buttons for a finger or the stylus, on the left of the strip, in Focus only
    // (in a scroller without scroll bars: the tiles don't set the window's minimum width, they give way)
    tabletBox = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tabletBox), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
    gtk_buildable_set_name(GTK_BUILDABLE(tabletBox), "aiTabletButtons");
    GtkWidget* tiles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_set(tiles, "margin", 4, nullptr);
    gtk_container_add(GTK_CONTAINER(tabletBox), tiles);
    const auto& cat = quick_tools::catalog();
    auto make = [&](const std::string& id, const char* icon, const char* tip) {
        GtkWidget* b = gtk_button_new();
        GtkWidget* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_DND);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 36);
        gtk_container_add(GTK_CONTAINER(b), image);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-quick-tile");
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-tablet-tile");
        gtk_widget_set_size_request(b, TABLET_W, -1);
        gtk_widget_set_tooltip_text(b, tip);
        gtk_widget_set_can_focus(b, FALSE);
        gtk_buildable_set_name(GTK_BUILDABLE(b), ("aiTablet-" + id).c_str());
        g_object_set_data_full(G_OBJECT(b), "tool", g_strdup(id.c_str()), g_free);
        return b;
    };
    tabletTiles.reserve(std::size(TABLET));  // (their addresses are watched: no reallocation)
    for (size_t i = 0; i < std::size(TABLET); i++) {
        const auto it = std::find_if(cat.begin(), cat.end(), [&](const auto& e) { return std::string(TABLET[i]) == e.id; });
        GtkWidget* b = make(TABLET[i], it->icon, it->tip);
        if (isSwitch(TABLET[i])) {
            // A switch, and it says so: "Touch off" / "Touch on" under the icon (filled in by refreshTablet)
            gtk_widget_destroy(gtk_bin_get_child(GTK_BIN(b)));
            gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-touch-switch");
            touchShown = -1;
        }
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* w, gpointer d) {
                             static_cast<QuickTools*>(d)->activate(
                                     static_cast<const char*>(g_object_get_data(G_OBJECT(w), "tool")));
                             static_cast<QuickTools*>(d)->refreshTablet();
                         }),
                         this);
        if (i == TABLET_ACTIONS || i == TABLET_FIRST) {
            g_object_set(b, "margin-start", 14, nullptr);  // (the gap between drawing and AI)
        }
        gtk_box_pack_start(GTK_BOX(tiles), b, FALSE, FALSE, 0);
        tabletTiles.emplace_back(TABLET[i], b);
        g_object_add_weak_pointer(G_OBJECT(b), reinterpret_cast<gpointer*>(&tabletTiles.back().second));
        if (std::string(TABLET[i]) == "pen") {
            // The pen's colours: shown while the pen is in use, opens the palette as big swatches
            colourButton = make("colours", "applications-graphics-symbolic", "Pen colour: the palette");
            g_signal_connect(colourButton, "clicked", G_CALLBACK(+[](GtkButton* w, gpointer d) {
                                 static_cast<QuickTools*>(d)->openColours(GTK_WIDGET(w));
                             }),
                             this);
            gtk_box_pack_start(GTK_BOX(tiles), colourButton, FALSE, FALSE, 0);
            g_object_add_weak_pointer(G_OBJECT(colourButton), reinterpret_cast<gpointer*>(&colourButton));
        }
    }
    gtk_box_pack_start(GTK_BOX(outer), tabletBox, TRUE, TRUE, 0);
    gtk_widget_show_all(tabletBox);
    gtk_widget_set_no_show_all(tabletBox, TRUE);
    gtk_widget_set_visible(tabletBox, FALSE);
    g_object_add_weak_pointer(G_OBJECT(tabletBox), reinterpret_cast<gpointer*>(&tabletBox));
    // When the window is narrow, the last ones give way (the right-hand buttons never do)
    g_signal_connect(tabletBox, "size-allocate", G_CALLBACK(+[](GtkWidget* w, GdkRectangle* a, gpointer d) {
                         auto* self = static_cast<QuickTools*>(d);
                         if (a->width != self->tabletFits) {
                             self->tabletFits = a->width;  // (the width there is; refreshTablet fills it)
                             g_idle_add(
                                     [](gpointer d) -> gboolean {
                                         static_cast<QuickTools*>(d)->refreshTablet();
                                         return G_SOURCE_REMOVE;
                                     },
                                     d);
                         }
                         (void)w;
                     }),
                     this);
}

void QuickTools::refreshTablet() {
    if (!tabletBox || !focused) {
        return;
    }
    ToolHandler* th = control->getToolHandler();
    const ToolType active = th->getToolType();
    auto stateOf = [&](const char* name) {
        bool on = false;
        if (auto* win = control->getWindow() ? control->getWindow()->getWindow() : nullptr) {
            if (GAction* a = g_action_map_lookup_action(G_ACTION_MAP(win), name)) {
                GVariant* st = g_action_get_state(a);
                on = st && g_variant_get_boolean(st);
                if (st) {
                    g_variant_unref(st);
                }
            }
        }
        return on;
    };
    const bool highlighting = stateOf("ai-highlighter");
    const bool asking = stateOf("ai-ask");
    // In priority order, as many as the width there is takes (the colour button counts while it is shown)
    const bool colours = active == TOOL_PEN && !highlighting;
    int room = tabletFits < 0 ? G_MAXINT : tabletFits - 8;
    bool full = false;
    int tileW = 0;  // (all tiles are alike; a hidden one has no width: measured on the first, always shown)
    if (!tabletTiles.empty() && tabletTiles[0].second) {
        gtk_widget_get_preferred_width(tabletTiles[0].second, nullptr, &tileW);
    }
    tileW = std::max(tileW, TABLET_W);
    for (size_t i = 0; i < tabletTiles.size(); i++) {
        auto& [id, w] = tabletTiles[i];
        if (!w) {
            continue;
        }
        int need = tileW + 6 + (i == TABLET_ACTIONS || i == TABLET_FIRST ? 14 : 0);
        if (id == "pen" && colours && colourButton) {
            need += tileW + 6;
        }
        full = full || need > room;
        if (!full) {
            room -= need;
        }
        bool on = (id == "ai-highlighter" && highlighting) || (id == "ask" && asking) ||
                  (id == "touch" && control->getSettings()->getTouchDrawingEnabled()) ||
                  (id == "auto-improve" && stateOf("ai-auto-improve"));
        for (const auto& t: TOOLS) {
            on = on || (!highlighting && !asking && id == t.id && t.type == active);
        }
        GtkStyleContext* sc = gtk_widget_get_style_context(w);
        on ? gtk_style_context_add_class(sc, "xoai-active") : gtk_style_context_remove_class(sc, "xoai-active");
        int& shown = id == "touch" ? touchShown : autoShown;
        if (isSwitch(id) && shown != static_cast<int>(on)) {
            shown = on;
            if (GtkWidget* old = gtk_bin_get_child(GTK_BIN(w))) {
                gtk_widget_destroy(old);
            }
            const char* name = id == "touch" ? "Touch" : "Auto-improve";
            GtkWidget* content = tileContent(id == "touch" ? "xopp-touch-drawing" : "starred-symbolic",
                                             std::string(name) + (on ? " on" : " off"), 30);
            gtk_container_add(GTK_CONTAINER(w), content);
            gtk_widget_show_all(content);
            on ? gtk_style_context_add_class(sc, "xoai-touch-on") : gtk_style_context_remove_class(sc, "xoai-touch-on");
        }
        const bool fits = !full;
        if (gtk_widget_get_visible(w) != fits) {
            gtk_widget_set_visible(w, fits);
        }
    }
    if (colourButton) {
        const bool show = colours && std::any_of(tabletTiles.begin(), tabletTiles.end(), [](const auto& t) {
                              return t.first == "pen" && t.second && gtk_widget_get_visible(t.second);
                          });
        if (gtk_widget_get_visible(colourButton) != show) {
            gtk_widget_set_visible(colourButton, show);
        }
    }
}

void QuickTools::openColours(GtkWidget* anchor) {
    closePopover();
    addCss();
    popover = gtk_popover_new(anchor);
    g_object_add_weak_pointer(G_OBJECT(popover), reinterpret_cast<gpointer*>(&popover));
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "aiQuickColours");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    g_object_set(box, "margin", 10, nullptr);
    addColourTiles(box);
    gtk_container_add(GTK_CONTAINER(popover), box);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
                         g_idle_add(
                                 [](gpointer w) -> gboolean {
                                     if (GTK_IS_WIDGET(w) && !gtk_widget_in_destruction(GTK_WIDGET(w))) {
                                         gtk_widget_destroy(GTK_WIDGET(w));
                                     }
                                     g_object_unref(w);
                                     return G_SOURCE_REMOVE;
                                 },
                                 g_object_ref(p));
                     }),
                     nullptr);
    gtk_widget_show_all(popover);
    gtk_popover_popup(GTK_POPOVER(popover));
}

void QuickTools::reapplyFocus() {
    if (!focused) {
        return;
    }
    if (toolbarColumn) {
        gtk_widget_set_visible(toolbarColumn, FALSE);
    }
    if (GTK_IS_APPLICATION_WINDOW(mainWindow)) {
        gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(mainWindow), FALSE);
    }
    MainWindow* win = control->getWindow();
    for (const auto& w: win->getToolbarWidgets()) {
        if (w && gtk_widget_get_visible(w.get())) {
            if (std::find(hiddenBars.begin(), hiddenBars.end(), w.get()) == hiddenBars.end()) {
                hiddenBars.push_back(w.get());  // (shown by full screen: given back with the rest)
            }
            gtk_widget_set_visible(w.get(), FALSE);
        }
    }
    GVariant* st = g_action_get_state(G_ACTION(control->getActionDatabase()->getAction(Action::SHOW_SIDEBAR).get()));
    const bool sidebar = st && g_variant_get_boolean(st);
    if (st) {
        g_variant_unref(st);
    }
    if (sidebar) {
        sidebarBefore = true;
        win->setSidebarVisible(false);
        control->getActionDatabase()->setActionState(Action::SHOW_SIDEBAR, false);
    }
}

void QuickTools::showModeBar(bool on, std::function<void()> done, std::function<void()> discard) {
    if (!outerBox || on == modeOn) {
        return;
    }
    addCss();
    if (on) {
        modeOnDone = std::move(done);
        modeOnDiscard = std::move(discard);
        if (!modeBar) {
            modeBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_buildable_set_name(GTK_BUILDABLE(modeBar), "aiHighlightBar");
            gtk_style_context_add_class(gtk_widget_get_style_context(modeBar), "xoai-mode-bar");
            g_object_set(modeBar, "margin", 4, nullptr);
            GtkWidget* icon = gtk_image_new_from_icon_name("xopp-ai-highlighter", GTK_ICON_SIZE_DND);
            gtk_image_set_pixel_size(GTK_IMAGE(icon), 36);
            gtk_box_pack_start(GTK_BOX(modeBar), icon, FALSE, FALSE, 6);
            modeLevel = gtk_drawing_area_new();
            gtk_widget_set_size_request(modeLevel, 90, 40);
            gtk_widget_set_valign(modeLevel, GTK_ALIGN_CENTER);
            g_signal_connect(modeLevel, "draw", G_CALLBACK(+[](GtkWidget* w, cairo_t* cr, gpointer d) -> gboolean {
                                 auto* self = static_cast<QuickTools*>(d);
                                 const double h = gtk_widget_get_allocated_height(w);
                                 const size_t n = 15;
                                 for (size_t i = 0; i < n; i++) {
                                     const size_t k = self->modeLevels.size();
                                     const float v = i + k >= n ? self->modeLevels[i + k - n] : 0.0F;
                                     const double bh = 3 + (h - 6) * std::min(1.0, std::sqrt(v / 0.25));
                                     cairo_rectangle(cr, 2 + i * 6.0, (h - bh) / 2, 4, bh);
                                 }
                                 cairo_set_source_rgb(cr, 0.71, 0.48, 1.0);
                                 cairo_fill(cr);
                                 return TRUE;
                             }),
                             this);
            gtk_box_pack_start(GTK_BOX(modeBar), modeLevel, FALSE, FALSE, 0);
            modeInfo = gtk_label_new("");
            gtk_style_context_add_class(gtk_widget_get_style_context(modeInfo), "xoai-mode-info");
            gtk_label_set_ellipsize(GTK_LABEL(modeInfo), PANGO_ELLIPSIZE_END);
            gtk_box_pack_start(GTK_BOX(modeBar), modeInfo, TRUE, TRUE, 0);
            gtk_widget_set_halign(modeInfo, GTK_ALIGN_START);
            modeDone = gtk_button_new_with_label("Done");
            gtk_style_context_add_class(gtk_widget_get_style_context(modeDone), "xoai-mode-done");
            gtk_widget_set_size_request(modeDone, 130, -1);
            gtk_widget_set_can_focus(modeDone, FALSE);
            gtk_buildable_set_name(GTK_BUILDABLE(modeDone), "aiHighlightDone");
            gtk_widget_set_tooltip_text(modeDone, "Done: open Ask with what you said (or tap the AI highlighter again)");
            g_signal_connect(modeDone, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                                 auto* self = static_cast<QuickTools*>(d);
                                 if (auto f = self->modeOnDone) {
                                     f();
                                 }
                             }),
                             this);
            gtk_box_pack_end(GTK_BOX(modeBar), modeDone, FALSE, FALSE, 0);
            GtkWidget* discard = gtk_button_new_with_label("Discard");
            gtk_style_context_add_class(gtk_widget_get_style_context(discard), "xoai-mode-discard");
            gtk_widget_set_size_request(discard, 96, -1);
            gtk_widget_set_can_focus(discard, FALSE);
            gtk_buildable_set_name(GTK_BUILDABLE(discard), "aiHighlightDiscard");
            gtk_widget_set_tooltip_text(discard, "Discard the marks and what was said (Escape)");
            g_signal_connect(discard, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) {
                                 auto* self = static_cast<QuickTools*>(d);
                                 if (auto f = self->modeOnDiscard) {
                                     f();
                                 }
                             }),
                             this);
            gtk_box_pack_end(GTK_BOX(modeBar), discard, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(outerBox), modeBar, TRUE, TRUE, 0);
            gtk_widget_show_all(modeBar);
            gtk_widget_set_no_show_all(modeBar, TRUE);
            gtk_widget_hide(modeBar);
            g_object_add_weak_pointer(G_OBJECT(modeBar), reinterpret_cast<gpointer*>(&modeBar));
        }
        // As tall as the strip is now: the page must not move under the pen
        const int h = gtk_widget_get_allocated_height(outerBox);
        gtk_widget_set_size_request(modeBar, -1, std::max(60, h - 8));
        modeHidden.clear();
        GList* kids = gtk_container_get_children(GTK_CONTAINER(outerBox));
        for (GList* k = kids; k; k = k->next) {
            auto* w = GTK_WIDGET(k->data);
            if (w != modeBar) {
                modeHidden.emplace_back(w, gtk_widget_get_visible(w));
                gtk_widget_hide(w);
            }
        }
        g_list_free(kids);
        modeLevels.clear();
        modeOn = true;
        gtk_widget_show(modeBar);
    } else {
        modeOn = false;
        modeOnDone = nullptr;
        modeOnDiscard = nullptr;
        if (modeBar) {
            gtk_widget_hide(modeBar);
        }
        for (auto& [w, shown]: modeHidden) {
            if (GTK_IS_WIDGET(w) && gtk_widget_get_parent(w) == outerBox) {
                gtk_widget_set_visible(w, shown);
            }
        }
        modeHidden.clear();
        refreshTablet();
    }
}

void QuickTools::setModeInfo(const std::string& info, const std::string& doneLabel) {
    if (modeInfo) {
        gtk_label_set_text(GTK_LABEL(modeInfo), info.c_str());
    }
    if (modeDone) {
        gtk_button_set_label(GTK_BUTTON(modeDone), doneLabel.c_str());
    }
}

void QuickTools::pushModeLevel(float rms) {
    modeLevels.push_back(rms);
    if (modeLevels.size() > 30) {
        modeLevels.erase(modeLevels.begin(), modeLevels.begin() + static_cast<std::ptrdiff_t>(modeLevels.size() - 30));
    }
    if (modeLevel && modeOn) {
        gtk_widget_queue_draw(modeLevel);
    }
}

void QuickTools::setFocused(bool on) {
    if (on == focused && focusButton && gtk_bin_get_child(GTK_BIN(focusButton))) {
        return;
    }
    if (on && !focused && GTK_IS_APPLICATION_WINDOW(mainWindow)) {
        menubarBefore = gtk_application_window_get_show_menubar(GTK_APPLICATION_WINDOW(mainWindow));
    }
    const bool changed = on != focused;
    focused = on;
    if (toolbarColumn) {
        gtk_widget_set_visible(toolbarColumn, !on);
    }
    if (tabletBox) {
        gtk_widget_set_visible(tabletBox, on);  // the tablet buttons: in Focus only
        refreshTablet();
    }
    if (changed) {
        // Every other toolbar (left, right, bottom) and the sidebar: hidden, then given back as they were.
        // (The saved settings are not touched: Focus is a view, not a new layout.)
        MainWindow* win = control->getWindow();
        const auto& bars = win->getToolbarWidgets();
        if (on) {
            hiddenBars.clear();
            for (const auto& w: bars) {
                if (w && gtk_widget_get_visible(w.get())) {
                    hiddenBars.push_back(w.get());
                    gtk_widget_set_visible(w.get(), FALSE);
                }
            }
            GVariant* st = g_action_get_state(G_ACTION(control->getActionDatabase()->getAction(Action::SHOW_SIDEBAR).get()));
            sidebarBefore = st && g_variant_get_boolean(st);
            if (st) {
                g_variant_unref(st);
            }
            if (sidebarBefore) {
                win->setSidebarVisible(false);
                control->getActionDatabase()->setActionState(Action::SHOW_SIDEBAR, false);
            }
        } else {
            for (GtkWidget* w: hiddenBars) {
                for (const auto& b: bars) {
                    if (b.get() == w) {  // (still one of the window's toolbars)
                        gtk_widget_set_visible(w, TRUE);
                    }
                }
            }
            hiddenBars.clear();
            if (sidebarBefore) {
                win->setSidebarVisible(true);
                control->getActionDatabase()->setActionState(Action::SHOW_SIDEBAR, true);
            }
        }
    }
    if (changed && GTK_IS_APPLICATION_WINDOW(mainWindow)) {
        gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(mainWindow), on ? FALSE : menubarBefore);
    }
    if (focusButton) {
        if (GtkWidget* old = gtk_bin_get_child(GTK_BIN(focusButton))) {
            gtk_widget_destroy(old);
        }
        GtkWidget* content = on ? tileContent("view-reveal-symbolic", "Menus", 34) :
                                  tileContent("view-conceal-symbolic", "Focus", 34);
        gtk_container_add(GTK_CONTAINER(focusButton), content);
        gtk_widget_show_all(content);
        gtk_widget_set_tooltip_text(focusButton, on ? "Show the menus and toolbars again (F9)" :
                                                      "Focus (F9): hide the menus and toolbars, keep these buttons");
        GtkStyleContext* sc = gtk_widget_get_style_context(focusButton);
        on ? gtk_style_context_add_class(sc, "xoai-focused") : gtk_style_context_remove_class(sc, "xoai-focused");
    }
    if (changed) {
        count("");  // (saves the choice)
    }
}

void QuickTools::setFocusVisible(bool visible) {
    wantFocusButton = visible;
    if (focusButton) {
        gtk_widget_set_visible(focusButton, visible);
    }
    count("");
}

void QuickTools::setFullscreenVisible(bool visible) {
    wantFullscreen = visible;
    if (fullscreenButton) {
        gtk_widget_set_visible(fullscreenButton, visible);
    }
    count("");  // (saves the choice)
}

void QuickTools::setRecordVisible(bool visible) {
    wantRecord = visible;
    if (recordButton) {
        gtk_widget_set_visible(recordButton, visible);
    }
    count("");  // (saves the choice)
}

void QuickTools::setButtonVisible(bool visible) {
    wantButton = visible;
    if (button) {
        gtk_widget_set_visible(button, visible);
    }
    count("");  // (saves the choice)
}

bool QuickTools::isButtonVisible() const { return button && gtk_widget_get_visible(button); }

}  // namespace xoj::assistant
