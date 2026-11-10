/*
 * xournalai (based on Xournal++)
 *
 * The research browser (🔎 Browser assistant; the owner's decisions 2026-11-09, docs/mcp/LIVE_MODES.md): a Chrome of
 * the app's own, with a profile in the app's folder (the owner signs in to Google once; none of his other accounts),
 * on another monitor if there are several, else on the next workspace. Agents search in its tabs (the browser does
 * the searching, not the model), read what they need and propose quotes and pictures; the owner sees every tab and
 * can open it. Driven through the Chrome DevTools protocol on a local port: open, list, read (evaluate) and close
 * tabs, take a picture. Clicks, typing and a site's navigation are the agents' part, through the Claude in Chrome
 * extension in the same profile (prepareProfile gives it the owner's native-messaging manifest).
 *
 * Settings (mcp.json assistant.research_browser): chrome (the command), profile ("own" | "incognito"), where ("auto" |
 * "here" | "workspace:N" | "monitor:N"), password_store ("" = the system's | "basic"), search (kind → URL template
 * with {q}: web, images, wiki, scholar).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for shared_ptr
#include <string>      // for string
#include <vector>      // for vector

#include <gio/gio.h>

#include <gtk/gtk.h>

#include "util/PathUtil.h"  // for fs

#include "mcp/Json.h"

typedef struct _SoupSession SoupSession;

namespace xoj::assistant {

class ResearchBrowser final {
public:
    struct Settings {
        std::string chrome = "google-chrome";
        std::string profile = "own";  ///< own | incognito
        fs::path profileDir;          ///< the own profile's folder (in the app's data folder)
        std::string where = "auto";   ///< auto | here | workspace:N | monitor:N
        std::string passwordStore;    ///< "" (the system's) | basic
        std::vector<std::string> extraArgs;  ///< more flags for Chrome (tests: no background networking)
        bool closeOnExit = false;            ///< ends the browser it started when the app quits (tests)
        bool offerExtension = true;  ///< opens the Claude in Chrome extension's store page when it isn't installed
        std::map<std::string, std::string> search;  ///< kind → URL template with {q}
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
    void close(const std::string& tab, Done done);
    /// The owner's selection in the tab he looks at (or any tab with one): text, url, title
    void selection(Done done);
    /// The Claude in Chrome extension is installed in its profile (agents click, type and navigate through it)
    bool extensionInstalled() const;
    static constexpr const char* EXTENSION_ID = "fcoeoabgfenejglbffodgkkbkcdhcgfn";
    static constexpr const char* PROFILE_NAME = "xournalai research";

private:
    void launch(std::function<void(const std::string&)> ready);
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
    GSubprocess* process = nullptr;  ///< the browser it started (closeOnExit)
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}  // namespace xoj::assistant
