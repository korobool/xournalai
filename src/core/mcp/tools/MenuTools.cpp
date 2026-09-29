// Tools: ui_menu_tree, ui_menu_select

#include <algorithm>  // for transform
#include <memory>     // for shared_ptr

#include <gtk/gtk.h>

#include "api/AgentGate.h"     // for AgentGate
#include "api/Menus.h"         // for walkMenu
#include "api/UiAutomation.h"  // for findMenuItem
#include "control/Control.h"   // for Control
#include "gui/MainWindow.h"    // for MainWindow
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "mcp/Variant.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (end == std::string::npos) {
            end = path.size();
        }
        std::string p = path.substr(start, end - start);
        if (!p.empty()) {
            parts.push_back(p);
        }
        start = end + 1;
    }
    return parts;
}

struct ActionRef {
    GActionGroup* group = nullptr;
    std::string name;
};

ActionRef actionFor(Control* ctrl, const std::string& full) {
    ActionRef r;
    const auto dot = full.find('.');
    if (dot == std::string::npos) {
        return r;
    }
    const std::string prefix = full.substr(0, dot);
    r.name = full.substr(dot + 1);
    GtkWidget* win = ctrl->getWindow()->getWindow();
    if (prefix == "win") {
        r.group = G_ACTION_GROUP(win);
    } else if (prefix == "app") {
        r.group = G_ACTION_GROUP(gtk_window_get_application(GTK_WINDOW(win)));
    }
    if (r.group && !g_action_group_has_action(r.group, r.name.c_str())) {
        r.group = nullptr;
    }
    return r;
}

/// Visible navigation state: open each menu level with a pause, then activate
struct Walk {
    Control* ctrl;
    GtkWidget* menubar;
    std::vector<std::string> labels;
    size_t level = 0;
    GtkWidget* shell = nullptr;
    ActionRef action;
    std::shared_ptr<GVariant> target;
    Responder respond;
    std::string path;
};

gboolean walkStep(gpointer data) {
    auto* w = static_cast<Walk*>(data);
    auto fail = [w](const std::string& msg) {
        gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->menubar));
        w->respond(ToolResult::error(msg));
        delete w;
        return G_SOURCE_REMOVE;
    };
    if (w->level < w->labels.size()) {
        GtkWidget* item = api::ui::findMenuItem(w->shell, w->labels[w->level]);
        if (!item) {
            return fail("Menu item '" + w->labels[w->level] + "' not found on screen (menu path '" + w->path + "')");
        }
        gtk_menu_shell_select_item(GTK_MENU_SHELL(w->shell), item);  // highlights it / opens its submenu
        if (GtkWidget* sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item))) {
            w->shell = sub;
        }
        w->level++;
        return G_SOURCE_CONTINUE;
    }
    gtk_menu_shell_deactivate(GTK_MENU_SHELL(w->menubar));
    g_action_group_activate_action(w->action.group, w->action.name.c_str(), w->target.get());
    json out = {{"selected", w->path}, {"action", w->action.name}, {"visible", true}};
    w->respond(ToolResult::structured(std::move(out)));
    delete w;
    return G_SOURCE_REMOVE;
}

}  // namespace

void registerMenuTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec tree;
    tree.name = "ui_menu_tree";
    tree.title = "Menu tree";
    tree.description =
            "The complete main menu: every entry with its path (e.g. \"File/Export as PDF\"), action, keyboard "
            "shortcut, enabled flag and check/radio state, including dynamic menus (recent documents, plugins, "
            "page types). Choose entries with ui_menu_select.";
    tree.inputSchema = schema::object({{"filter", schema::string("Only entries whose path contains this")}});
    tree.readOnly = true;
    tree.tier = Tier::Ui;
    tree.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"filter"});
        const std::string filter = lower(args.str("filter", ""));
        json out = json::array();
        for (const auto& e: api::walkMenu(ctrl->getWindow()->getMenuModel())) {
            if (!filter.empty() && lower(e.path).find(filter) == std::string::npos) {
                continue;
            }
            json item = {{"path", e.path}, {"depth", e.depth}};
            if (e.submenu) {
                item["submenu"] = true;
            }
            if (!e.action.empty()) {
                item["action"] = e.action;
                ActionRef a = actionFor(ctrl, e.action);
                if (a.group) {
                    item["enabled"] = static_cast<bool>(g_action_group_get_action_enabled(a.group, a.name.c_str()));
                    if (GVariant* st = g_action_group_get_action_state(a.group, a.name.c_str())) {
                        // radio items: checked if the state equals the item's target; checkboxes: boolean state
                        item["checked"] = e.target ? json(static_cast<bool>(g_variant_equal(st, e.target.get()))) :
                                                     json(variantToJson(st));
                        g_variant_unref(st);
                    }
                }
                if (e.target) {
                    item["target"] = variantToJson(e.target.get());
                }
            }
            if (!e.accel.empty()) {
                item["shortcut"] = e.accel;
            }
            out.push_back(std::move(item));
        }
        return ToolResult::structured({{"count", out.size()}, {"entries", out}});
    };
    server.getRegistry().addTool(std::move(tree));

    ToolSpec select;
    select.name = "ui_menu_select";
    select.title = "Choose a menu entry";
    select.description =
            "Chooses a main-menu entry by its path, e.g. \"File/Export as PDF\" or \"View/Zoom/Zoom In\" (case "
            "insensitive; see ui_menu_tree). With visible=true (default) the menu really opens and each level is "
            "highlighted so the user can follow; then the entry is activated. Entries that open dialogs leave the "
            "dialog open: continue with ui_windows / ui_interact.";
    select.inputSchema = schema::object(
            {{"path", schema::string("Menu path, levels separated by '/'")},
             {"visible", schema::withDefault(schema::boolean("Show the navigation in the real menu"), true)},
             {"step_ms", schema::withDefault(schema::integer("Pause per menu level when visible"), 350)}},
            {"path"});
    select.tier = Tier::Ui;
    select.asyncHandler = [ctrl, &server](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"path", "visible", "step_ms"});
        const std::string path = args.str("path");
        const auto entries = api::walkMenu(ctrl->getWindow()->getMenuModel());
        const api::MenuEntry* entry = nullptr;
        for (const auto& e: entries) {
            if (lower(e.path) == lower(path)) {
                entry = &e;
                break;
            }
        }
        if (!entry) {  // unique suffix match, e.g. "Export as PDF"
            for (const auto& e: entries) {
                const std::string lp = lower(e.path), lq = lower(path);
                if (lp.size() >= lq.size() && lp.compare(lp.size() - lq.size(), lq.size(), lq) == 0 &&
                    (lp.size() == lq.size() || lp[lp.size() - lq.size() - 1] == '/')) {
                    if (entry) {
                        throw ToolError("Menu path '" + path + "' is ambiguous; give the full path");
                    }
                    entry = &e;
                }
            }
        }
        if (!entry) {
            throw ToolError("No menu entry '" + path + "'. Use ui_menu_tree to see all entries");
        }
        if (entry->submenu || entry->action.empty()) {
            throw ToolError("'" + entry->path + "' is a submenu; choose one of its entries");
        }
        ActionRef action = actionFor(ctrl, entry->action);
        if (!action.group) {
            throw ToolError("The entry's action '" + entry->action + "' is not available");
        }
        if (api::AgentGate::userOnlyAction(action.name)) {
            throw ToolError("'" + entry->path + "' is for the user only");
        }
        if (action.name == "quit") {
            server.requireTier(Tier::Destructive, "quitting the application");
        }
        if (!g_action_group_get_action_enabled(action.group, action.name.c_str())) {
            throw ToolError("'" + entry->path + "' is disabled right now");
        }
        GtkWidget* menubar = api::ui::findDescendant(ctrl->getWindow()->getWindow(), GTK_TYPE_MENU_BAR);
        if (!args.boolean("visible", true) || !menubar || !gtk_widget_get_visible(menubar)) {
            g_action_group_activate_action(action.group, action.name.c_str(), entry->target.get());
            respond(ToolResult::structured({{"selected", entry->path}, {"action", entry->action}, {"visible", false}}));
            return;
        }
        auto* walk = new Walk{ctrl,    menubar,    splitPath(entry->path), 0, menubar, action, entry->target,
                              respond, entry->path};
        g_timeout_add(static_cast<guint>(args.integer("step_ms", 350, 0, 3000)), walkStep, walk);
    };
    server.getRegistry().addTool(std::move(select));
}

}  // namespace xoj::mcp::tools
