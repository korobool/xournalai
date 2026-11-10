#include "FocusGuard.h"

#include <algorithm>  // for find
#include <utility>    // for move

#include <glib.h>

#include "config-features.h"  // for ENABLE_X11
#include "gui/TooltipGuard.h"  // for setInputObserver

#ifdef ENABLE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <gdk/gdkx.h>
#endif

namespace xoj::assistant {

using nlohmann::json;

namespace {
constexpr int64_t RECENT_US = 1500000;  ///< a press of his this recent explains a switch
constexpr size_t KEPT = 64;

void observeApp(guint32 time, gpointer data) { static_cast<FocusGuard*>(data)->noteAppInput(time); }
}  // namespace

FocusGuard::FocusGuard(GtkWindow* window, std::string cls, Hooks h):
        app(window), browserClass(std::move(cls)), hooks(std::move(h)) {
#ifdef ENABLE_X11
    GdkDisplay* display = gdk_display_get_default();
    if (!display || !GDK_IS_X11_DISPLAY(display)) {
        return;
    }
    x11 = true;
    Display* dpy = GDK_DISPLAY_XDISPLAY(display);
    // The active window, as the window manager announces it on the root window
    GdkWindow* root = gdk_get_default_root_window();
    gdk_window_set_events(root, static_cast<GdkEventMask>(gdk_window_get_events(root) | GDK_PROPERTY_CHANGE_MASK));
    gdk_window_add_filter(nullptr, &FocusGuard::rootFilter, this);
    // Every press the server sees, wherever it goes (XI2 raw events: also during the window manager's grabs)
    int event = 0, error = 0;
    if (XQueryExtension(dpy, "XInputExtension", &xiOpcode, &event, &error)) {
        unsigned char bits[XIMaskLen(XI_LASTEVENT)] = {};
        XISetMask(bits, XI_RawKeyPress);
        XISetMask(bits, XI_RawButtonPress);
        XISetMask(bits, XI_RawTouchBegin);
        XIEventMask mask{XIAllMasterDevices, static_cast<int>(sizeof(bits)), bits};
        XISelectEvents(dpy, DefaultRootWindow(dpy), &mask, 1);
    }
    xoj::gui::setInputObserver(&observeApp, this);
    // (what is active now)
    Atom type;
    int format;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False), 0, 1, False,
                           XA_WINDOW, &type, &format, &n, &after, &data) == Success &&
        data && n == 1) {
        activeWindow = *reinterpret_cast<Window*>(data);
    }
    if (data) {
        XFree(data);
    }
#endif
}

FocusGuard::~FocusGuard() {
#ifdef ENABLE_X11
    if (x11) {
        xoj::gui::setInputObserver(nullptr, nullptr);
        gdk_window_remove_filter(nullptr, &FocusGuard::rootFilter, this);
    }
#endif
}

void FocusGuard::noteAppInput(uint32_t time) {
    appPresses.push_back(time);
    while (appPresses.size() > KEPT) {
        appPresses.pop_front();
    }
}

GdkFilterReturn FocusGuard::rootFilter(GdkXEvent* xev, GdkEvent*, gpointer data) {
#ifdef ENABLE_X11
    auto* self = static_cast<FocusGuard*>(data);
    auto* e = static_cast<XEvent*>(xev);
    if (e->type == GenericEvent && e->xcookie.extension == self->xiOpcode && e->xcookie.data &&
        (e->xcookie.evtype == XI_RawKeyPress || e->xcookie.evtype == XI_RawButtonPress ||
         e->xcookie.evtype == XI_RawTouchBegin)) {
        const auto* raw = static_cast<XIRawEvent*>(e->xcookie.data);
        self->rawSeen++;
        self->rawPresses.push_back({static_cast<uint32_t>(raw->time), g_get_monotonic_time()});
        while (self->rawPresses.size() > KEPT) {
            self->rawPresses.pop_front();
        }
        return GDK_FILTER_REMOVE;  // (only ours: GDK never asked for them)
    }
    if (e->type == PropertyNotify && e->xproperty.window == DefaultRootWindow(e->xproperty.display) &&
        e->xproperty.atom == XInternAtom(e->xproperty.display, "_NET_ACTIVE_WINDOW", False)) {
        Display* dpy = e->xproperty.display;
        Atom type;
        int format;
        unsigned long n = 0, after = 0;
        unsigned char* value = nullptr;
        unsigned long active = 0;
        if (XGetWindowProperty(dpy, DefaultRootWindow(dpy), e->xproperty.atom, 0, 1, False, XA_WINDOW, &type,
                               &format, &n, &after, &value) == Success &&
            value && n == 1) {
            active = *reinterpret_cast<Window*>(value);
        }
        if (value) {
            XFree(value);
        }
        if (active != self->activeWindow) {
            self->activeChanged(active);
        }
    }
#endif
    return GDK_FILTER_CONTINUE;
}

