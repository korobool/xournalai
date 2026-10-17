#include "XHangRecovery.h"

#include <algorithm>  // for max
#include <csignal>    // for SIGABRT
#include <cstdint>    // for int64_t
#include <cstdlib>  // for atoi
#include <string>   // for string

#include <glib.h>
#include <unistd.h>  // for _exit
#include <gtk/gtk.h>

#include "control/CrashHandler.h"  // for rescueSave, crashing
#include "util/PathUtil.h"         // for getExeFile
#include "util/StallWatch.h"       // for setHangHandler

#ifdef GDK_WINDOWING_X11
#include <atomic>  // for atomic
#include <map>     // for map

#include <chrono>   // for milliseconds
#include <cstdlib>  // for free
#include <thread>   // for thread, sleep_for

#include <X11/Xlib.h>    // for NextRequest, LastKnownRequestProcessed, ConnectionNumber
#include <dlfcn.h>       // for dlopen, dlsym
#include <gdk/gdkx.h>    // for GDK_IS_X11_DISPLAY, gdk_x11_display_get_xdisplay
#include <sys/socket.h>  // for shutdown
#include <sys/uio.h>     // for iovec
#include <xcb/xcb.h>     // (types only: the functions are looked up, libX11 loads libxcb)
#endif

namespace {
/// A fresh xournalai, 2 s after this one closes its display: it offers the rescued document. At most twice in a row
/// (XOURNALAI_RELAUNCHES counts), in tests only when XOURNALAI_TEST_RELAUNCH asks.
void relaunch() {
    if (g_getenv("XOURNALAI_TEST_HOOKS") && !g_getenv("XOURNALAI_TEST_RELAUNCH")) {
        return;
    }
    const char* count = g_getenv("XOURNALAI_RELAUNCHES");
    const int n = count ? atoi(count) : 0;
    if (n >= 2) {
        g_warning("Not restarting xournalai again (restarted %d times in a row after a frozen display)", n);
        return;
    }
    // (not /proc/self/exe as it is: after an update replaced the binary it ends in " (deleted)", and the restart
    // silently failed, 2026-10-17 23:04)
    const std::string self = Util::getExeFile().string();
    if (!g_file_test(self.c_str(), G_FILE_TEST_IS_EXECUTABLE)) {
        g_warning("Could not restart xournalai: %s is gone", self.c_str());
        return;
    }
    const std::string next = std::to_string(n + 1);
    gchar* exe = g_shell_quote(self.c_str());
    const std::string script = "sleep 2; XOURNALAI_RELAUNCHES=" + next + " exec " + exe;
    const gchar* argv[] = {"/bin/sh", "-c", script.c_str(), nullptr};
    GError* err = nullptr;
    if (!g_spawn_async(nullptr, const_cast<gchar**>(argv), nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, &err)) {
        g_warning("Could not restart xournalai: %s", err ? err->message : "?");
        g_clear_error(&err);
    } else {
        g_message("Restarting xournalai in 2 s (the rescued document will be offered): %s", self.c_str());
    }
    g_free(exe);
}
}  // namespace

