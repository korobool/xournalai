#include "XHangRecovery.h"

#include <cstdint>  // for int64_t
#include <cstdlib>  // for atoi
#include <string>   // for string

#include <glib.h>
#include <gtk/gtk.h>

#include "control/CrashHandler.h"  // for rescueSave
#include "util/StallWatch.h"       // for setHangHandler

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>    // for NextRequest, LastKnownRequestProcessed, ConnectionNumber
#include <gdk/gdkx.h>    // for GDK_IS_X11_DISPLAY, gdk_x11_display_get_xdisplay
#include <sys/socket.h>  // for shutdown
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

void installXHangRecovery() {
    const char* after = g_getenv("XOURNALAI_RESCUE_AFTER_MS");  // (tests)
    const int64_t afterUs = after ? g_ascii_strtoll(after, nullptr, 10) * 1000 : 15 * G_USEC_PER_SEC;
#ifdef GDK_WINDOWING_X11
    GdkDisplay* display = gdk_display_get_default();
    if (display && GDK_IS_X11_DISPLAY(display)) {
        Display* x = gdk_x11_display_get_xdisplay(display);
        const int fd = ConnectionNumber(x);
        // (read racily from the watchdog thread: for diagnosis only)
        xoj::util::stall::setHangReporter([x] {
            return "X: last request sent #" + std::to_string(NextRequest(x) - 1) + ", last answered #" +
                   std::to_string(LastKnownRequestProcessed(x));
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
