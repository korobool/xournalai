/*
 * xournalai (based on Xournal++)
 *
 * The research browser's focus guard (1.18.2; the owner, 10 Nov 2026: "Браузер неконтролируемо перехватывает фокус …
 * меня даже уводит на другой workspace"; "если я сам руками поменяю фокус на браузер или альт-табом — что будет?").
 *
 * The browser may bring itself to the front while agents work in it (the Claude in Chrome extension activates the tab
 * it looks at). When it becomes the active window while he was in xournalai, no input of his went anywhere but the app
 * in the last moments, and the browser was busy, he is brought back at once. His own switches are never undone: an
 * Alt+Tab, a tap on the browser, a gesture all start with a press that the X server sees (XI2 raw events on the root
 * window) but that never reaches the app (the same server time among the app's own presses). Once he is in the
 * browser by his own doing, nothing pulls him out.
 *
 * Also «🔎 Browser» (the browser in front, he asked) and «↩ Back» (the notes in front again). X11 only; elsewhere
 * nothing happens (Wayland compositors don't let a window take focus anyway).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for int64_t
#include <deque>       // for deque
#include <functional>  // for function
#include <string>      // for string

#include <gtk/gtk.h>

#include "mcp/Json.h"

namespace xoj::assistant {

class FocusGuard final {
public:
    struct Hooks {
        std::function<bool()> enabled;      ///< the setting is on and the browser isn't open for signing in
        std::function<bool()> browserBusy;  ///< agents worked in the browser in the last moments
        std::function<void()> returned;     ///< he was brought back (a message)
        std::function<void(bool in)> ownerInBrowser;  ///< he is in the browser by his own doing (or no longer)
    };
    FocusGuard(GtkWindow* app, std::string browserClass, Hooks hooks);
    ~FocusGuard();
    FocusGuard(const FocusGuard&) = delete;
    FocusGuard& operator=(const FocusGuard&) = delete;

    /// «🔎 Browser»: the browser in front (he asked); false when it has no window
    bool showBrowser();
    /// «↩ Back»: the notes in front again
    void back();
    bool ownerInBrowser() const { return inBrowser; }
    /// The work area of the monitor the browser is on (for «↩ Back»); false when unknown
    bool browserMonitorArea(GdkRectangle* area) const;
    /// (tests) what it knows and did
    nlohmann::json state() const;

    /// (internal) the app saw a press at this server time
    void noteAppInput(uint32_t time);

private:
    void activeChanged(unsigned long window);
    bool isBrowserWindow(unsigned long window) const;
    bool isAppWindow(unsigned long window) const;
    bool pressWentElsewhere() const;
    void setInBrowser(bool in);
    void present(unsigned long window);
    unsigned long findBrowserWindow() const;
    bool windowManagerActivates() const;  ///< a window manager announces the active window (not the tests' bare X)
    static GdkFilterReturn rootFilter(GdkXEvent* xevent, GdkEvent* event, gpointer data);

    GtkWindow* app;
    std::string browserClass;
    Hooks hooks;
    bool x11 = false;
    int xiOpcode = -1;
    unsigned long activeWindow = 0;
    bool inBrowser = false;
    bool shown = false;  ///< (he asked for it with «🔎 Browser»)
    int returnedCount = 0;
    int rawSeen = 0;
    std::string lastDecision;
    struct Press {
        uint32_t time;
        int64_t received;  ///< (monotonic, µs)
    };
    std::deque<Press> rawPresses;   ///< every press the server saw (the last few seconds)
    std::deque<uint32_t> appPresses;  ///< the server times of the presses that reached the app
};

}  // namespace xoj::assistant