#ifdef GDK_WINDOWING_X11
// The last requests this app sent to the X server, by type: when the server leaves one unanswered (the freezes),
// the HANG report names it. libX11 sends every request through libxcb's xcb_writev; this binary exports its own
// (-rdynamic), which notes the requests' headers and calls the real one.
namespace {
struct SentRequest {
    uint64_t seq;
    uint8_t major;
    uint8_t minor;
};
constexpr size_t RING = 64;
SentRequest ring[RING];
std::atomic<uint64_t> ringNext{0};
std::atomic<Display*> tracedDisplay{nullptr};
std::atomic<void*> tracedConnection{nullptr};  ///< its xcb connection (XGetXCBConnection)
std::map<int, std::string> extensionNames;     // major opcode → extension (set once, before tracing starts)

const char* coreName(int op) {
    static const char* names[] = {nullptr,
                                  "CreateWindow",
                                  "ChangeWindowAttributes",
                                  "GetWindowAttributes",
                                  "DestroyWindow",
                                  "DestroySubwindows",
                                  "ChangeSaveSet",
                                  "ReparentWindow",
                                  "MapWindow",
                                  "MapSubwindows",
                                  "UnmapWindow",
                                  "UnmapSubwindows",
                                  "ConfigureWindow",
                                  "CirculateWindow",
                                  "GetGeometry",
                                  "QueryTree",
                                  "InternAtom",
                                  "GetAtomName",
                                  "ChangeProperty",
                                  "DeleteProperty",
                                  "GetProperty",
                                  "ListProperties",
                                  "SetSelectionOwner",
                                  "GetSelectionOwner",
                                  "ConvertSelection",
                                  "SendEvent",
                                  "GrabPointer",
                                  "UngrabPointer",
                                  "GrabButton",
                                  "UngrabButton",
                                  "ChangeActivePointerGrab",
                                  "GrabKeyboard",
                                  "UngrabKeyboard",
                                  "GrabKey",
                                  "UngrabKey",
                                  "AllowEvents",
                                  "GrabServer",
                                  "UngrabServer",
                                  "QueryPointer",
                                  "GetMotionEvents",
                                  "TranslateCoordinates",
                                  "WarpPointer",
                                  "SetInputFocus",
                                  "GetInputFocus",
                                  "QueryKeymap",
                                  "OpenFont",
                                  "CloseFont",
                                  "QueryFont",
                                  "QueryTextExtents",
                                  "ListFonts",
                                  "ListFontsWithInfo",
                                  "SetFontPath",
                                  "GetFontPath",
                                  "CreatePixmap",
                                  "FreePixmap",
                                  "CreateGC",
                                  "ChangeGC",
                                  "CopyGC",
                                  "SetDashes",
                                  "SetClipRectangles",
                                  "FreeGC",
                                  "ClearArea",
                                  "CopyArea",
                                  "CopyPlane",
                                  "PolyPoint",
                                  "PolyLine",
                                  "PolySegment",
                                  "PolyRectangle",
                                  "PolyArc",
                                  "FillPoly",
                                  "PolyFillRectangle",
                                  "PolyFillArc",
                                  "PutImage",
                                  "GetImage"};
    return op > 0 && op < static_cast<int>(sizeof names / sizeof *names) ? names[op] : nullptr;
}

std::string describe(const SentRequest& r) {
    std::string s = "#" + std::to_string(r.seq) + " ";
    if (const char* n = r.major < 128 ? coreName(r.major) : nullptr) {
        return s + n;
    }
    auto it = extensionNames.find(r.major);
    return s + (it != extensionNames.end() ? it->second : "op" + std::to_string(r.major)) + ":" +
           std::to_string(r.minor);
}

/// The requests from `from` on (sequence numbers), oldest first
std::string requestsSince(uint64_t from) {
    std::string out;
    const uint64_t end = ringNext.load();
    for (uint64_t i = end > RING ? end - RING : 0; i < end; i++) {
        const SentRequest r = ring[i % RING];
        if (r.seq >= from) {
            out += (out.empty() ? "" : ", ") + describe(r);
        }
    }
    return out;
}
}  // namespace

