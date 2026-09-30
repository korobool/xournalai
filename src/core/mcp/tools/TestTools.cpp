// Test hooks: registered only when XOURNALAI_TEST_HOOKS=1 (the integration tests set it). Never in normal use.
//
// test_touch feeds synthetic touchscreen events into the canvas through GTK's normal event path (including the
// scrolled window's capture-phase gestures), so pinch zoom can be tested on a display without a touchscreen.

#include <map>  // for map

#include <gtk/gtk.h>

#include "control/Control.h"                 // for Control
#include "control/settings/Settings.h"       // for Settings
#include "control/settings/SettingsEnums.h"  // for InputDeviceTypeOption
#include "control/zoom/ZoomControl.h"        // for ZoomControl
#include "gui/MainWindow.h"                  // for MainWindow
#include "gui/XournalView.h"                 // for XournalView
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "util/StallWatch.h"  // for stall::Activity

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {
/// A slave pointer device to attribute the touches to (mapped to "touchscreen" in the settings on first use)
GdkDevice* touchDevice(Control* ctrl) {
    GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
    GList* slaves = gdk_seat_get_slaves(seat, GDK_SEAT_CAPABILITY_ALL_POINTING);
    GdkDevice* device = slaves ? GDK_DEVICE(slaves->data) : gdk_seat_get_pointer(seat);
    g_list_free(slaves);
    Settings* settings = ctrl->getSettings();
    if (settings->getDeviceClassForDevice(device) != InputDeviceTypeOption::Touchscreen) {
        settings->setDeviceClassForDevice(device, InputDeviceTypeOption::Touchscreen);
    }
    return device;
}
}  // namespace

void registerTestTools(McpServer& server) {
    const char* hooks = g_getenv("XOURNALAI_TEST_HOOKS");
    if (!hooks || std::string(hooks) != "1") {
        return;
    }
    Control* ctrl = server.getControl();

    ToolSpec t;
    t.name = "test_touch";
    t.title = "Test hook: touch the canvas";
    t.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): a synthetic touchscreen event on the canvas. op=begin | "
                    "update | end for finger N at (x, y) in canvas widget coordinates. Returns the zoom.";
    t.inputSchema = schema::object({{"op", schema::enumeration("Touch phase", {"begin", "update", "end"})},
                                    {"finger", schema::integer("Finger number (0, 1, …)")},
                                    {"x", schema::number("x in canvas widget coordinates")},
                                    {"y", schema::number("y in canvas widget coordinates")}},
                                   {"op", "finger", "x", "y"});
    t.tier = Tier::Ui;
    t.handler = [ctrl](const json& j) {
        Args args(j);
        args.rejectUnknown({"op", "finger", "x", "y"});
        static std::map<int64_t, bool> down;  // fingers currently down
        const std::string op = args.choice("op", {"begin", "update", "end"}, "");
        const int64_t finger = args.integer("finger", 0, 0, 9);
        const double x = args.number("x");
        const double y = args.number("y");

        GtkWidget* widget = ctrl->getWindow()->getXournal()->getWidget();
        GdkWindow* window = gtk_widget_get_window(widget);
        if (!window) {
            throw ToolError("The canvas is not realized");
        }
        GdkDevice* device = touchDevice(ctrl);
        const GdkEventType type = op == "begin" ? GDK_TOUCH_BEGIN : (op == "update" ? GDK_TOUCH_UPDATE : GDK_TOUCH_END);
        if (op == "begin") {
            down[finger] = down.empty();  // the first finger down emulates the pointer (as on X11)
        } else if (!down.count(finger)) {
            throw ToolError("That finger is not down");
        }

        GdkEvent* ev = gdk_event_new(type);
        ev->touch.window = GDK_WINDOW(g_object_ref(window));
        ev->touch.time = static_cast<guint32>(g_get_monotonic_time() / 1000);
        ev->touch.x = x;
        ev->touch.y = y;
        int ox = 0, oy = 0;
        gdk_window_get_origin(window, &ox, &oy);
        ev->touch.x_root = ox + x;
        ev->touch.y_root = oy + y;
        ev->touch.sequence = reinterpret_cast<GdkEventSequence*>(static_cast<guintptr>(1000 + finger));
        ev->touch.emulating_pointer = down[finger];
        gdk_event_set_device(ev, gdk_seat_get_pointer(gdk_device_get_seat(device)));
        gdk_event_set_source_device(ev, device);
        gtk_main_do_event(ev);
        gdk_event_free(ev);

        if (op == "end") {
            down.erase(finger);
        }
        return ToolResult::structured({{"zoom", ctrl->getZoomControl()->getZoomReal()}});
    };
    server.getRegistry().addTool(std::move(t));

    ToolSpec block;
    block.name = "test_block_ui";
    block.title = "Test hook: block the UI thread";
    block.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): keeps the UI thread busy for `ms` milliseconds, "
                        "announced as the activity \"test stall\" (checks the stall watchdog).";
    block.inputSchema = schema::object({{"ms", schema::integer("How long")}}, {"ms"});
    block.tier = Tier::Ui;
    block.handler = [](const json& j) {
        Args args(j);
        const auto ms = args.integer("ms", 100, 1, 5000);
        xoj::util::stall::Activity activity("test stall");
        g_usleep(static_cast<gulong>(ms) * 1000);
        return ToolResult::structured({{"blocked_ms", ms}});
    };
    server.getRegistry().addTool(std::move(block));
}

}  // namespace xoj::mcp::tools
