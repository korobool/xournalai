/*
 * xournalai (based on Xournal++)
 *
 * The research browser (🔎 Browser assistant; the owner's decisions 2026-11-09, docs/mcp/LIVE_MODES.md): a Chrome of
 * the app's own, with a profile in the app's folder (the owner signs in to Google once; none of his other accounts),
 * on another monitor if there are several, else behind the app on the same workspace (1.18.2: it never takes the focus,
 * FocusGuard; «🔎 Browser» / «↩ Back»; its tabs never sleep). Agents search in its tabs (the browser does
 * the searching, not the model), read what they need and propose quotes and pictures; the owner sees every tab and
 * can open it. Driven through the Chrome DevTools protocol on a local port: open, list, read (evaluate) and close
 * tabs, take a picture. Clicks, typing and a site's navigation are the agents' part, through the Claude in Chrome
 * extension in the same profile (prepareProfile gives it the owner's native-messaging manifest).
 *
 * Signing in (1.18.1): Google refuses to sign in a browser with a DevTools port ("This browser or app may not be
 * secure", the owner's first try, 10 Nov 2026), so for signing in the browser is restarted without it; once he closes
 * that window it comes back connected (the sign-ins stay in the profile). Its first start is such a sign-in.
 *
 * Settings (mcp.json assistant.research_browser): chrome (the command), profile ("own" | "incognito"), where ("auto" |
 * "behind" | "here" | "workspace[:N]" | "monitor:N"), focus_guard (true), password_store ("" = the system's | "basic"), search (kind → URL template
 * with {q}: web, images, wiki, scholar), sign_in_pages (opened for signing in; default claude.ai's login).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <map>         // for map
#include <set>         // for set
#include <memory>      // for shared_ptr
#include <string>      // for string
#include <vector>      // for vector

#include <gio/gio.h>

#include <gtk/gtk.h>

#include "util/PathUtil.h"  // for fs

#include "mcp/Json.h"

typedef struct _SoupSession SoupSession;
typedef struct _SoupWebsocketConnection SoupWebsocketConnection;

namespace xoj::assistant {

class ResearchBrowser final {
public:
    struct Settings {
        std::string chrome = "google-chrome";
        std::string profile = "own";  ///< own | incognito
        fs::path profileDir;          ///< the own profile's folder (in the app's data folder)
        std::string where = "auto";  ///< auto (another monitor, else behind the app) | behind | here | workspace[:N] |
                                     ///< monitor:N
        bool focusGuard = true;      ///< the owner is brought back when it takes the focus by itself
        std::string passwordStore;    ///< "" (the system's) | basic
        std::vector<std::string> extraArgs;  ///< more flags for Chrome (tests: no background networking)
        bool closeOnExit = false;            ///< ends the browser it started when the app quits (tests)
        bool offerExtension = true;  ///< opens the Claude in Chrome extension's store page when it isn't installed
        std::map<std::string, std::string> search;  ///< kind → URL template with {q}
        std::vector<std::string> signInPages = {"https://claude.ai/login"};  ///< opened for signing in
        static Settings fromJson(const nlohmann::json& j, const fs::path& dataDir);
    };
    using Done = std::function<void(const nlohmann::json& result, const std::string& error)>;

    ResearchBrowser(Settings settings, GtkWindow* appWindow);
    ~ResearchBrowser();
    ResearchBrowser(const ResearchBrowser&) = delete;
    ResearchBrowser& operator=(const ResearchBrowser&) = delete;

    /// Starts the browser if it doesn't run (or finds the one still running from before); `ready` when it answers
    void ensure(std::function<void(const std::string& error)> ready);
    bool running() const { return port > 0; }

    /// The search page for `query` (kind: web, images, wiki, scholar)
    std::string searchUrl(const std::string& kind, const std::string& query) const;
    /// A new tab with `url` (the topic is remembered with it)
    void open(const std::string& url, const std::string& topic, Done done);
    /// The tabs: id, url, title, topic
    void list(Done done);
    /// What a tab shows: title, url, the text (at most `maxChars`), its pictures, the owner's selection
    void read(const std::string& tab, size_t maxChars, Done done);
    /// Saves picture `n` of a tab (as `read` numbers them) into `folder`: path, width, height, source
    void saveImage(const std::string& tab, int n, const fs::path& folder, Done done);
    /// Closes a tab; one that is kept (★) or that he looked at only when he asked for it (`ownerAsked`)
    void close(const std::string& tab, bool ownerAsked, Done done);
    /// Closes the tabs: all but the kept ones and those he looked at (unless he asked); its first, empty tab stays
    void closeAll(bool ownerAsked, Done done);
    /// A target page (what was looked for, or what he asked for: the owner, 10 Nov): ★ in its title, never closed by
    /// an agent, open until he closes it. By tab, or by its address (a page an agent reached through the extension)
    void keep(const std::string& tab, const std::string& url, const std::string& topic, const std::string& note,
              Done done);
    /// The kept pages, by topic
    void sources(Done done);
    /// He looked at this tab (in front while he was in the browser): an agent doesn't close it
    void markSeen(const std::string& tab) { seen.insert(tab); }
    /// The tab in front of him now (he is in the browser) counts as seen
    void markVisibleSeen();
    /// Agents worked in it in the last few seconds (a tab opened, navigated, closed, looked at)
    bool busy() const;
    /// He is in the browser (no «N new» counted meanwhile) or not
    void ownerLooking(bool here);
    /// (the UI: «🔎 Browser · N new») tabs opened while he wasn't looking
    std::function<void(int newTabs)> onNewTabs;
    /// (the UI) the browser was closed (by him): the next use starts it again
    std::function<void()> onClosed;
    bool focusGuard() const { return settings.focusGuard; }
    /// The Chrome holding its profile (from its SingletonLock, checked to be alive and of this very profile), 0 if
    /// none; `withPort`: whether it has a DevTools port
    int profilePid(bool* withPort = nullptr) const;
    /// The owner's selection in the tab he looks at (or any tab with one): text, url, title
    void selection(Done done);
    /// The Claude in Chrome extension is installed in its profile (agents click, type and navigate through it)
    bool extensionInstalled() const;

    /// Opens it for signing in (Google, claude.ai, the Claude in Chrome extension): restarted without the app's DevTools
    /// port, which Google refuses to sign in with, on this workspace, with a page that says what to do. When he closes
    /// that window it comes back connected, if it ran before (or was set up for the first time). `done` once it shows.
    void signIn(std::function<void(const std::string& error)> done);
    /// It is open for signing in (the app can't read it then)
    bool signingIn() const { return signInOpen; }
    /// (the UI) the sign-in window was closed; `connected`: the browser is back with the app's connection
    std::function<void(bool connected)> onSignInClosed;
    /// What ensure() and the tool say while it is open for signing in
    static constexpr const char* SIGN_IN_OPEN =
            "the research browser is open for signing in (Google and Claude in Chrome refuse to sign in while the app "
            "is connected to it); once the owner closes that window it comes back connected";
    static constexpr const char* EXTENSION_ID = "fcoeoabgfenejglbffodgkkbkcdhcgfn";
    /// How agents find this browser among the ones connected to Claude in Chrome (every Chrome of his is listed, all
    /// under the host's name by default): the extension's own name for it (set to PROFILE_NAME when he has set none)
    /// and its device id (made when it first connects): installed, name, device_id
    void identity(Done done);
    static constexpr const char* PROFILE_NAME = "xournalai research";

private:
    void launch(std::function<void(const std::string&)> ready);
    /// Its profile's settings that keep tabs awake (Memory Saver, Energy Saver, freezing off)
    void keepAwake(const fs::path& dir);
    /// The browser's own DevTools connection (/devtools/browser/<id>)
    void browserPath(std::function<void(const std::string& path, const std::string& error)> done);
    void wake(const std::string& tab, std::function<void()> then);
    void readAwake(const std::string& tab, size_t maxChars, Done done);
    /// (the first start: set up, else launch)
    void start(std::function<void(const std::string&)> ready);
    /// Closes the browser (Browser.close; else SIGTERM to its verified pid) and waits until its profile is free
    void shutDown(std::function<void(const std::string&)> done);
    void waitForExit(int tries, std::function<void(bool exited)> done);
    void launchPlain(const std::vector<std::string>& urls, std::function<void(const std::string&)> done);
    void watchSignIn();
    std::string setupPage() const;
    /// One DevTools command over a WebSocket (path: /devtools/page/<id> or /devtools/browser/<id>): its result
    void command(const std::string& path, const std::string& method, const nlohmann::json& params, Done done);
    bool signInOpen = false;
    bool reconnectAfterSignIn = false;
    guint signInWatch = 0;
    void prepareProfile(const fs::path& dir);
    bool offered = false;  ///< (the store page, once a run)
    void waitForPort(int tries, std::function<void(const std::string&)> ready);
    void place();
    void http(const std::string& method, const std::string& path, Done done);
    void evaluate(const std::string& tab, const std::string& expression, Done done);
    int readPortFile() const;

    Settings settings;
    GtkWindow* appWindow;
    SoupSession* session = nullptr;
    int port = 0;
    bool launching = false;
    std::vector<std::function<void(const std::string&)>> waiting;  ///< (ensure() while it starts)
    std::map<std::string, std::string> topics;                     ///< tab → topic
    struct Kept {
        std::string topic, note, url, title;
    };
    std::map<std::string, Kept> kept;  ///< tab → why it is kept (a ★ in a title counts too: after a restart)
    std::set<std::string> seen;        ///< the tabs he looked at
    /// Whether a tab of /json/list is kept, or he looked at it
    bool isKept(const nlohmann::json& tab) const;
    bool isSeen(const std::string& tab) const { return seen.count(tab) > 0; }
    void closeTab(const std::string& tab, Done done);
    /// (connected) its events: Target.setDiscoverTargets on a connection of its own
    void connected();
    void watchTargets();
    void targetEvent(GBytes* bytes);
    void checkClosedLater();
    SoupWebsocketConnection* watcher = nullptr;
    bool connectingWatcher = false;
    std::set<std::string> knownTargets;  ///< (the pages there before the watch: not new)
    gint64 lastActivity = 0;
    int newTabs = 0;
    bool ownerHere = false;
    void appendSource(const Kept& k) const;
    GSubprocess* process = nullptr;  ///< the browser it started (closeOnExit)
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}  // namespace xoj::assistant