extern "C" int xcb_writev(void* c, struct iovec* vector, int count, uint64_t requests) {
    using Real = int (*)(void*, struct iovec*, int, uint64_t);
    static Real real = reinterpret_cast<Real>(dlsym(RTLD_NEXT, "xcb_writev"));
    Display* d = tracedDisplay.load();
    if (d && requests > 0 && tracedConnection.load() == c) {
        // Walk the request headers: major opcode, minor (extensions), length in 4-byte units (0: BIG-REQUESTS)
        uint64_t seq = NextRequest(d) - requests;  // (Xlib numbers them before sending)
        int vi = 0;
        size_t off = 0;
        auto byteAt = [&](size_t k, uint8_t& out) {
            int v = vi;
            size_t o = off + k;
            while (v < count && o >= vector[v].iov_len) {
                o -= vector[v].iov_len;
                v++;
            }
            if (v >= count) {
                return false;
            }
            out = static_cast<const uint8_t*>(vector[v].iov_base)[o];
            return true;
        };
        for (uint64_t r = 0; r < requests; r++) {
            uint8_t b[8];
            bool ok = true;
            for (int k = 0; k < 4 && ok; k++) {
                ok = byteAt(static_cast<size_t>(k), b[k]);
            }
            if (!ok) {
                break;
            }
            size_t len = static_cast<size_t>(b[2] | (b[3] << 8)) * 4;
            if (len == 0) {
                for (int k = 4; k < 8 && ok; k++) {
                    ok = byteAt(static_cast<size_t>(k), b[k]);
                }
                len = static_cast<size_t>(b[4] | (b[5] << 8) | (b[6] << 16) | (static_cast<uint32_t>(b[7]) << 24)) * 4;
            }
            const uint64_t i = ringNext.fetch_add(1);
            ring[i % RING] = {seq++, b[0], b[1]};
            if (len == 0) {
                break;
            }
            off += len;
            while (vi < count && off >= vector[vi].iov_len) {
                off -= vector[vi].iov_len;
                vi++;
            }
        }
    }
    return real(c, vector, count, requests);
}

namespace {
/// A second connection's round trip, started when a hang is reported: does the X server answer anyone? Answered
/// while this app waits, the server ignores only this app; not answered, it is busy or someone grabbed it.
struct Probe {
    std::atomic<uint64_t> hang{0};  ///< the hang it is for (this app's last sent request)
    std::atomic<int64_t> startUs{0};
    std::atomic<int64_t> answeredUs{0};
    std::atomic<bool> failed{false};
};
Probe probe;

template <typename F>
F lookup(const char* name) {
    return reinterpret_cast<F>(dlsym(RTLD_DEFAULT, name));
}

void probeServer(const char* displayName, uint64_t hang) {
    if (probe.hang.exchange(hang) == hang) {
        return;  // (already asked during this hang)
    }
    probe.answeredUs = 0;
    probe.failed = false;
    probe.startUs = g_get_monotonic_time();
    std::thread([name = std::string(displayName ? displayName : "")] {
        auto connect = lookup<decltype(&xcb_connect)>("xcb_connect");
        auto hasError = lookup<decltype(&xcb_connection_has_error)>("xcb_connection_has_error");
        auto ask = lookup<decltype(&xcb_get_input_focus)>("xcb_get_input_focus");
        auto reply = lookup<decltype(&xcb_get_input_focus_reply)>("xcb_get_input_focus_reply");
        auto disconnect = lookup<decltype(&xcb_disconnect)>("xcb_disconnect");
        if (!connect || !hasError || !ask || !reply || !disconnect) {
            probe.failed = true;
            return;
        }
        xcb_connection_t* c = connect(name.c_str(), nullptr);  // (a silent server keeps this thread: harmless)
        if (hasError(c)) {
            probe.failed = true;
        } else {
            free(reply(c, ask(c), nullptr));
            probe.answeredUs = g_get_monotonic_time();
        }
        disconnect(c);
    }).detach();
}

std::string probeResult() {
    if (!probe.startUs) {
        return "";
    }
    if (probe.failed) {
        return "a second connection fails";
    }
    if (const int64_t answered = probe.answeredUs) {
        return "a second connection was answered in " + std::to_string((answered - probe.startUs) / 1000) +
               " ms (the server serves others)";
    }
    return "a second connection has no answer either (" +
           std::to_string((g_get_monotonic_time() - probe.startUs) / 1000) + " ms)";
}
}  // namespace
#endif

namespace {
/// The UI thread froze in the crash handler (stuck on the allocator's lock after heap corruption): the document is
/// rescued, so end the dead app now instead of leaving its window, MCP port and helpers to the next start
void endIfCrashed() {
    if (crashing()) {
        g_message("Crashed, and the crash handler is stuck: ending the app (the document was rescued)");
        _exit(128 + SIGABRT);
    }
}
}  // namespace

