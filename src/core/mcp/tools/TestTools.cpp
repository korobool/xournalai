// Test hooks: registered only when XOURNALAI_TEST_HOOKS=1 (the integration tests set it). Never in normal use.
//
// test_touch feeds synthetic touchscreen events into the canvas through GTK's normal event path (including the
// scrolled window's capture-phase gestures), so pinch zoom can be tested on a display without a touchscreen.

#include <map>  // for map

#include <gtk/gtk.h>

#include "assistant/SpeechToText.h"          // for SpeechToText
#include "control/Control.h"                 // for Control
#include "control/RecordingObserver.h"       // for recordingObserver
#include "control/settings/Settings.h"       // for Settings
#include "control/settings/SettingsEnums.h"  // for InputDeviceTypeOption
#include "control/zoom/ZoomControl.h"        // for ZoomControl
#include "gui/MainWindow.h"                  // for MainWindow
#include "gui/RecordingIndicator.h"          // for RecordingIndicator
#include "gui/XournalView.h"                 // for XournalView
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"  // for McpUi
#include "mcp/Schema.h"
#include "util/StallWatch.h"  // for stall::Activity

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {
/// A slave pointer device to attribute synthetic events to, mapped to `kind` in the settings
GdkDevice* testDevice(Control* ctrl, InputDeviceTypeOption kind) {
    GdkSeat* seat = gdk_display_get_default_seat(gdk_display_get_default());
    GList* slaves = gdk_seat_get_slaves(seat, GDK_SEAT_CAPABILITY_ALL_POINTING);
    GdkDevice* device = slaves ? GDK_DEVICE(slaves->data) : gdk_seat_get_pointer(seat);
    g_list_free(slaves);
    Settings* settings = ctrl->getSettings();
    if (settings->getDeviceClassForDevice(device) != kind) {
        settings->setDeviceClassForDevice(device, kind);
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
        GdkDevice* device = testDevice(ctrl, InputDeviceTypeOption::Touchscreen);
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

    ToolSpec pen;
    pen.name = "test_pen";
    pen.title = "Test hook: the pen";
    pen.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): a synthetic pen event on the canvas: op=hover | "
                      "barrel_down | barrel_up (the first barrel button) | tip_down | move | tip_up, at (x, y) in "
                      "canvas widget coordinates.";
    pen.inputSchema = schema::object({{"op", schema::enumeration("Pen event", {"hover", "barrel_down", "barrel_up",
                                                                               "tip_down", "move", "tip_up"})},
                                      {"x", schema::number("x in canvas widget coordinates")},
                                      {"y", schema::number("y in canvas widget coordinates")}},
                                     {"op", "x", "y"});
    pen.tier = Tier::Ui;
    pen.handler = [ctrl](const json& j) {
        Args args(j);
        args.rejectUnknown({"op", "x", "y"});
        static bool barrel = false, tip = false;
        const std::string op =
                args.choice("op", {"hover", "barrel_down", "barrel_up", "tip_down", "move", "tip_up"}, "");
        const double x = args.number("x"), y = args.number("y");
        GtkWidget* widget = ctrl->getWindow()->getXournal()->getWidget();
        GdkWindow* window = gtk_widget_get_window(widget);
        if (!window) {
            throw ToolError("The canvas is not realized");
        }
        GdkDevice* device = testDevice(ctrl, InputDeviceTypeOption::Pen);
        auto state = [&]() {
            return static_cast<guint>((tip ? GDK_BUTTON1_MASK : 0) | (barrel ? GDK_BUTTON2_MASK : 0));
        };
        GdkEvent* ev = nullptr;
        if (op == "hover" || op == "move") {
            ev = gdk_event_new(GDK_MOTION_NOTIFY);
            ev->motion.x = x;
            ev->motion.y = y;
            ev->motion.state = state();
            ev->motion.time = static_cast<guint32>(g_get_monotonic_time() / 1000);
        } else {
            const bool press = op == "barrel_down" || op == "tip_down";
            const guint button = (op == "barrel_down" || op == "barrel_up") ? 2 : 1;
            ev = gdk_event_new(press ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
            ev->button.x = x;
            ev->button.y = y;
            ev->button.button = button;
            ev->button.state = state();  // before this event, as GDK reports it
            ev->button.time = static_cast<guint32>(g_get_monotonic_time() / 1000);
            (button == 2 ? barrel : tip) = press;
        }
        ev->any.window = GDK_WINDOW(g_object_ref(window));
        gdk_event_set_device(ev, gdk_seat_get_pointer(gdk_device_get_seat(device)));
        gdk_event_set_source_device(ev, device);
        gtk_main_do_event(ev);
        gdk_event_free(ev);
        return ToolResult::structured({{"barrel", barrel}, {"tip", tip}});
    };
    server.getRegistry().addTool(std::move(pen));

    McpServer* srv = &server;
    ToolSpec speech;
    speech.name = "test_speech";
    speech.title = "Test hook: speech to text";
    speech.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): op=start begins listening, op=stop returns the "
                         "transcript (with XOURNALAI_STT_FAKE_MIC the helper 'hears' that WAV file).";
    speech.inputSchema = schema::object({{"op", schema::enumeration("start | stop", {"start", "stop"})}}, {"op"});
    speech.tier = Tier::Ui;
    speech.asyncHandler = [srv](const json& j, Responder respond) {
        Args args(j);
        auto* sp = srv->getUi() ? srv->getUi()->speech() : nullptr;
        if (!sp) {
            throw ToolError("speech is off");
        }
        if (args.choice("op", {"start", "stop"}, "") == "start") {
            sp->start();
            respond(ToolResult::structured({{"state", assistant::SpeechToText::name(sp->state())}}));
            return;
        }
        sp->stop([respond](const std::string& text, bool silent, const std::string& error) {
            respond(ToolResult::structured({{"text", text}, {"silent", silent}, {"error", error}}));
        });
    };
    server.getRegistry().addTool(std::move(speech));

    ToolSpec rec;
    rec.name = "test_recording";
    rec.title = "Test hook: a recording ended";
    rec.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): as if the Xournal++ recorder stopped: file (full "
                      "path), name (the file name strokes refer to), duration_ms.";
    rec.inputSchema = schema::object({{"file", schema::string("the recording")},
                                      {"name", schema::string("its file name")},
                                      {"duration_ms", schema::integer("length")}},
                                     {"file", "name", "duration_ms"});
    rec.tier = Tier::Ui;
    rec.handler = [srv](const json& j) {
        Args args(j);
        // Through the recorder's observer, like the real thing (nobody listens if sharing recordings is off)
        const auto observer = xoj::audio::recordingObserver();
        if (observer) {
            observer({fs::path(args.str("file")), args.str("name"), args.integer("duration_ms")});
        }
        return ToolResult::structured({{"observed", static_cast<bool>(observer)}});
    };
    server.getRegistry().addTool(std::move(rec));

    ToolSpec recOn;
    recOn.name = "test_recorder";
    recOn.title = "Test hook: the recorder is on or off";
    recOn.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): shows or hides the recording indicator as if the "
                        "Xournal++ recorder started (on=true) or stopped, without opening the microphone.";
    recOn.inputSchema = schema::object({{"on", schema::boolean("recording")}}, {"on"});
    recOn.tier = Tier::Ui;
    recOn.handler = [srv](const json& j) {
        Args args(j);
        auto* ind = srv->getControl()->getRecordingIndicator();
        if (!ind) {
            throw ToolError("no recording indicator (audio is disabled)");
        }
        ind->setRecording(args.boolean("on", false));
        return ToolResult::structured({{"recording", ind->isRecording()}});
    };
    server.getRegistry().addTool(std::move(recOn));

    ToolSpec connect;
    connect.name = "test_connect_dialog";
    connect.title = "Test hook: open Connect an Agent";
    connect.description =
            "Test hook (XOURNALAI_TEST_HOOKS=1 only): opens the Connect an Agent dialog, as the user would "
            "from the AI Agent menu (agents themselves cannot open or read it).";
    connect.inputSchema = schema::object({}, {});
    connect.tier = Tier::Ui;
    connect.handler = [srv](const json&) {
        if (auto* ui = srv->getUi()) {
            ui->showConnect();
        }
        return ToolResult::structured({{"opened", srv->getUi() != nullptr}});
    };
    server.getRegistry().addTool(std::move(connect));
}

}  // namespace xoj::mcp::tools