bool FocusGuard::pressWentElsewhere() const {
    // A recent press the server saw whose time is not among the app's own: it went to the window manager (Alt+Tab,
    // Super, a gesture) or to another window (a tap on the browser)
    const int64_t now = g_get_monotonic_time();
    for (const auto& p: rawPresses) {
        if (now - p.received < RECENT_US && std::find(appPresses.begin(), appPresses.end(), p.time) == appPresses.end()) {
            return true;
        }
    }
    return false;
}

void FocusGuard::activeChanged(unsigned long window) {
    const unsigned long previous = activeWindow;
    activeWindow = window;
    if (!isBrowserWindow(window)) {
        if (inBrowser && !isBrowserWindow(window)) {
            shown = false;
            setInBrowser(false);
        }
        return;
    }
    if (inBrowser) {
        return;  // (another window of the browser: he is there already)
    }
    if (shown) {
        lastDecision = "shown: he asked for it";
        setInBrowser(true);
        return;
    }
    const bool fromApp = isAppWindow(previous);
    const bool his = pressWentElsewhere();
    const bool busy = hooks.browserBusy && hooks.browserBusy();
    const bool on = !hooks.enabled || hooks.enabled();
    if (on && fromApp && !his && busy) {
        returnedCount++;
        lastDecision = "returned: the browser came to the front by itself while he was in the app";
        g_message("research browser: it took the focus by itself; he is brought back to the notes");
        present(0);
        if (hooks.returned) {
            hooks.returned();
        }
        return;
    }
    lastDecision = std::string("his own: ") + (his ? "a press of his went past the app" :
                                               !fromApp ? "he wasn't in the app" :
                                               !busy    ? "the browser wasn't busy" :
                                                          "the guard is off");
    setInBrowser(true);
}

void FocusGuard::setInBrowser(bool in) {
    if (inBrowser == in) {
        return;
    }
    inBrowser = in;
    if (hooks.ownerInBrowser) {
        hooks.ownerInBrowser(in);
    }
}

bool FocusGuard::isBrowserWindow(unsigned long window) const {
#ifdef ENABLE_X11
    if (!x11 || !window) {
        return false;
    }
    GdkDisplay* gd = gdk_display_get_default();
    Display* dpy = GDK_DISPLAY_XDISPLAY(gd);
    gdk_x11_display_error_trap_push(gd);
    XClassHint hint{};
    bool is = false;
    if (XGetClassHint(dpy, window, &hint)) {
        auto same = [this](const char* s) { return s && g_ascii_strcasecmp(s, browserClass.c_str()) == 0; };
        is = same(hint.res_name) || same(hint.res_class);
        XFree(hint.res_name);
        XFree(hint.res_class);
    }
    gdk_x11_display_error_trap_pop_ignored(gd);
    return is;
#else
    (void)window;
    return false;
#endif
}

bool FocusGuard::isAppWindow(unsigned long window) const {
#ifdef ENABLE_X11
    return x11 && window && gdk_x11_window_lookup_for_display(gdk_display_get_default(), window);
#else
    (void)window;
    return false;
#endif
}

unsigned long FocusGuard::findBrowserWindow() const {
#ifdef ENABLE_X11
    if (!x11) {
        return 0;
    }
    Display* dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    const Window root = DefaultRootWindow(dpy);
    unsigned long found = 0;
    // The window manager's list of windows; without one (the tests), the root's children
    Atom type;
    int format;
    unsigned long n = 0, after = 0;
    unsigned char* data = nullptr;
    const Atom list = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
    if (list != None && XGetWindowProperty(dpy, root, list, 0, 4096, False, XA_WINDOW, &type, &format, &n, &after,
                                           &data) == Success &&
        data) {
        auto* windows = reinterpret_cast<Window*>(data);
        for (unsigned long i = 0; i < n; i++) {
            if (isBrowserWindow(windows[i])) {
                found = windows[i];  // (the last one: the newest)
            }
        }
        XFree(data);
        return found;
    }
    Window r, parent;
    Window* children = nullptr;
    unsigned int count = 0;
    if (XQueryTree(dpy, root, &r, &parent, &children, &count)) {
        for (unsigned int i = 0; i < count; i++) {
            if (isBrowserWindow(children[i])) {
                XWindowAttributes a;
                if (XGetWindowAttributes(dpy, children[i], &a) && a.map_state == IsViewable) {
                    found = children[i];
                }
            }
        }
        if (children) {
            XFree(children);
        }
    }
    return found;
#else
    return 0;
#endif
}

