#include "ScreenKeyboard.h"

#include <cstring>  // for strcmp
#include <string>   // for string

#include <gtk/gtk.h>

namespace xoj::gui::screen_keyboard {

namespace {
/// (where the session's choice is kept for the processes the app starts: GTK_IM_MODULE itself is taken out)
constexpr const char* SESSION_VAR = "XOURNALAI_SESSION_IM";
std::string session;
bool known = false;
bool manualNow = false;
}  // namespace

void takeOverInputMethod(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--mcp-stdio") == 0 || std::strncmp(argv[i], "--ai-hook", 9) == 0) {
            return;  // (no window; they may start the app, which takes over itself)
        }
    }
    if (const char* im = g_getenv("GTK_IM_MODULE")) {
        session = im;
        g_setenv(SESSION_VAR, im, TRUE);
        g_unsetenv("GTK_IM_MODULE");
    } else if (const char* kept = g_getenv(SESSION_VAR)) {
        session = kept;  // (started again by the app itself)
    }
    known = true;
}

const char* sessionInputMethod() { return session.empty() ? nullptr : session.c_str(); }

void setManual(bool manual) {
    manualNow = manual;
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings || !known) {
        return;  // (GTK_IM_MODULE still decides)
    }
    // (the text fields switch on their next focus: a change of this setting makes them look again)
    g_object_set(settings, "gtk-im-module", manual ? "gtk-im-context-simple" : sessionInputMethod(), nullptr);
}

bool isManual() { return manualNow; }

}  // namespace xoj::gui::screen_keyboard
