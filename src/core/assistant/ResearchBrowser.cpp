#include "ResearchBrowser.h"

#include <algorithm>  // for clamp
#include <csignal>    // for SIGTERM, kill
#include <fstream>    // for ifstream, ofstream
#include <sstream>    // for ostringstream

#include <libsoup/soup.h>

#include "config-features.h"  // for ENABLE_X11

#ifdef ENABLE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>
#endif

namespace xoj::assistant {

using nlohmann::json;

namespace {
constexpr const char* WINDOW_CLASS = "xournalai-research";
const char* const USER_AGENT =
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/154.0.0.0 Safari/537.36";

std::string escapeQuery(const std::string& q) {
    gchar* e = g_uri_escape_string(q.c_str(), nullptr, TRUE);
    std::string out = e;
    g_free(e);
    return out;
}

/// What `read` and `saveImage` count as a picture: big enough to mean something
const char* const PICTURES = "[...document.images].filter(i => i.naturalWidth >= 100 && i.naturalHeight >= 60)";

std::string readExpression(size_t maxChars) {
    // (lazy pictures of a tab that was never on screen don't load: loaded first, a moment at most)
    return std::string("(async () => { const lazy = [...document.images].filter(i => i.loading === 'lazy' && "
                       "!i.complete); lazy.forEach(i => { i.loading = 'eager'; }); if (lazy.length) { await "
                       "Promise.race([Promise.all(lazy.map(i => i.decode().catch(() => {}))), new Promise(r => "
                       "setTimeout(r, 1500))]); } const pics = ") +
           PICTURES +
           ".slice(0, 30).map((i, n) => ({n, src: (i.currentSrc || i.src || '').slice(0, 300), alt: i.alt || '', "
           "width: i.naturalWidth, height: i.naturalHeight}));"
           "const root = document.querySelector('article, main, #content, #search, #mw-content-text') || document.body;"
           "let text = (root ? root.innerText : '').replace(/\\n{3,}/g, '\\n\\n');"
           "return JSON.stringify({title: document.title, url: location.href, text: text.slice(0, " +
           std::to_string(maxChars) +
           "), truncated: text.length > " + std::to_string(maxChars) +
           ", pictures: pics, selection: String(window.getSelection ? window.getSelection() : '').slice(0, 4000), "
           "visible: document.visibilityState === 'visible', focused: document.hasFocus()}); })()";
}
}  // namespace

ResearchBrowser::Settings ResearchBrowser::Settings::fromJson(const json& j, const fs::path& dataDir) {
    Settings s;
    s.profileDir = dataDir / "research-browser";
    s.search = {{"web", "https://www.google.com/search?q={q}"},
                {"images", "https://www.google.com/search?tbm=isch&q={q}"},
                {"wiki", "https://en.wikipedia.org/w/index.php?search={q}"},
                {"scholar", "https://scholar.google.com/scholar?q={q}"}};
    if (!j.is_object()) {
        return s;
    }
    s.chrome = j.value("chrome", s.chrome);
    s.profile = j.value("profile", s.profile);
    s.where = j.value("where", s.where);
    s.passwordStore = j.value("password_store", s.passwordStore);
    s.closeOnExit = j.value("close_on_exit", s.closeOnExit);
    s.offerExtension = j.value("offer_extension", s.offerExtension);
    if (j.contains("extra_args") && j["extra_args"].is_array()) {
        for (const auto& a: j["extra_args"]) {
            if (a.is_string()) {
                s.extraArgs.push_back(a.get<std::string>());
            }
        }
    }
    if (j.contains("sign_in_pages") && j["sign_in_pages"].is_array()) {
        s.signInPages.clear();
        for (const auto& a: j["sign_in_pages"]) {
            if (a.is_string()) {
                s.signInPages.push_back(a.get<std::string>());
            }
        }
    }
    if (j.contains("profile_dir") && j["profile_dir"].is_string()) {
        s.profileDir = fs::path(j["profile_dir"].get<std::string>());
    }
    if (j.contains("search") && j["search"].is_object()) {
        for (const auto& [k, v]: j["search"].items()) {
            if (v.is_string()) {
                s.search[k] = v.get<std::string>();
            }
        }
    }
    return s;
}

ResearchBrowser::ResearchBrowser(Settings s, GtkWindow* window): settings(std::move(s)), appWindow(window) {
    session = soup_session_new();
    soup_session_set_timeout(session, 20);
}

ResearchBrowser::~ResearchBrowser() {
    *alive = false;
    if (signInWatch) {
        g_source_remove(signInWatch);
    }
    // (the browser stays: it is a window of the owner's; the next start of the app finds it again; unless asked)
    if (process) {
        if (settings.closeOnExit) {
            g_subprocess_send_signal(process, SIGTERM);
        }
        g_object_unref(process);
    }
    g_object_unref(session);
}

std::string ResearchBrowser::searchUrl(const std::string& kind, const std::string& query) const {
    auto it = settings.search.find(kind);
    std::string t = it != settings.search.end() ? it->second : settings.search.at("web");
    const std::string q = escapeQuery(query);
    for (size_t p = t.find("{q}"); p != std::string::npos; p = t.find("{q}", p + q.size())) {
        t.replace(p, 3, q);
    }
    return t;
}

int ResearchBrowser::readPortFile() const {
    std::ifstream in(settings.profileDir / "DevToolsActivePort");
    int p = 0;
    in >> p;
    return in ? p : 0;
}

void ResearchBrowser::ensure(std::function<void(const std::string& error)> ready) {
    if (signInOpen) {
        ready(SIGN_IN_OPEN);
        return;
    }
    if (launching) {
        waiting.push_back(std::move(ready));
        return;
    }
    // (it may still run: from before, or the app restarted; else it is started)
    const int known = port ? port : readPortFile();
    if (known <= 0) {
        start(std::move(ready));
        return;
    }
    port = known;
    std::weak_ptr<bool> token = alive;
    http("GET", "/json/version", [this, token, ready](const json&, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        if (error.empty()) {
            ready("");
            return;
        }
        port = 0;
        start(ready);
    });
}

void ResearchBrowser::start(std::function<void(const std::string&)> ready) {
    if (settings.offerExtension && !offered && settings.profile != "incognito" && !extensionInstalled()) {
        // Its first start: set up (Claude in Chrome installed, signed in), which needs it without the app's port; it
        // comes back connected when he closes it
        offered = true;
        reconnectAfterSignIn = true;
        signIn([ready](const std::string& error) { ready(error.empty() ? SIGN_IN_OPEN : error); });
        return;
    }
    launch(std::move(ready));
}

void ResearchBrowser::launch(std::function<void(const std::string&)> ready) {
    if (settings.profile != "incognito") {
        bool withPort = false;
        if (profilePid(&withPort) > 0) {
            if (withPort) {
                ready("the research browser runs but does not answer; close its window and try again");
                return;
            }
            // open without the app's port (for signing in, from before the app restarted, or started by hand): it
            // comes back connected once he closes it
            signInOpen = true;
            reconnectAfterSignIn = true;
            watchSignIn();
            ready(SIGN_IN_OPEN);
            return;
        }
    }
    launching = true;
    waiting.push_back(std::move(ready));
    std::error_code ec;
    fs::path dir = settings.profileDir;
    if (settings.profile == "incognito") {
        dir = settings.profileDir.parent_path() / "research-incognito";
    }
    fs::create_directories(dir, ec);
    fs::remove(dir / "DevToolsActivePort", ec);
    settings.profileDir = dir;
    prepareProfile(dir);
    std::vector<std::string> args = {settings.chrome,
                                     "--user-data-dir=" + dir.string(),
                                     "--remote-debugging-port=0",
                                     std::string("--class=") + WINDOW_CLASS,
                                     "--no-first-run",
                                     "--no-default-browser-check",
                                     "--new-window"};
    if (settings.profile == "incognito") {
        args.emplace_back("--incognito");
    }
    if (!settings.passwordStore.empty()) {
        args.push_back("--password-store=" + settings.passwordStore);
    }
    // Its tabs work while nobody looks at them (the owner, 10 Nov: "не будет такого, что табы подзасыпают?"):
    // no timer throttling or lower priority for background tabs and covered windows, no intensive throttling after
    // 5 minutes (one --disable-features: Chrome takes only the last one given)
    args.emplace_back("--disable-background-timer-throttling");
    args.emplace_back("--disable-renderer-backgrounding");
    args.emplace_back("--disable-backgrounding-occluded-windows");
    std::string disabled = "IntensiveWakeUpThrottling";
    for (const auto& a: settings.extraArgs) {
        if (a.rfind("--disable-features=", 0) == 0) {
            disabled += "," + a.substr(19);
        } else {
            args.push_back(a);
        }
    }
    args.push_back("--disable-features=" + disabled);
    keepAwake(dir);
    // Where: another monitor, if there are several (else the next workspace, once its window shows)
    GdkDisplay* display = gdk_display_get_default();
    const int monitors = display ? gdk_display_get_n_monitors(display) : 1;
    GdkMonitor* target = nullptr;
    if (display && settings.where.rfind("monitor:", 0) == 0) {
        target = gdk_display_get_monitor(display, std::max(0, std::atoi(settings.where.c_str() + 8) - 1));
    } else if (display && settings.where == "auto" && monitors > 1) {
        GdkMonitor* here = appWindow && gtk_widget_get_window(GTK_WIDGET(appWindow)) ?
                                   gdk_display_get_monitor_at_window(display, gtk_widget_get_window(GTK_WIDGET(appWindow))) :
                                   nullptr;
        for (int i = 0; i < monitors && !target; i++) {
            GdkMonitor* m = gdk_display_get_monitor(display, i);
            if (m != here) {
                target = m;
            }
        }
    }
    if (target) {
        GdkRectangle r;
        gdk_monitor_get_workarea(target, &r);
        args.push_back("--window-position=" + std::to_string(r.x) + "," + std::to_string(r.y));
        args.push_back("--window-size=" + std::to_string(r.width) + "," + std::to_string(r.height));
    }
    args.emplace_back("about:blank");
    std::vector<const char*> argv;
    for (const auto& a: args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);
    GError* err = nullptr;
    GSubprocess* p = g_subprocess_newv(argv.data(),
                                       static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                                                     G_SUBPROCESS_FLAGS_STDERR_SILENCE),
                                       &err);
    if (!p) {
        const std::string e = std::string("the browser could not be started (") + settings.chrome +
                              "): " + (err ? err->message : "?");
        g_clear_error(&err);
        launching = false;
        auto w = std::move(waiting);
        waiting.clear();
        for (auto& f: w) {
            f(e);
        }
        return;
    }
    if (process) {
        g_object_unref(process);
    }
    process = p;  // (it runs on its own; it stays when the app quits, unless closeOnExit)
    waitForPort(100, [this, target](const std::string& error) {
        launching = false;
        if (error.empty() && !target) {
            place();
        }
        auto w = std::move(waiting);
        waiting.clear();
        for (auto& f: w) {
            f(error);
        }
    });
}