void FocusGuard::present(unsigned long window) {
#ifdef ENABLE_X11
    if (!x11) {
        return;
    }
    GdkDisplay* gd = gdk_display_get_default();
    Display* dpy = GDK_DISPLAY_XDISPLAY(gd);
    GdkWindow* appWin = gtk_widget_get_window(GTK_WIDGET(app));
    if (!appWin) {
        return;
    }
    // A fresh server time: the window manager lets the request through (an older one could be refused as stealing)
    const guint32 now = gdk_x11_get_server_time(appWin);
    if (!window) {
        gtk_window_present_with_time(app, now);
        return;
    }
    const Window root = DefaultRootWindow(dpy);
    const Atom active = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    if (windowManagerActivates()) {
        XEvent ev{};
        ev.xclient.type = ClientMessage;
        ev.xclient.window = window;
        ev.xclient.message_type = active;
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 2;  // (as a pager would: he asked for it)
        ev.xclient.data.l[1] = now;
        ev.xclient.data.l[2] = static_cast<long>(gdk_x11_window_get_xid(appWin));
        XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    } else {
        // (no window manager: the tests) raised and focused directly, as one would do it
        gdk_x11_display_error_trap_push(gd);
        XRaiseWindow(dpy, window);
        XSetInputFocus(dpy, window, RevertToParent, now);
        gdk_x11_display_error_trap_pop_ignored(gd);
    }
    XFlush(dpy);
#else
    (void)window;
#endif
}

bool FocusGuard::windowManagerActivates() const {
#ifdef ENABLE_X11
    return x11 && gdk_x11_screen_supports_net_wm_hint(gdk_screen_get_default(),
                                                      gdk_atom_intern_static_string("_NET_ACTIVE_WINDOW"));
#else
    return false;
#endif
}

bool FocusGuard::showBrowser() {
    const unsigned long w = findBrowserWindow();
    if (!w) {
        return false;
    }
    shown = true;
    present(w);
    if (!windowManagerActivates()) {  // (nobody announces the change: he is there now)
        lastDecision = "shown: he asked for it";
        setInBrowser(true);
    }
    return true;
}

void FocusGuard::back() {
    shown = false;
    present(0);
    if (!windowManagerActivates()) {
        setInBrowser(false);
    }
}

bool FocusGuard::browserMonitorArea(GdkRectangle* area) const {
#ifdef ENABLE_X11
    const unsigned long w = findBrowserWindow();
    GdkDisplay* gd = gdk_display_get_default();
    if (!w || !gd) {
        return false;
    }
    Display* dpy = GDK_DISPLAY_XDISPLAY(gd);
    gdk_x11_display_error_trap_push(gd);
    XWindowAttributes a{};
    Window child;
    int x = 0, y = 0;
    const bool ok = XGetWindowAttributes(dpy, w, &a) &&
                    XTranslateCoordinates(dpy, w, DefaultRootWindow(dpy), 0, 0, &x, &y, &child);
    gdk_x11_display_error_trap_pop_ignored(gd);
    if (!ok) {
        return false;
    }
    GdkMonitor* m = gdk_display_get_monitor_at_point(gd, x + a.width / 2, y + a.height / 2);
    if (!m) {
        return false;
    }
    gdk_monitor_get_workarea(m, area);
    return true;
#else
    (void)area;
    return false;
#endif
}

json FocusGuard::state() const {
    json apps = json::array();
#ifdef ENABLE_X11
    if (GdkWindow* w = gtk_widget_get_window(GTK_WIDGET(app)); w && x11) {
        apps.push_back(static_cast<unsigned long>(gdk_x11_window_get_xid(w)));
    }
#endif
    return {{"x11", x11},
            {"app_window", apps.empty() ? json() : apps[0]},
            {"browser_window", findBrowserWindow()},
            {"active_window", activeWindow},
            {"in_browser", inBrowser},
            {"shown", shown},
            {"returned", returnedCount},
            {"raw_presses_seen", rawSeen},
            {"last_decision", lastDecision}};
}

}  // namespace xoj::assistant
