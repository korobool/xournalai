// Tools: ui_windows, ui_inspect, ui_screenshot

#include "api/AgentGate.h"
#include "api/UiAutomation.h"  // for ui
#include "control/Control.h"   // for Control
#include "gui/MainWindow.h"    // for MainWindow
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"
#include "UiCommon.h"

namespace xoj::mcp::tools {

void refuseUserOnly(GtkWidget* w) {
    GtkWidget* top = w ? gtk_widget_get_toplevel(w) : nullptr;
    if (top && GTK_IS_BUILDABLE(top)) {
        const char* name = gtk_buildable_get_name(GTK_BUILDABLE(top));
        if (name && api::AgentGate::userOnlyWindow(name)) {
            throw ToolError("This dialog (AI agent settings / connecting agents) is for the user only; agents "
                            "cannot operate it");
        }
    }
}

GtkWidget* resolveWidget(Control* ctrl, const Args& args, const std::string& key, bool defaultToFocused) {
    if (args.has(key)) {
        GtkWidget* w = api::ui::lookup(args.str(key));
        if (!w) {
            throw ToolError("Unknown or closed widget/window '" + args.str(key) +
                            "' (ids come from ui_windows / "
                            "ui_inspect; call them again)");
        }
        refuseUserOnly(w);
        return w;
    }
    if (!defaultToFocused) {
        throw ToolError("'" + key + "' is required");
    }
    auto wins = api::ui::windows(ctrl->getWindow()->getWindow());
    for (const auto& w: wins) {
        if (w.focused) {
            refuseUserOnly(w.window);
            return w.window;
        }
    }
    GtkWidget* w = wins.empty() ? ctrl->getWindow()->getWindow() : wins.front().window;
    refuseUserOnly(w);
    return w;
}

json widgetJson(const api::ui::WidgetInfo& w) {
    json j = {{"id", api::ui::idOf(w.widget)},
              {"role", w.role},
              {"type", w.type},
              {"depth", w.depth},
              {"bbox", {w.x, w.y, w.width, w.height}}};
    if (!w.label.empty()) {
        j["label"] = w.label;
    }
    if (!w.name.empty()) {
        j["name"] = w.name;
    }
    if (!w.tooltip.empty()) {
        j["tooltip"] = w.tooltip;
    }
    if (w.value) {
        j["value"] = *w.value;
    }
    if (!w.sensitive) {
        j["enabled"] = false;
    }
    if (!w.visible) {
        j["visible"] = false;
    }
    return j;
}

json windowsJson(Control* ctrl) {
    json list = json::array();
    for (const auto& w: api::ui::windows(ctrl->getWindow()->getWindow())) {
        GtkAllocation a;
        gtk_widget_get_allocation(w.window, &a);
        list.push_back({{"id", api::ui::idOf(w.window)},
                        {"title", w.title},
                        {"kind", w.kind},
                        {"modal", w.modal},
                        {"focused", w.focused},
                        {"size", {a.width, a.height}}});
    }
    return list;
}

void registerUiTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec wins;
    wins.name = "ui_windows";
    wins.title = "Open windows";
    wins.description = "Lists the app's open windows: the main window, dialogs (e.g. after an action opened one), file "
                       "choosers and open menus, each with an id for ui_inspect / ui_screenshot / ui_interact.";
    wins.inputSchema = schema::object({});
    wins.readOnly = true;
    wins.tier = Tier::Ui;
    wins.handler = [ctrl](const json&) {
        requireDocument(ctrl);
        return ToolResult::structured({{"windows", windowsJson(ctrl)}});
    };
    server.getRegistry().addTool(std::move(wins));

    ToolSpec insp;
    insp.name = "ui_inspect";
    insp.title = "Inspect UI";
    insp.description =
            "The widget tree of a window (default: the focused one) or of one widget: each visible widget with id, "
            "role (button, check, radio, toggle, entry, spin, scale, combo, list, tabs, menu_item, label, ...), "
            "label, glade name, current value, enabled flag and bbox (pixels in its window). Use the ids with "
            "ui_interact. max_depth limits the depth; all=true includes hidden widgets and plain containers.";
    insp.inputSchema =
            schema::object({{"target", schema::string("Window or widget id (default: focused window)")},
                            {"max_depth", schema::withDefault(schema::integer("Depth limit"), 25)},
                            {"all", schema::withDefault(schema::boolean("Include hidden/layout widgets"), false)},
                            {"filter", schema::string("Only widgets whose label/name/tooltip/role "
                                                      "contains this")}});
    insp.readOnly = true;
    insp.tier = Tier::Ui;
    insp.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"target", "max_depth", "all", "filter"});
        GtkWidget* root = resolveWidget(ctrl, args, "target", true);
        auto list = api::ui::inspect(root, static_cast<int>(args.integer("max_depth", 25, 0, 200)),
                                     !args.boolean("all", false));
        std::string filter = args.str("filter", "");
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        json out = json::array();
        for (const auto& w: list) {
            if (!filter.empty()) {
                std::string hay = w.label + " " + w.name + " " + w.tooltip + " " + w.role;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if (hay.find(filter) == std::string::npos) {
                    continue;
                }
            }
            out.push_back(widgetJson(w));
            if (out.size() >= 800) {
                break;
            }
        }
        return ToolResult::structured({{"root", api::ui::idOf(root)}, {"count", out.size()}, {"widgets", out}});
    };
    server.getRegistry().addTool(std::move(insp));

    ToolSpec shot;
    shot.name = "ui_screenshot";
    shot.title = "Screenshot UI";
    shot.description = "A PNG of a window (default: the focused one) or widget, to see the interface as the user does.";
    shot.inputSchema = schema::object({{"target", schema::string("Window or widget id")}});
    shot.readOnly = true;
    shot.tier = Tier::Ui;
    shot.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"target"});
        GtkWidget* w = resolveWidget(ctrl, args, "target", true);
        const std::string png = api::ui::screenshot(w);
        ToolResult r = ToolResult::structured({{"target", api::ui::idOf(w)}});
        r.addImage(base64Encode(png), "image/png");
        (void)srv;
        return r;
    };
    server.getRegistry().addTool(std::move(shot));
}

}  // namespace xoj::mcp::tools
