// Tools: actions_list, action_run

#include <map>  // for map
#include <set>  // for set

#include <gtk/gtk.h>

#include "api/Menus.h"        // for walkMenu
#include "control/Control.h"  // for Control
#include "gui/MainWindow.h"   // for MainWindow
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "mcp/Variant.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

/// Actions that could end the session or lose work without an undo step
const std::set<std::string>& destructiveActions() {
    static const std::set<std::string> s = {"win.quit", "app.quit", "quit"};
    return s;
}

struct Group {
    std::string prefix;
    GActionGroup* group;
};

std::vector<Group> actionGroups(Control* ctrl) {
    std::vector<Group> out;
    GtkWidget* win = ctrl->getWindow()->getWindow();
    out.push_back({"win", G_ACTION_GROUP(win)});
    if (GtkApplication* app = gtk_window_get_application(GTK_WINDOW(win))) {
        out.push_back({"app", G_ACTION_GROUP(app)});
    }
    return out;
}

/// "open", "win.open" or "app.quit" → group + bare name
std::pair<GActionGroup*, std::string> findAction(Control* ctrl, const std::string& name) {
    std::string prefix, bare = name;
    if (auto dot = name.find('.'); dot != std::string::npos) {
        prefix = name.substr(0, dot);
        bare = name.substr(dot + 1);
    }
    for (const auto& g: actionGroups(ctrl)) {
        if ((prefix.empty() || prefix == g.prefix) && g_action_group_has_action(g.group, bare.c_str())) {
            return {g.group, bare};
        }
    }
    throw ToolError("Unknown action '" + name + "'. Use actions_list to see all actions");
}

}  // namespace

void registerActionTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec list;
    list.name = "actions_list";
    list.title = "List actions";
    list.description =
            "Lists every application action (everything in menus, toolbars and shortcuts): name (e.g. "
            "\"win.zoom-in\"), menu label/path, enabled, parameter type and current state (for toggles and choices). "
            "Run them with action_run. Use 'filter' to search names and labels.";
    list.inputSchema = schema::object({{"filter", schema::string("Only actions whose name or label contains this")}});
    list.readOnly = true;
    list.tier = Tier::Ui;
    list.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"filter"});
        std::string filter = args.str("filter", "");
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        std::map<std::string, std::string> labels;  // action → menu path
        for (const auto& e: api::walkMenu(ctrl->getWindow()->getMenuModel())) {
            if (!e.action.empty() && !labels.count(e.action)) {
                labels[e.action] = e.path;
            }
        }
        json out = json::array();
        for (const auto& g: actionGroups(ctrl)) {
            gchar** names = g_action_group_list_actions(g.group);
            for (gchar** n = names; n && *n; n++) {
                const std::string full = g.prefix + "." + *n;
                std::string menu = labels.count(full) ? labels[full] : "";
                std::string hay = full + " " + menu;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if (!filter.empty() && hay.find(filter) == std::string::npos) {
                    continue;
                }
                json a = {{"name", full},
                          {"enabled", static_cast<bool>(g_action_group_get_action_enabled(g.group, *n))}};
                if (!menu.empty()) {
                    a["menu"] = menu;
                }
                if (const GVariantType* pt = g_action_group_get_action_parameter_type(g.group, *n)) {
                    a["parameter_type"] =
                            std::string(g_variant_type_peek_string(pt), g_variant_type_get_string_length(pt));
                }
                if (GVariant* st = g_action_group_get_action_state(g.group, *n)) {
                    a["state"] = variantToJson(st);
                    g_variant_unref(st);
                }
                out.push_back(std::move(a));
            }
            g_strfreev(names);
        }
        std::sort(out.begin(), out.end(), [](const json& a, const json& b) { return a["name"] < b["name"]; });
        return ToolResult::structured({{"count", out.size()}, {"actions", out}});
    };
    server.getRegistry().addTool(std::move(list));

    ToolSpec run;
    run.name = "action_run";
    run.title = "Run action";
    run.description =
            "Runs an application action by name (from actions_list), like choosing it in a menu: e.g. "
            "\"win.zoom-in\", \"win.new-page-after\", \"win.layer-new-above-current\", \"win.fullscreen\". For "
            "actions with a parameter give 'parameter' (e.g. win.zoom with 1.5; win.select-tool with a tool "
            "number); for toggles/choices you can set 'state' directly. Some actions open dialogs: see ui_windows "
            "to operate them. Quitting needs the 'destructive' permission.";
    run.inputSchema = schema::object(
            {{"action", schema::string("Action name, e.g. \"win.zoom-in\" (the \"win.\" prefix is optional)")},
             {"parameter", schema::string("Parameter value (number, true/false or text; any GVariant text works)")},
             {"state", schema::string("New state for stateful actions (instead of activating)")}},
            {"action"});
    run.tier = Tier::Ui;
    run.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"action", "parameter", "state"});
        const std::string name = args.str("action");
        auto [group, bare] = findAction(ctrl, name);
        if (destructiveActions().count(name) || destructiveActions().count("win." + bare)) {
            srv->requireTier(Tier::Destructive, "action '" + name + "'");
        }
        if (!g_action_group_get_action_enabled(group, bare.c_str())) {
            throw ToolError("Action '" + name + "' is currently disabled (e.g. nothing selected, nothing to undo)");
        }
        if (args.has("state")) {
            GVariant* current = g_action_group_get_action_state(group, bare.c_str());
            if (!current) {
                throw ToolError("Action '" + name + "' has no state; call it without 'state'");
            }
            GVariant* v = jsonToVariant(args.raw("state"), g_variant_get_type(current));
            g_variant_unref(current);
            g_action_group_change_action_state(group, bare.c_str(), v);
        } else {
            const GVariantType* pt = g_action_group_get_action_parameter_type(group, bare.c_str());
            if (pt && !args.has("parameter")) {
                throw ToolError("Action '" + name + "' needs a 'parameter' of type '" +
                                std::string(g_variant_type_peek_string(pt), g_variant_type_get_string_length(pt)) +
                                "'");
            }
            GVariant* param = pt ? jsonToVariant(args.raw("parameter"), pt) : nullptr;
            g_action_group_activate_action(group, bare.c_str(), param);
        }
        json out = {{"ran", name}};
        if (GVariant* st = g_action_group_get_action_state(group, bare.c_str())) {
            out["state"] = variantToJson(st);
            g_variant_unref(st);
        }
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(run));
}

}  // namespace xoj::mcp::tools
