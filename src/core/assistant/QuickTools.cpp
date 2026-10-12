#include "QuickTools.h"

#include <algorithm>  // for stable_sort
#include <fstream>    // for ifstream, ofstream

#include "control/Control.h"                 // for Control
#include "control/ToolEnums.h"               // for ToolType, ToolSize
#include "control/ToolHandler.h"             // for ToolHandler
#include "control/actions/ActionDatabase.h"  // for ActionDatabase
#include "gui/MainWindow.h"                  // for MainWindow
#include "gui/XournalView.h"                 // for XournalView
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
    uses[id]++;
    quick_tools::save(file, uses);
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

void QuickTools::open(GtkWidget* anchor) {
    closePopover();
    GtkWidget* relative = anchor;
    GdkRectangle point{0, 0, 1, 1};
    if (!relative || !gtk_widget_get_mapped(relative)) {
        relative = control->getWindow()->getXournal()->getWidget();
        point = {gtk_widget_get_allocated_width(relative) / 2, gtk_widget_get_allocated_height(relative) / 3, 1, 1};
    }
    popover = gtk_popover_new(relative);
    g_object_add_weak_pointer(G_OBJECT(popover), reinterpret_cast<gpointer*>(&popover));
    if (relative != anchor) {
        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &point);
    }
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "aiQuickTools");

    static bool cssAdded = false;
    if (!cssAdded) {
        cssAdded = true;
        GtkCssProvider* css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(css,
                                        ".xoai-quick-tile { padding: 6px 4px; min-width: 84px; min-height: 76px; }"
                                        ".xoai-quick-tile label { font-size: 0.92em; }",
                                        -1, nullptr);
        gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(css);
    }

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    g_object_set(box, "margin", 8, nullptr);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    const auto ids = quick_tools::ranked(uses);
    const ToolType active = control->getToolHandler()->getToolType();
    for (size_t i = 0; i < ids.size(); i++) {
        const auto& cat = quick_tools::catalog();
        const auto it = std::find_if(cat.begin(), cat.end(), [&](const auto& e) { return ids[i] == e.id; });
        std::string label = it->label;
        if (ids[i] == "size" && control->getToolHandler()->hasCapability(TOOL_CAP_SIZE)) {
            label = std::string("Size: ") + sizeName(control->getToolHandler()->getActiveTool()->getSize());
        }
        GtkWidget* tile = gtk_button_new();
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        GtkWidget* image = gtk_image_new_from_icon_name(it->icon, GTK_ICON_SIZE_DND);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 36);
        gtk_box_pack_start(GTK_BOX(inner), image, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(inner), gtk_label_new(label.c_str()), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(tile), inner);
        gtk_style_context_add_class(gtk_widget_get_style_context(tile), "xoai-quick-tile");
        gtk_widget_set_tooltip_text(tile, it->tip);
        gtk_widget_set_can_focus(tile, FALSE);
        gtk_buildable_set_name(GTK_BUILDABLE(tile), (std::string("aiQuick-") + it->id).c_str());
        for (const auto& t: TOOLS) {
            if (ids[i] == t.id && t.type == active) {
                gtk_style_context_add_class(gtk_widget_get_style_context(tile), "suggested-action");
            }
        }
        g_object_set_data_full(G_OBJECT(tile), "tool", g_strdup(it->id), g_free);
        g_signal_connect(tile, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             auto* self = static_cast<QuickTools*>(d);
                             const std::string id = static_cast<const char*>(g_object_get_data(G_OBJECT(b), "tool"));
                             if (self->popover) {
                                 gtk_popover_popdown(GTK_POPOVER(self->popover));
                             }
                             self->activate(id);
                         }),
                         this);
        gtk_grid_attach(GTK_GRID(grid), tile, static_cast<int>(i % 5), static_cast<int>(i / 5), 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);
    GtkWidget* note = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(note), "<small>Your most used tools first (F8)</small>");
    gtk_widget_set_opacity(note, 0.7);
    gtk_box_pack_start(GTK_BOX(box), note, FALSE, FALSE, 0);
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

}  // namespace xoj::assistant
