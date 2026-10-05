#include "XHangRecovery.h"

#include <cstdint>  // for int64_t
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
                shutdown(fd, SHUT_RDWR);  // the waiting poll wakes, GTK sees the display gone and exits
            }
        });
        return;
    }
#endif
    xoj::util::stall::setHangHandler(afterUs, [] { rescueSave(); });
}