int ResearchBrowser::profilePid(bool* withPort) const {
    // Chrome holds its profile folder with a link "SingletonLock" → "<host>-<pid>"
    std::error_code ec;
    const std::string target = fs::read_symlink(settings.profileDir / "SingletonLock", ec).string();
    const auto dash = target.rfind('-');
    if (ec || dash == std::string::npos) {
        return 0;
    }
    const int pid = std::atoi(target.c_str() + dash + 1);
    if (pid <= 0 || kill(pid, 0) != 0) {
        return 0;
    }
    // (verified: that process is a Chrome of this very profile folder)
    std::ifstream in("/proc/" + std::to_string(pid) + "/cmdline", std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    std::string cmd = all.str();
    std::replace(cmd.begin(), cmd.end(), '\0', ' ');
    if (cmd.find("--user-data-dir=" + settings.profileDir.string()) == std::string::npos) {
        return 0;
    }
    if (withPort) {
        *withPort = cmd.find("--remote-debugging-port") != std::string::npos;
    }
    return pid;
}

void ResearchBrowser::waitForExit(int tries, std::function<void(bool exited)> done) {
    struct Wait {
        std::weak_ptr<bool> token;
        ResearchBrowser* self;
        int tries;
        std::function<void(bool)> done;
    };
    g_timeout_add(
            200,
            +[](gpointer d) -> gboolean {
                std::unique_ptr<Wait> w(static_cast<Wait*>(d));
                if (!w->token.lock()) {
                    return G_SOURCE_REMOVE;
                }
                if (w->self->profilePid() == 0) {
                    w->done(true);
                } else if (w->tries <= 0) {
                    w->done(false);
                } else {
                    w->self->waitForExit(w->tries - 1, std::move(w->done));
                }
                return G_SOURCE_REMOVE;
            },
            new Wait{alive, this, tries, std::move(done)});
}

void ResearchBrowser::shutDown(std::function<void(const std::string&)> done) {
    std::weak_ptr<bool> token = alive;
    // the last resort: SIGTERM (a clean exit for Chrome) to the pid that holds this very profile
    auto terminate = [this, token, done]() {
        if (!token.lock()) {
            return;
        }
        if (const int pid = profilePid(); pid > 0) {
            kill(pid, SIGTERM);
        }
        waitForExit(50, [done](bool exited) { done(exited ? "" : "the research browser did not close"); });
    };
    if (port <= 0) {
        port = readPortFile();
    }
    http("GET", "/json/version", [this, token, terminate, done](const json& v, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        const std::string ws = v.is_object() ? v.value("webSocketDebuggerUrl", "") : "";
        const auto at = ws.find("/devtools/browser/");
        if (!error.empty() || at == std::string::npos) {
            terminate();
            return;
        }
        command(ws.substr(at), "Browser.close", json::object(), [](const json&, const std::string&) {});
        port = 0;
        waitForExit(50, [token, terminate, done](bool exited) {
            if (!token.lock()) {
                return;
            }
            if (exited) {
                done("");
            } else {
                terminate();
            }
        });
    });
}

std::string ResearchBrowser::setupPage() const {
    // A page of the app's own, first in the sign-in window: what to do there (in the app's language: English)
    const fs::path page = settings.profileDir.parent_path() / "research-sign-in.html";
    std::ofstream out(page);
    out << R"(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>xournalai research browser: sign in</title>
<style>
:root { color-scheme: light dark; --bg: #fbfbf8; --fg: #222; --muted: #666; --accent: #2f6fde; }
@media (prefers-color-scheme: dark) { :root { --bg: #1d1f21; --fg: #e8e8e8; --muted: #a0a0a0; --accent: #7aa7ff; } }
body { background: var(--bg); color: var(--fg); font: 18px/1.5 system-ui, sans-serif; margin: 0; padding: 48px 16px; }
main { max-width: 640px; margin: 0 auto; }
h1 { font-size: 26px; margin: 0 0 6px; }
p.lead { color: var(--muted); margin: 0 0 28px; }
ol { padding-left: 22px; } li { margin: 0 0 16px; }
b { color: var(--accent); }
</style></head><body><main>
<h1>Sign in to the research browser</h1>
<p class="lead">The app has let go of this window for now: Google doesn't allow signing in while a program is
connected to the browser. Your sign-ins stay in this profile.</p>
<ol>
<li><b>Claude in Chrome:</b> if a Chrome Web Store tab is open, add the extension (once).</li>
<li><b>Claude:</b> sign in on the claude.ai tab (Continue with Google works here), then open the extension (its icon
or the side panel) and sign in there too.</li>
<li><b>Google</b> (optional): sign in if you want your searches signed in.</li>
<li><b>Close this window.</b> The app opens the research browser again, connected, with your sign-ins.</li>
</ol>
</main></body></html>
)";
    return page.string();
}

void ResearchBrowser::signIn(std::function<void(const std::string&)> done) {
    if (settings.profile == "incognito") {
        done("the research browser runs incognito (its settings): nothing stays signed in there");
        return;
    }
    if (launching) {
        done("the research browser is starting: try again in a moment");
        return;
    }
    std::vector<std::string> urls = {"file://" + setupPage()};
    if (settings.offerExtension && !extensionInstalled()) {
        urls.push_back(std::string("https://chromewebstore.google.com/detail/") + EXTENSION_ID);
    }
    for (const auto& u: settings.signInPages) {
        urls.push_back(u);
    }
    bool withPort = false;
    const int pid = profilePid(&withPort);
    if (pid > 0 && !withPort) {
        launchPlain(urls, done);  // (open for signing in already: its pages shown again)
        return;
    }
    if (pid > 0) {
        reconnectAfterSignIn = true;  // (it ran: it comes back connected)
        std::weak_ptr<bool> token = alive;
        shutDown([this, token, urls, done](const std::string& error) {
            if (!token.lock()) {
                return;
            }
            if (!error.empty()) {
                done(error);
                return;
            }
            launchPlain(urls, done);
        });
        return;
    }
    launchPlain(urls, done);
}

void ResearchBrowser::launchPlain(const std::vector<std::string>& urls, std::function<void(const std::string&)> done) {
    std::error_code ec;
    const fs::path dir = settings.profileDir;
    fs::create_directories(dir, ec);
    fs::remove(dir / "DevToolsActivePort", ec);
    prepareProfile(dir);
    port = 0;
    // On this workspace, where he signs in; no DevTools port (the point)
    std::vector<std::string> args = {settings.chrome, "--user-data-dir=" + dir.string(),
                                     std::string("--class=") + WINDOW_CLASS, "--no-first-run",
                                     "--no-default-browser-check", "--new-window"};
    if (!settings.passwordStore.empty()) {
        args.push_back("--password-store=" + settings.passwordStore);
    }
    for (const auto& a: settings.extraArgs) {
        args.push_back(a);
    }
    for (const auto& u: urls) {
        args.push_back(u);
    }
    std::vector<const char*> argv;
    for (const auto& a: args) {
        argv.push_back(a.c_str());
    }
    argv.push_back(nullptr);
    GError* err = nullptr;
    GSubprocess* p = g_subprocess_newv(argv.data(),
                                       static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                                                     G_SUBPROCESS_FLAGS_STDERR_SILENCE),
                                       &err);
    if (!p) {
        const std::string e = std::string("the browser could not be started (") + settings.chrome +
                              "): " + (err ? err->message : "?");
        g_clear_error(&err);
        done(e);
        return;
    }
    if (process) {
        g_object_unref(process);
    }
    process = p;
    signInOpen = true;
    watchSignIn();
    done("");
}

void ResearchBrowser::watchSignIn() {
    if (signInWatch) {
        return;
    }
    // Once a second: whether a Chrome still holds its profile (it may take a moment to start: 5 s; if it is slower
    // than that, the next start finds it open without the port and watches it again)
    struct Watch {
        ResearchBrowser* self;
        int startup = 5;
        bool seen = false;
    };
    signInWatch = g_timeout_add_seconds_full(
            G_PRIORITY_DEFAULT, 1,
            +[](gpointer d) -> gboolean {
                auto* w = static_cast<Watch*>(d);
                ResearchBrowser* self = w->self;
                if (self->profilePid() > 0) {
                    w->seen = true;
                    return G_SOURCE_CONTINUE;
                }
                if (!w->seen && --w->startup > 0) {
                    return G_SOURCE_CONTINUE;
                }
                // closed: back with the app's connection, if it ran before (or this was its first setup)
                self->signInWatch = 0;
                self->signInOpen = false;
                const bool reconnect = self->reconnectAfterSignIn;
                self->reconnectAfterSignIn = false;
                if (!reconnect) {
                    if (self->onSignInClosed) {
                        self->onSignInClosed(false);
                    }
                    return G_SOURCE_REMOVE;
                }
                std::weak_ptr<bool> token = self->alive;
                self->ensure([self, token](const std::string& error) {
                    if (token.lock() && self->onSignInClosed) {
                        self->onSignInClosed(error.empty());
                    }
                });
                return G_SOURCE_REMOVE;
            },
            new Watch{this}, +[](gpointer d) { delete static_cast<Watch*>(d); });
}

void ResearchBrowser::prepareProfile(const fs::path& dir) {
    std::error_code ec;
    // The Claude in Chrome extension talks to Claude Code through a native messaging host; a Chrome with a profile
    // folder of its own looks for its manifest in that folder: a copy of the owner's (his files are only read)
    const fs::path home = g_get_home_dir();
    for (const fs::path& from: {home / ".config" / "google-chrome" / "NativeMessagingHosts",
                                home / ".config" / "chromium" / "NativeMessagingHosts"}) {
        if (!fs::is_directory(from, ec)) {
            continue;
        }
        for (const auto& f: fs::directory_iterator(from, ec)) {
            const std::string name = f.path().filename().string();
            if (name.rfind("com.anthropic.", 0) != 0 || f.path().extension() != ".json") {
                continue;
            }
            fs::create_directories(dir / "NativeMessagingHosts", ec);
            fs::copy_file(f.path(), dir / "NativeMessagingHosts" / name, fs::copy_options::overwrite_existing, ec);
        }
    }
    // Its name, so that agents tell it from his own Chrome among the browsers the extension connects (first run)
    const fs::path prefs = dir / "Default" / "Preferences";
    if (!fs::exists(prefs, ec)) {
        fs::create_directories(prefs.parent_path(), ec);
        std::ofstream out(prefs);
        out << json({{"profile", {{"name", PROFILE_NAME}}}}).dump() << "\n";
    }
}

bool ResearchBrowser::extensionInstalled() const {
    std::error_code ec;
    return fs::is_directory(settings.profileDir / "Default" / "Extensions" / EXTENSION_ID, ec);
}

void ResearchBrowser::waitForPort(int tries, std::function<void(const std::string&)> ready) {
    std::weak_ptr<bool> token = alive;
    struct Wait {
        std::weak_ptr<bool> token;
        ResearchBrowser* self;
        int tries;
        std::function<void(const std::string&)> ready;
    };
    g_timeout_add(
            200,
            +[](gpointer d) -> gboolean {
                std::unique_ptr<Wait> w(static_cast<Wait*>(d));
                if (!w->token.lock()) {
                    return G_SOURCE_REMOVE;
                }
                if (int p = w->self->readPortFile(); p > 0) {
                    w->self->port = p;
                    w->ready("");
                } else if (w->tries <= 0) {
                    w->ready("the browser did not answer (no DevTools port)");
                } else {
                    w->self->waitForPort(w->tries - 1, std::move(w->ready));
                }
                return G_SOURCE_REMOVE;
            },
            new Wait{token, this, tries, std::move(ready)});
}

void ResearchBrowser::place() {
#ifdef ENABLE_X11
    // One monitor: its window goes to the next workspace (or the one asked for), where he works with it
    if (settings.where == "here") {
        return;
    }
    GdkDisplay* display = gdk_display_get_default();
    if (!display || !GDK_IS_X11_DISPLAY(display)) {
        return;
    }
    struct Try {
        std::weak_ptr<bool> token;
        ResearchBrowser* self;
        int left;
    };
    g_timeout_add(
            400,
            +[](gpointer d) -> gboolean {
                auto* t = static_cast<Try*>(d);
                if (!t->token.lock() || t->left-- <= 0) {
                    delete t;
                    return G_SOURCE_REMOVE;
                }
                GdkDisplay* gd = gdk_display_get_default();
                Display* dpy = GDK_DISPLAY_XDISPLAY(gd);
                const Window root = DefaultRootWindow(dpy);
                const Atom clientList = XInternAtom(dpy, "_NET_CLIENT_LIST", True);
                const Atom current = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", True);
                if (clientList == None || current == None) {
                    delete t;  // (no window manager that knows workspaces: nothing to do)
                    return G_SOURCE_REMOVE;
                }
                gdk_x11_display_error_trap_push(gd);
                Atom type;
                int format;
                unsigned long n = 0, after = 0;
                unsigned char* data = nullptr;
                Window found = 0;
                if (XGetWindowProperty(dpy, root, clientList, 0, 4096, False, XA_WINDOW, &type, &format, &n, &after,
                                       &data) == Success &&
                    data) {
                    auto* windows = reinterpret_cast<Window*>(data);
                    for (unsigned long i = 0; i < n && !found; i++) {
                        XClassHint hint{};
                        if (XGetClassHint(dpy, windows[i], &hint)) {
                            auto is = [](const char* s) { return s && g_ascii_strcasecmp(s, WINDOW_CLASS) == 0; };
                            if (is(hint.res_name) || is(hint.res_class)) {
                                found = windows[i];
                            }
                            XFree(hint.res_name);
                            XFree(hint.res_class);
                        }
                    }
                    XFree(data);
                }
                long desktop = -1;
                if (found) {
                    const std::string& where = t->self->settings.where;
                    if (where.rfind("workspace:", 0) == 0) {
                        desktop = std::max(0, std::atoi(where.c_str() + 10) - 1);
                    } else {
                        unsigned char* cur = nullptr;
                        if (XGetWindowProperty(dpy, root, current, 0, 1, False, XA_CARDINAL, &type, &format, &n, &after,
                                               &cur) == Success &&
                            cur && n == 1) {
                            desktop = *reinterpret_cast<long*>(cur) + 1;
                        }
                        if (cur) {
                            XFree(cur);
                        }
                    }
                }
                if (found && desktop >= 0) {
                    XEvent ev{};
                    ev.xclient.type = ClientMessage;
                    ev.xclient.window = found;
                    ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
                    ev.xclient.format = 32;
                    ev.xclient.data.l[0] = desktop;
                    ev.xclient.data.l[1] = 2;  // (as a pager would)
                    XSendEvent(dpy, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &ev);
                    XFlush(dpy);
                }
                gdk_x11_display_error_trap_pop_ignored(gd);
                if (found) {
                    delete t;
                    return G_SOURCE_REMOVE;
                }
                return G_SOURCE_CONTINUE;
            },
            new Try{alive, this, 40});
#endif
}

void ResearchBrowser::http(const std::string& method, const std::string& path, Done done) {
    if (port <= 0) {
        done(nullptr, "the browser does not run");
        return;
    }
    const std::string uri = "http://127.0.0.1:" + std::to_string(port) + path;
    SoupMessage* msg = soup_message_new(method.c_str(), uri.c_str());
    if (!msg) {
        done(nullptr, "a bad address: " + path);
        return;
    }
    struct Call {
        std::weak_ptr<bool> token;
        SoupMessage* msg;
        Done done;
    };
    soup_session_send_and_read_async(
            session, msg, G_PRIORITY_DEFAULT, nullptr,
            +[](GObject* src, GAsyncResult* res, gpointer d) {
                std::unique_ptr<Call> c(static_cast<Call*>(d));
                GError* err = nullptr;
                GBytes* body = soup_session_send_and_read_finish(SOUP_SESSION(src), res, &err);
                const guint status = soup_message_get_status(c->msg);
                std::string text;
                if (body) {
                    gsize size = 0;
                    const auto* p = static_cast<const char*>(g_bytes_get_data(body, &size));
                    text.assign(p ? p : "", size);
                    g_bytes_unref(body);
                }
                g_object_unref(c->msg);
                if (!c->token.lock()) {
                    g_clear_error(&err);
                    return;
                }
                if (err || status < 200 || status >= 300) {
                    const std::string e = err ? err->message : "HTTP " + std::to_string(status) + " " + text;
                    g_clear_error(&err);
                    c->done(nullptr, e);
                    return;
                }
                json j = json::parse(text, nullptr, false);
                c->done(j.is_discarded() ? json(text) : j, "");
            },
            new Call{alive, msg, std::move(done)});
}

void ResearchBrowser::open(const std::string& url, const std::string& topic, Done done) {
    std::weak_ptr<bool> token = alive;
    // A tab in the background (Target.createTarget): /json/new made it the active one, and Chrome came to the front
    // with it (the owner, 10 Nov: it took focus, even moved him to its workspace)
    browserPath([this, token, url, topic, done](const std::string& path, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        if (!error.empty()) {
            done(nullptr, error);
            return;
        }
        command(path, "Target.createTarget", {{"url", url}, {"background", true}},
                [this, token, url, topic, done](const json& r, const std::string& error) {
                    if (!token.lock()) {
                        return;
                    }
                    if (!error.empty()) {
                        done(nullptr, error);
                        return;
                    }
                    const std::string id = r.value("targetId", "");
                    if (!topic.empty()) {
                        topics[id] = topic;
                    }
                    done({{"tab", id}, {"url", url}, {"topic", topic}}, "");
                });
    });
}

void ResearchBrowser::browserPath(std::function<void(const std::string& path, const std::string& error)> done) {
    std::weak_ptr<bool> token = alive;
    http("GET", "/json/version", [token, done](const json& v, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        const std::string ws = v.is_object() ? v.value("webSocketDebuggerUrl", "") : "";
        const auto at = ws.find("/devtools/browser/");
        if (!error.empty() || at == std::string::npos) {
            done("", error.empty() ? "the browser gave no connection of its own" : error);
            return;
        }
        done(ws.substr(at), "");
    });
}

void ResearchBrowser::keepAwake(const fs::path& dir) {
    // Its profile's own settings (Chrome 154's names; read while no Chrome holds the profile): Memory Saver and
    // Energy Saver off (a tab unloaded comes back without its state), no freezing, foreground priority for every tab
    std::error_code ec;
    const fs::path prefs = dir / "Default" / "Preferences";
    json p = json::object();
    if (fs::exists(prefs, ec)) {
        std::ifstream in(prefs);
        p = json::parse(in, nullptr, false);
        if (!p.is_object()) {
            return;  // (unreadable: left as it is)
        }
    }
    json& t = p["performance_tuning"];
    if (!t.is_object()) {
        t = json::object();
    }
    t["high_efficiency_mode"]["state"] = 0;
    t["high_efficiency_mode"]["enabled"] = false;
    t["battery_saver_mode"]["state"] = 0;
    t["tab_freezing"]["enabled"] = false;
    t["force_foreground_priority_for_all_tabs"] = true;
    fs::create_directories(prefs.parent_path(), ec);
    std::ofstream out(prefs);
    out << p.dump() << "\n";
}

void ResearchBrowser::list(Done done) {
    std::weak_ptr<bool> token = alive;
    http("GET", "/json/list", [this, token, done](const json& all, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        if (!error.empty()) {
            done(nullptr, error);
            return;
        }
        json tabs = json::array();
        for (const auto& t: all.is_array() ? all : json::array()) {
            if (t.value("type", "") != "page") {
                continue;
            }
            json tab = {{"tab", t.value("id", "")}, {"url", t.value("url", "")}, {"title", t.value("title", "")}};
            if (auto it = topics.find(t.value("id", "")); it != topics.end()) {
                tab["topic"] = it->second;
            }
            tabs.push_back(std::move(tab));
        }
        done({{"tabs", tabs}}, "");
    });
}

void ResearchBrowser::close(const std::string& tab, Done done) {
    topics.erase(tab);
    http("GET", "/json/close/" + tab, [done](const json&, const std::string& error) { done(nullptr, error); });
}

void ResearchBrowser::evaluate(const std::string& tab, const std::string& expression, Done done) {
    // the value returned as JSON text
    command("/devtools/page/" + tab, "Runtime.evaluate",
            {{"expression", expression}, {"returnByValue", true}, {"awaitPromise", true}},
            [done](const json& r, const std::string& error) {
                if (!error.empty()) {
                    done(nullptr, error);
                } else if (r.contains("exceptionDetails")) {
                    done(nullptr, "the page said: " + r["exceptionDetails"].value("text", std::string("?")));
                } else {
                    const json& v = r.contains("result") ? r["result"] : json();
                    done(v.contains("value") ? v["value"] : json(), "");
                }
            });
}

void ResearchBrowser::command(const std::string& path, const std::string& method, const json& params, Done done) {
    if (port <= 0) {
        done(nullptr, "the browser does not run");
        return;
    }
    // one DevTools connection per command
    const std::string uri = "ws://127.0.0.1:" + std::to_string(port) + path;
    SoupMessage* msg = soup_message_new("GET", uri.c_str());
    if (!msg) {
        done(nullptr, "no such target: " + path);
        return;
    }
    struct Ask {
        std::weak_ptr<bool> token;
        std::string request;
        Done done;
        bool answered = false;
        SoupWebsocketConnection* conn = nullptr;
        guint timeout = 0;
        void answer(const json& v, const std::string& e) {
            if (answered) {
                return;
            }
            answered = true;
            if (timeout) {
                g_source_remove(timeout);
                timeout = 0;
            }
            if (token.lock()) {
                done(v, e);
            }
        }
    };
    auto* ask = new Ask{alive, json({{"id", 1}, {"method", method}, {"params", params}}).dump(), std::move(done)};
    ask->timeout = g_timeout_add_seconds(
            15,
            +[](gpointer d) -> gboolean {
                auto* a = static_cast<Ask*>(d);
                a->timeout = 0;
                a->answer(nullptr, "the browser did not answer");
                if (a->conn) {
                    soup_websocket_connection_close(a->conn, SOUP_WEBSOCKET_CLOSE_NORMAL, nullptr);
                }
                return G_SOURCE_REMOVE;
            },
            ask);
    soup_session_websocket_connect_async(
            session, msg, nullptr, nullptr, G_PRIORITY_DEFAULT, nullptr,
            +[](GObject* src, GAsyncResult* res, gpointer d) {
                auto* a = static_cast<Ask*>(d);
                GError* err = nullptr;
                SoupWebsocketConnection* conn = soup_session_websocket_connect_finish(SOUP_SESSION(src), res, &err);
                if (!conn) {
                    a->answer(nullptr, std::string("no connection to the browser: ") + (err ? err->message : "?"));
                    g_clear_error(&err);
                    delete a;  // (answered: its timeout is gone)
                    return;
                }
                a->conn = conn;
                soup_websocket_connection_set_max_incoming_payload_size(conn, 64 * 1024 * 1024);
                g_signal_connect(conn, "message",
                                 G_CALLBACK(+[](SoupWebsocketConnection* c, gint, GBytes* bytes, gpointer d) {
                                     auto* a = static_cast<Ask*>(d);
                                     gsize size = 0;
                                     const auto* p = static_cast<const char*>(g_bytes_get_data(bytes, &size));
                                     json m = json::parse(std::string(p ? p : "", size), nullptr, false);
                                     if (m.is_discarded() || m.value("id", 0) != 1) {
                                         return;
                                     }
                                     if (m.contains("error")) {
                                         a->answer(nullptr, m["error"].dump());
                                     } else {
                                         a->answer(m.contains("result") ? m["result"] : json::object(), "");
                                     }
                                     soup_websocket_connection_close(c, SOUP_WEBSOCKET_CLOSE_NORMAL, nullptr);
                                 }),
                                 a);
                g_signal_connect(conn, "closed", G_CALLBACK(+[](SoupWebsocketConnection* c, gpointer d) {
                                     auto* a = static_cast<Ask*>(d);
                                     a->answer(nullptr, "the browser closed the connection");
                                     g_object_unref(c);
                                     delete a;
                                 }),
                                 a);
                soup_websocket_connection_send_text(conn, a->request.c_str());
            },
            ask);
    g_object_unref(msg);
}

void ResearchBrowser::wake(const std::string& tab, std::function<void()> then) {
    // (a tab Chrome froze is woken first; nothing happens to one that runs)
    std::weak_ptr<bool> token = alive;
    command("/devtools/page/" + tab, "Page.setWebLifecycleState", {{"state", "active"}},
            [token, then](const json&, const std::string&) {
                if (token.lock()) {
                    then();
                }
            });
}

void ResearchBrowser::read(const std::string& tab, size_t maxChars, Done done) {
    std::weak_ptr<bool> token = alive;
    wake(tab, [this, token, tab, maxChars, done]() { readAwake(tab, maxChars, done); });
}

void ResearchBrowser::readAwake(const std::string& tab, size_t maxChars, Done done) {
    std::weak_ptr<bool> token = alive;
    evaluate(tab, readExpression(std::clamp<size_t>(maxChars, 200, 60000)),
             [this, token, tab, done](const json& v, const std::string& error) {
                 if (!token.lock()) {
                     return;
                 }
                 if (!error.empty() || !v.is_string()) {
                     done(nullptr, error.empty() ? "the tab gave nothing" : error);
                     return;
                 }
                 json r = json::parse(v.get<std::string>(), nullptr, false);
                 if (r.is_discarded()) {
                     done(nullptr, "the tab gave something unreadable");
                     return;
                 }
                 r["tab"] = tab;
                 if (auto it = topics.find(tab); it != topics.end()) {
                     r["topic"] = it->second;
                 }
                 done(r, "");
             });
}

void ResearchBrowser::saveImage(const std::string& tab, int n, const fs::path& folder, Done done) {
    std::weak_ptr<bool> token = alive;
    const std::string expr = std::string("(() => { const i = ") + PICTURES + "[" + std::to_string(n) +
                             "]; return JSON.stringify(i ? {src: i.currentSrc || i.src, width: i.naturalWidth, "
                             "height: i.naturalHeight, alt: i.alt || '', page: location.href, title: document.title} "
                             ": null); })()";
    evaluate(tab, expr, [this, token, folder, done](const json& v, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        json info = v.is_string() ? json::parse(v.get<std::string>(), nullptr, false) : json();
        if (!error.empty() || !info.is_object()) {
            done(nullptr, error.empty() ? "no such picture in the tab (see read: pictures)" : error);
            return;
        }
        const std::string src = info.value("src", "");
        std::error_code ec;
        fs::create_directories(folder, ec);
        auto finish = [info, folder, done](const std::string& bytes, const std::string& type) {
            const std::string ext = type.find("png") != std::string::npos  ? ".png" :
                                    type.find("gif") != std::string::npos  ? ".gif" :
                                    type.find("webp") != std::string::npos ? ".webp" :
                                    type.find("svg") != std::string::npos  ? ".svg" :
                                                                             ".jpg";
            gchar* sum = g_compute_checksum_for_string(G_CHECKSUM_SHA1, bytes.data(), static_cast<gssize>(bytes.size()));
            const fs::path path = folder / (std::string(sum).substr(0, 16) + ext);
            g_free(sum);
            std::ofstream out(path, std::ios::binary);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!out) {
                done(nullptr, "could not write " + path.string());
                return;
            }
            done({{"path", path.string()},
                  {"width", info.value("width", 0)},
                  {"height", info.value("height", 0)},
                  {"alt", info.value("alt", "")},
                  {"source", info.value("page", "")},
                  {"title", info.value("title", "")}},
                 "");
        };
        if (src.rfind("data:", 0) == 0) {  // (a thumbnail in the page itself)
            const auto comma = src.find(',');
            const std::string head = src.substr(0, comma);
            gsize len = 0;
            guchar* raw = g_base64_decode(src.substr(comma + 1).c_str(), &len);
            std::string bytes(reinterpret_cast<char*>(raw), len);
            g_free(raw);
            finish(bytes, head);
            return;
        }
        SoupMessage* msg = soup_message_new("GET", src.c_str());
        if (!msg) {
            done(nullptr, "the picture's address can't be fetched: " + src.substr(0, 120));
            return;
        }
        auto* h = soup_message_get_request_headers(msg);
        soup_message_headers_replace(h, "User-Agent", USER_AGENT);
        soup_message_headers_replace(h, "Referer", info.value("page", "").c_str());
        struct Get {
            std::weak_ptr<bool> token;
            SoupMessage* msg;
            std::function<void(const std::string&, const std::string&)> finish;
            Done done;
        };
        soup_session_send_and_read_async(
                session, msg, G_PRIORITY_DEFAULT, nullptr,
                +[](GObject* src, GAsyncResult* res, gpointer d) {
                    std::unique_ptr<Get> g(static_cast<Get*>(d));
                    GError* err = nullptr;
                    GBytes* body = soup_session_send_and_read_finish(SOUP_SESSION(src), res, &err);
                    const guint status = soup_message_get_status(g->msg);
                    const char* type =
                            soup_message_headers_get_content_type(soup_message_get_response_headers(g->msg), nullptr);
                    std::string bytes;
                    if (body) {
                        gsize size = 0;
                        const auto* p = static_cast<const char*>(g_bytes_get_data(body, &size));
                        bytes.assign(p ? p : "", size);
                        g_bytes_unref(body);
                    }
                    const std::string t = type ? type : "";
                    g_object_unref(g->msg);
                    if (!g->token.lock()) {
                        g_clear_error(&err);
                        return;
                    }
                    if (err || status != 200 || bytes.empty()) {
                        g->done(nullptr, std::string("the picture could not be fetched: ") +
                                                 (err ? err->message : "HTTP " + std::to_string(status)));
                        g_clear_error(&err);
                        return;
                    }
                    g->finish(bytes, t);
                },
                new Get{alive, msg, finish, done});
    });
}

void ResearchBrowser::selection(Done done) {
    std::weak_ptr<bool> token = alive;
    list([this, token, done](const json& r, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        if (!error.empty()) {
            done(nullptr, error);
            return;
        }
        const json tabs = r.value("tabs", json::array());
        if (tabs.empty()) {
            done(nullptr, "no tabs");
            return;
        }
        // asks every tab; the one he looks at wins, else any with a selection
        struct Gather {
            size_t left;
            json best;
            bool bestVisible = false;
            Done done;
        };
        auto g = std::make_shared<Gather>(Gather{tabs.size(), nullptr, false, done});
        const std::string expr = "JSON.stringify({text: String(window.getSelection ? window.getSelection() : ''), "
                                 "url: location.href, title: document.title, visible: document.visibilityState === "
                                 "'visible', focused: document.hasFocus()})";
        for (const auto& t: tabs) {
            evaluate(t.value("tab", ""), expr, [g](const json& v, const std::string&) {
                json s = v.is_string() ? json::parse(v.get<std::string>(), nullptr, false) : json();
                if (s.is_object() && !s.value("text", "").empty()) {
                    const bool visible = s.value("visible", false) || s.value("focused", false);
                    if (g->best.is_null() || (visible && !g->bestVisible)) {
                        g->best = s;
                        g->bestVisible = visible;
                    }
                }
                if (--g->left == 0) {
                    g->done(g->best, g->best.is_null() ? "nothing is selected in the browser" : "");
                }
            });
        }
    });
}

}  // namespace xoj::assistant
