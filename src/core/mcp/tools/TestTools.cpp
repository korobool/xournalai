// Test hooks: registered only when XOURNALAI_TEST_HOOKS=1 (the integration tests set it). Never in normal use.
//
// test_touch feeds synthetic touchscreen events into the canvas through GTK's normal event path (including the
// scrolled window's capture-phase gestures), so pinch zoom can be tested on a display without a touchscreen.

#include <csignal>       // for raise, SIGABRT
#include <map>           // for map
#include <shared_mutex>  // for shared_lock

#include <gtk/gtk.h>

#include "assistant/SpeechToText.h"          // for SpeechToText
#include "control/Control.h"                 // for Control
#include "control/CrashHandler.h"            // for simulateStuckCrashHandler
#include "control/RecordingObserver.h"       // for recordingObserver
#include "control/SampleTap.h"               // for tapSamples
#include "control/XHangRecovery.h"           // for provokeRenderError
#include "control/settings/Settings.h"       // for Settings
#include "control/settings/SettingsEnums.h"  // for InputDeviceTypeOption
#include "control/zoom/ZoomControl.h"        // for ZoomControl
#include "gui/MainWindow.h"                  // for MainWindow
#include "gui/RecordingIndicator.h"          // for RecordingIndicator
#include "gui/XournalView.h"                 // for XournalView
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"  // for McpUi
#ifdef ENABLE_AI_TERMINAL
#include "assistant/terminal/TerminalDock.h"  // for TerminalDock
#endif
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document
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

    ToolSpec crash;
    crash.name = "test_crash_stuck";
    crash.title = "Test hook: a crash whose handler hangs";
    crash.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): aborts on the UI thread with the crash handler "
                        "stuck, as after heap corruption (the allocator's lock held). The app must rescue the "
                        "document and end by itself.";
    crash.inputSchema = schema::object({}, {});
    crash.tier = Tier::Ui;
    crash.handler = [](const json&) -> ToolResult {
        simulateStuckCrashHandler();
        std::raise(SIGABRT);
        return ToolResult::structured({});
    };
    server.getRegistry().addTool(std::move(crash));

    ToolSpec xError;
    xError.name = "test_x_error";
    xError.title = "Test hook: an X error";
    xError.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): sends a RENDER request the X server answers with "
                         "an error (BadPicture), as during GTK's drawing on 2026-10-18. The app must log it and live.";
    xError.inputSchema = schema::object({}, {});
    xError.tier = Tier::Ui;
    xError.handler = [](const json&) -> ToolResult {
        if (!provokeRenderError()) {
            throw ToolError("No X11 display with the error hook");
        }
        return ToolResult::structured({{"sent", true}});
    };
    server.getRegistry().addTool(std::move(xError));

    ToolSpec pen;
    pen.name = "test_pen";
    pen.title = "Test hook: the pen";
    pen.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): a synthetic pen event on the canvas: op=hover | "
                      "barrel_down | barrel_up (the first barrel button) | tip_down | move | tip_up | out (leaves the "
                      "tablet's range, as when the driver force-ejects a pen: no button release), at (x, y) in "
                      "canvas widget coordinates.";
    pen.inputSchema = schema::object({{"op", schema::enumeration("Pen event", {"hover", "barrel_down", "barrel_up",
                                                                               "tip_down", "move", "tip_up", "out"})},
                                      {"x", schema::number("x in canvas widget coordinates")},
                                      {"y", schema::number("y in canvas widget coordinates")}},
                                     {"op", "x", "y"});
    pen.tier = Tier::Ui;
    pen.handler = [ctrl](const json& j) {
        Args args(j);
        args.rejectUnknown({"op", "x", "y"});
        static bool barrel = false, tip = false;
        const std::string op =
                args.choice("op", {"hover", "barrel_down", "barrel_up", "tip_down", "move", "tip_up", "out"}, "");
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
        if (op == "out") {
            ev = gdk_event_new(GDK_PROXIMITY_OUT);
            ev->proximity.time = static_cast<guint32>(g_get_monotonic_time() / 1000);
            barrel = tip = false;  // (gone, nothing released)
        } else if (op == "hover" || op == "move") {
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

#ifdef ENABLE_AI_TERMINAL
    ToolSpec termText;
    termText.name = "test_terminal_text";
    termText.title = "Test hook: the text of a terminal tab";
    termText.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): the visible text of the AI terminal's tab with "
                           "this title (default: the serving tab), e.g. to see what a program asks at its start.";
    termText.inputSchema = schema::object({{"title", schema::string("the tab's title")}}, {});
    termText.tier = Tier::Read;
    termText.handler = [srv](const json& j) {
        Args args(j);
        auto* dock = srv->getUi() ? srv->getUi()->terminal() : nullptr;
        if (!dock) {
            throw ToolError("no terminal");
        }
        int tab = dock->findTab(args.str("title", "Claude · serving"));
        return ToolResult::structured(
                {{"tab", tab}, {"running", tab >= 0 && dock->isRunning(tab)}, {"text", tab >= 0 ? dock->text(tab) : ""}});
    };
    server.getRegistry().addTool(std::move(termText));
#endif

    ToolSpec liveAudio;
    liveAudio.name = "test_live_audio";
    liveAudio.title = "Test hook: live notes from a sound file";
    liveAudio.description =
            "Test hook (XOURNALAI_TEST_HOOKS=1 only): op=start (name, mode: a live-notes mode's id, default conspect): "
            "as if that mode's tile of the Record menu started recording `name`; op=feed (file: WAV PCM 16-bit mono): its sound through the recorder's sample tap. End it with "
            "test_recording (the same name), like the recorder.";
    liveAudio.inputSchema = schema::object({{"op", schema::enumeration("Operation", {"start", "feed"})},
                                            {"name", schema::string("start: the recording's file name")},
                                            {"mode", schema::string("start: the mode (text, drawing, conspect, …)")},
                                            {"file", schema::string("feed: a WAV file")}},
                                           {"op"});
    liveAudio.tier = Tier::Ui;
    liveAudio.handler = [srv](const json& j) {
        Args args(j);
        if (args.str("op", "") == "start") {
            std::string error;
            if (!srv->getUi() || !srv->getUi()->beginLive(args.str("name"), error, args.str("mode", ""))) {
                throw ToolError(error.empty() ? "no UI" : error);
            }
            return ToolResult::structured({{"started", true}});
        }
        gchar* data = nullptr;
        gsize size = 0;
        if (!g_file_get_contents(args.str("file").c_str(), &data, &size, nullptr) || size < 44) {
            g_free(data);
            throw ToolError("cannot read the WAV file");
        }
        // (a plain 44-byte header: PCM 16-bit mono at the rate it names)
        const auto* bytes = reinterpret_cast<const unsigned char*>(data);
        const int rate = bytes[24] | (bytes[25] << 8) | (bytes[26] << 16) | (bytes[27] << 24);
        std::vector<float> samples;
        for (gsize i = 44; i + 1 < size; i += 2) {
            samples.push_back(static_cast<float>(static_cast<int16_t>(bytes[i] | (bytes[i + 1] << 8))) / 32768.0F);
        }
        g_free(data);
        for (size_t i = 0; i < samples.size(); i += 64) {  // (in blocks, like the recorder)
            xoj::audio::tapSamples(samples.data() + i, std::min<size_t>(64, samples.size() - i), 1, rate);
        }
        return ToolResult::structured({{"fed_s", static_cast<double>(samples.size()) / rate}});
    };
    server.getRegistry().addTool(std::move(liveAudio));

    ToolSpec talk;
    talk.name = "test_talk";
    talk.title = "Test hook: «Hold to talk» without a microphone";
    talk.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): op=press (as if the button were pressed, no "
                       "microphone), op=feed (file: WAV PCM 16-bit mono: its sound), op=release (let go: the phrase "
                       "goes to the speech recognizer).";
    talk.inputSchema = schema::object({{"op", schema::enumeration("Operation", {"press", "feed", "release"})},
                                       {"file", schema::string("feed: a WAV file")}},
                                      {"op"});
    talk.tier = Tier::Ui;
    talk.handler = [srv](const json& j) {
        Args args(j);
        McpUi* ui = srv->getUi();
        if (!ui) {
            throw ToolError("no UI");
        }
        const std::string op = args.choice("op", {"press", "feed", "release"}, "");
        if (op == "feed") {
            gchar* data = nullptr;
            gsize size = 0;
            if (!g_file_get_contents(args.str("file").c_str(), &data, &size, nullptr) || size < 44) {
                g_free(data);
                throw ToolError("cannot read the WAV file");
            }
            const auto* bytes = reinterpret_cast<const unsigned char*>(data);
            const int rate = static_cast<int>(bytes[24] | (bytes[25] << 8) | (bytes[26] << 16) | (bytes[27] << 24));
            std::vector<float> samples((size - 44) / 2);
            for (size_t i = 0; i < samples.size(); i++) {
                samples[i] = static_cast<float>(static_cast<int16_t>(bytes[44 + 2 * i] | (bytes[45 + 2 * i] << 8))) /
                             32768.0f;
            }
            g_free(data);
            ui->talkFeed(samples.data(), samples.size(), 1, rate);
        } else {
            ui->talkPressed(op == "press", false);
        }
        return ToolResult::structured({{"ok", true}});
    };
    server.getRegistry().addTool(std::move(talk));

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

    ToolSpec modal;
    modal.name = "test_modal";
    modal.title = "Test hook: a modal dialog like Print";
    modal.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): opens a fake modal dialog right after answering (as "
                        "a key press would): for `ms`, a nested main loop runs while the document's shared lock is "
                        "held, as with the Print dialog.";
    modal.inputSchema = schema::object({{"ms", schema::integer("How long")}}, {"ms"});
    modal.tier = Tier::Ui;
    modal.handler = [ctrl](const json& j) {
        Args args(j);
        const auto ms = args.integer("ms", 1000, 1, 10000);
        struct Modal {
            Control* ctrl;
            guint ms;
        };
        g_idle_add(
                +[](gpointer data) -> gboolean {
                    std::unique_ptr<Modal> m(static_cast<Modal*>(data));
                    std::shared_lock lock(*m->ctrl->getDocument());
                    GMainLoop* loop = g_main_loop_new(nullptr, false);
                    g_timeout_add(
                            m->ms,
                            +[](gpointer l) -> gboolean {
                                g_main_loop_quit(static_cast<GMainLoop*>(l));
                                return G_SOURCE_REMOVE;
                            },
                            loop);
                    g_main_loop_run(loop);
                    g_main_loop_unref(loop);
                    return G_SOURCE_REMOVE;
                },
                new Modal{ctrl, static_cast<guint>(ms)});
        return ToolResult::structured({{"modal_ms", ms}});
    };
    server.getRegistry().addTool(std::move(modal));

    ToolSpec focus;
    focus.name = "test_focus";
    focus.title = "Test hook: the research browser's focus guard";
    focus.description = "Test hook (XOURNALAI_TEST_HOOKS=1 only): the focus guard's state (its windows, whether the "
                        "owner is in the browser, how often he was brought back, the last decision), the «↩ Back» "
                        "pill and the «🔎 Browser» button's text.";
    focus.inputSchema = schema::object({}, {});
    focus.tier = Tier::Ui;
    focus.handler = [srv](const json&) {
        auto* ui = srv->getUi();
        return ToolResult::structured(ui ? ui->researchFocusState() : json::object());
    };
    server.getRegistry().addTool(std::move(focus));

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