uint64_t xRequestsLogged() {
#ifdef GDK_WINDOWING_X11
    return tracedDisplay.load() ? ringNext.load() : 0;
#else
    return 0;
#endif
}

void installXHangRecovery() {
    const char* after = g_getenv("XOURNALAI_RESCUE_AFTER_MS");  // (tests)
    const int64_t afterUs = after ? g_ascii_strtoll(after, nullptr, 10) * 1000 : 15 * G_USEC_PER_SEC;
#ifdef GDK_WINDOWING_X11
    GdkDisplay* display = gdk_display_get_default();
    if (display && GDK_IS_X11_DISPLAY(display)) {
        Display* x = gdk_x11_display_get_xdisplay(display);
        const int fd = ConnectionNumber(x);
        // (read racily from the watchdog thread: for diagnosis only)
        // Extension names for the request log, then start it
        int n = 0;
        if (char** names = XListExtensions(x, &n)) {
            for (int i = 0; i < n; i++) {
                int major = 0, event = 0, error = 0;
                if (XQueryExtension(x, names[i], &major, &event, &error)) {
                    extensionNames[major] = names[i];
                }
            }
            XFreeExtensionList(names);
        }
        // (libX11-xcb, loaded here: no build dependency. GDK does not link it, and whether audio had loaded it by
        // now was a race: on the owner's laptop the log never ran, so no HANG report named a request)
        using GetXcb = void* (*)(Display*);
        void* x11xcb = dlopen("libX11-xcb.so.1", RTLD_NOW | RTLD_GLOBAL);
        auto getXcb = reinterpret_cast<GetXcb>(x11xcb ? dlsym(x11xcb, "XGetXCBConnection") : nullptr);
        if (!getXcb) {
            g_warning("The X request log is off (no libX11-xcb): HANG reports will not name the requests");
        } else {
            tracedConnection = getXcb(x);
            tracedDisplay = x;
        }
        const std::string displayName = DisplayString(x);
        xoj::util::stall::setHangReporter([x, displayName] {
            const uint64_t answered = LastKnownRequestProcessed(x);
            const uint64_t sent = NextRequest(x) - 1;
            probeServer(displayName.c_str(), sent);
            return "X: " + probeResult() + "; last request sent #" + std::to_string(sent) + ", last answered #" +
                   std::to_string(answered) +
                   "; unanswered: " + requestsSince(std::max(answered + 1, sent > 15 ? sent - 15 : 0)) +
                   " | before: " + requestsSince(answered > 6 ? answered - 6 : 0) + " | logged " +
                   (tracedDisplay.load() ? std::to_string(ringNext.load()) : std::string("nothing (log off)"));
        });
        xoj::util::stall::setHangHandler(afterUs, [fd, x, displayName] {
            probeServer(displayName.c_str(), NextRequest(x) - 1);  // (asked at the first report, if tracing)
            rescueSave();
            endIfCrashed();
            for (int i = 0; i < 20 && !probe.answeredUs && !probe.failed &&
                            g_get_monotonic_time() - probe.startUs < 2 * G_USEC_PER_SEC;
                 i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            // waiting for a reply, or to send (the server stopped reading): either way stuck on the X server
            const bool onX = xoj::util::stall::uiThreadIn({"libxcb.so"});
            g_message("Frozen: the UI thread is %s; %s", onX ? "waiting on the X server" : "busy (not on X)",
                      probeResult().c_str());
            if (onX) {
                g_warning("The X server does not answer (the UI waits for it): closing the display connection; "
                          "your document was rescued and is offered on the next start");
                relaunch();
                shutdown(fd, SHUT_RDWR);  // the waiting poll wakes, GTK sees the display gone and exits
            }
        });
        return;
    }
#endif
    xoj::util::stall::setHangHandler(afterUs, [] {
        rescueSave();
        endIfCrashed();
    });
}
