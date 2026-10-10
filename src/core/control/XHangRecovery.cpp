#include "XHangRecovery.h"

#include <algorithm>  // for max
#include <cstdint>    // for int64_t
#include <cstdlib>  // for atoi
#include <string>   // for string

#include <glib.h>
#include <gtk/gtk.h>

#include "control/CrashHandler.h"  // for rescueSave
#include "util/StallWatch.h"       // for setHangHandler

#ifdef GDK_WINDOWING_X11
#include <atomic>  // for atomic
#include <map>     // for map

#include <X11/Xlib.h>    // for NextRequest, LastKnownRequestProcessed, ConnectionNumber
#include <dlfcn.h>       // for dlsym
#include <gdk/gdkx.h>    // for GDK_IS_X11_DISPLAY, gdk_x11_display_get_xdisplay
#include <sys/socket.h>  // for shutdown
#include <sys/uio.h>     // for iovec
#endif

namespace {
/// A fresh xournalai, 2 s after this one closes its display: it offers the rescued document. At most twice in a row
/// (XOURNALAI_RELAUNCHES counts), never in tests.
void relaunch() {
    if (g_getenv("XOURNALAI_TEST_HOOKS")) {
        return;
    }
    const char* count = g_getenv("XOURNALAI_RELAUNCHES");
    const int n = count ? atoi(count) : 0;
    if (n >= 2) {
        g_warning("Not restarting xournalai again (restarted %d times in a row after a frozen display)", n);
        return;
    }
    gchar* self = g_file_read_link("/proc/self/exe", nullptr);
    if (!self) {
        return;
    }
    const std::string next = std::to_string(n + 1);
    gchar* exe = g_shell_quote(self);
    const std::string script = "sleep 2; XOURNALAI_RELAUNCHES=" + next + " exec " + exe;
    const gchar* argv[] = {"/bin/sh", "-c", script.c_str(), nullptr};
    GError* err = nullptr;
    if (!g_spawn_async(nullptr, const_cast<gchar**>(argv), nullptr, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, &err)) {
        g_warning("Could not restart xournalai: %s", err ? err->message : "?");
        g_clear_error(&err);
    } else {
        g_message("Restarting xournalai in 2 s (the rescued document will be offered)");
    }
    g_free(exe);
    g_free(self);
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
#endif

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
        // (libX11-xcb is loaded by GDK; looked up here so no build dependency is needed)
        using GetXcb = void* (*)(Display*);
        if (auto getXcb = reinterpret_cast<GetXcb>(dlsym(RTLD_DEFAULT, "XGetXCBConnection"))) {
            tracedConnection = getXcb(x);
            tracedDisplay = x;
        }
        xoj::util::stall::setHangReporter([x] {
            const uint64_t answered = LastKnownRequestProcessed(x);
            const uint64_t sent = NextRequest(x) - 1;
            return "X: last request sent #" + std::to_string(sent) + ", last answered #" + std::to_string(answered) +
                   "; unanswered: " + requestsSince(std::max(answered + 1, sent > 15 ? sent - 15 : 0)) +
                   " | before: " + requestsSince(answered > 6 ? answered - 6 : 0);
        });
        xoj::util::stall::setHangHandler(afterUs, [fd] {
            rescueSave();
            // waiting for a reply, or to send (the server stopped reading): either way stuck on the X server
            const bool onX = xoj::util::stall::uiThreadIn({"libxcb.so"});
            g_message("Frozen: the UI thread is %s", onX ? "waiting on the X server" : "busy (not on X)");
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
    xoj::util::stall::setHangHandler(afterUs, [] { rescueSave(); });
}
