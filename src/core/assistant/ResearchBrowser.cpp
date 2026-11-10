#include "ResearchBrowser.h"

#include <algorithm>  // for clamp
#include <csignal>    // for SIGTERM
#include <fstream>    // for ifstream, ofstream

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
    return std::string("(() => { const pics = ") + PICTURES +
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
    if (launching) {
        waiting.push_back(std::move(ready));
        return;
    }
    // (it may still run: from before, or the app restarted; else it is started)
    const int known = port ? port : readPortFile();
    if (known <= 0) {
        launch(std::move(ready));
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
        launch(ready);
    });
}

void ResearchBrowser::launch(std::function<void(const std::string&)> ready) {
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
    for (const auto& a: settings.extraArgs) {
        args.push_back(a);
    }
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
        if (error.empty() && settings.offerExtension && !offered && !extensionInstalled()) {
            // (agents click, type and navigate through the Claude in Chrome extension: installed once, by him)
            offered = true;
            open(std::string("https://chromewebstore.google.com/detail/") + EXTENSION_ID, "install Claude in Chrome",
                 [](const json&, const std::string&) {});
        }
        auto w = std::move(waiting);
        waiting.clear();
        for (auto& f: w) {
            f(error);
        }
    });
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
    // (PUT: Chrome refuses a GET for /json/new; the address escaped whole: Chrome takes all of the query as it, and an
    // unescaped & ended it there)
    gchar* escaped = g_uri_escape_string(url.c_str(), nullptr, FALSE);
    const std::string path = std::string("/json/new?") + escaped;
    g_free(escaped);
    http("PUT", path, [this, token, topic, done](const json& t, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        if (!error.empty()) {
            done(nullptr, error);
            return;
        }
        const std::string id = t.value("id", "");
        if (!topic.empty()) {
            topics[id] = topic;
        }
        done({{"tab", id}, {"url", t.value("url", "")}, {"topic", topic}}, "");
    });
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
    if (port <= 0) {
        done(nullptr, "the browser does not run");
        return;
    }
    // one DevTools connection per question: Runtime.evaluate, the value returned as JSON text
    const std::string uri = "ws://127.0.0.1:" + std::to_string(port) + "/devtools/page/" + tab;
    SoupMessage* msg = soup_message_new("GET", uri.c_str());
    if (!msg) {
        done(nullptr, "no such tab: " + tab);
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
    auto* ask = new Ask{alive,
                        json({{"id", 1},
                              {"method", "Runtime.evaluate"},
                              {"params", {{"expression", expression}, {"returnByValue", true}, {"awaitPromise", true}}}})
                                .dump(),
                        std::move(done)};
    ask->timeout = g_timeout_add_seconds(
            15,
            +[](gpointer d) -> gboolean {
                auto* a = static_cast<Ask*>(d);
                a->timeout = 0;
                a->answer(nullptr, "the tab did not answer");
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
                    a->answer(nullptr, std::string("no connection to the tab: ") + (err ? err->message : "?"));
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
                                     const json& r = m.contains("result") ? m["result"] : json();
                                     if (m.contains("error")) {
                                         a->answer(nullptr, m["error"].dump());
                                     } else if (r.contains("exceptionDetails")) {
                                         a->answer(nullptr, "the page said: " +
                                                                    r["exceptionDetails"].value("text", std::string("?")));
                                     } else {
                                         const json& v = r.contains("result") ? r["result"] : json();
                                         a->answer(v.contains("value") ? v["value"] : json(), "");
                                     }
                                     soup_websocket_connection_close(c, SOUP_WEBSOCKET_CLOSE_NORMAL, nullptr);
                                 }),
                                 a);
                g_signal_connect(conn, "closed", G_CALLBACK(+[](SoupWebsocketConnection* c, gpointer d) {
                                     auto* a = static_cast<Ask*>(d);
                                     a->answer(nullptr, "the tab closed the connection");
                                     g_object_unref(c);
                                     delete a;
                                 }),
                                 a);
                soup_websocket_connection_send_text(conn, a->request.c_str());
            },
            ask);
    g_object_unref(msg);
}

void ResearchBrowser::read(const std::string& tab, size_t maxChars, Done done) {
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
