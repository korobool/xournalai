#include "QuickTools.h"

#include <algorithm>  // for stable_sort
#include <array>      // for array
#include <cmath>      // for sqrt, cos, sin
#include <fstream>    // for ifstream, ofstream

#include "control/Control.h"                 // for Control
#include "control/ToolEnums.h"               // for ToolType, ToolSize
#include "control/ToolHandler.h"             // for ToolHandler
#include "control/actions/ActionDatabase.h"  // for ActionDatabase
#include "gui/MainWindow.h"
#include "gui/RecordingIndicator.h"  // for RecordingIndicator                  // for MainWindow
#include "gui/XournalView.h"         // for XournalView
#include "mcp/Json.h"
#include "util/GVariantTemplate.h"  // for getGVariantValue

namespace xoj::assistant {

namespace quick_tools {

const std::vector<Entry>& catalog() {
    static const std::vector<Entry> all = {
            {"pen", "Pen", "xopp-tool-pencil", "Draw with the pen"},
            {"eraser", "Eraser", "xopp-tool-eraser", "Erase"},
            {"ask", "Ask", "audio-input-microphone-symbolic",
             "Ask the AI: circle an area, then say or type what you want"},
            {"select", "Select", "xopp-select-lasso", "Select a region (lasso)"},
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
    quick_tools::save(file, all);
}

void QuickTools::activate(const std::string& id) {
    if (id != "command") {  // (a command is counted when it is sent)
        count(id);
    }
    performing = true;
    ActionDatabase* db = control->getActionDatabase();
    bool done = false;
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
            {"Size", {"size-0", "size-1", "size-2", "size-3", "size-4"}},
            {"Page", {"undo", "redo", "new-page"}},
            {"AI", {"ask", "command", "terminal"}},
    };
    return all;
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

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    g_object_set(box, "margin", 10, nullptr);
    ToolHandler* th = control->getToolHandler();
    const ToolType active = th->getToolType();
    const auto& cat = quick_tools::catalog();
    for (const auto& g: groups()) {
        GtkWidget* heading = gtk_label_new(g.title);
        gtk_widget_set_halign(heading, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(heading), "xoai-quick-heading");
        gtk_box_pack_start(GTK_BOX(box), heading, FALSE, FALSE, 0);
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_set_homogeneous(GTK_BOX(row), FALSE);
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
        gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);
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
    recordPoll = g_timeout_add(
            200,
            [](gpointer d) -> gboolean {
                auto* self = static_cast<QuickTools*>(d);
                if (self->recordButton && self->isRecording() != self->shownRecording) {
                    self->shownRecording = !self->shownRecording;
                    self->refreshRecord();
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
