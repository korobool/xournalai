// Tool: research (the research browser: searches in tabs the owner sees; the cheap reading path)

#include <memory>  // for shared_ptr

#include "assistant/ResearchBrowser.h"  // for ResearchBrowser
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"  // for McpUi
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerResearchTools(McpServer& server) {
    McpServer* srv = &server;
    ToolSpec t;
    t.name = "research";
    t.title = "The research browser (search in tabs the owner sees)";
    t.description =
            "The app's research browser: a Chrome of its own (its own profile \"xournalai research\", on another "
            "monitor or workspace) where searches open as tabs the owner can see and open. The browser does the "
            "searching; you read only what you need. To click, type, navigate a site or switch tabs, use the Claude in "
            "Chrome tools (mcp__claude-in-chrome__*) on this browser: its profile has the extension (list says whether "
            "it is installed); with several browsers connected, pick the one of the profile \"xournalai research\" "
            "(list_connected_browsers / select_browser) and work in the extension's tab group. This tool is the cheap "
            "way to search and read. op=open: query + kind (web | images | wiki | scholar), or url; topic (what it is for, e.g. \"T3 "
            "notations\") → its tab. op=list: the tabs (tab, url, title, topic). op=read: tab, max_chars (default "
            "6000) → title, url, the page's text, its pictures (n, src, alt, width, height) and the owner's selection. "
            "op=image: tab, n (a picture's number from read) → it is saved to a file: path, width, height, source, "
            "title; place it with create_image(path, …) and a small source line under it. op=close: tab, or all=true. "
            "This tool itself only opens, reads and closes tabs and saves pictures. Never log in, buy, pay or type "
            "passwords there; posting, sending, uploading or downloading anything else needs the owner's tap first "
            "(ask_owner). Close the tabs you no longer need; keep a few at a time.";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("Operation", {"open", "list", "read", "image", "close"})},
             {"query", schema::string("open: what to search for")},
             {"kind", schema::enumeration("open: where to search", {"web", "images", "wiki", "scholar"})},
             {"url", schema::string("open: a page to open instead of a search")},
             {"topic", schema::string("open: what the tab is for (shown in list)")},
             {"tab", schema::string("read/image/close: the tab")},
             {"max_chars", schema::integer("read: at most this much text (default 6000)")},
             {"n", schema::integer("image: the picture's number (from read)")},
             {"all", schema::boolean("close: every tab")}},
            {"op"});
    t.tier = Tier::Ui;
    t.asyncHandler = [srv](const json& j, Responder respond) {
        Args args(j);
        args.rejectUnknown({"op", "query", "kind", "url", "topic", "tab", "max_chars", "n", "all"});
        const std::string op = args.choice("op", {"open", "list", "read", "image", "close"}, "");
        McpUi* ui = srv->getUi();
        assistant::ResearchBrowser* b = ui ? ui->research() : nullptr;
        if (!b) {
            throw ToolError("no research browser in this window");
        }
        auto reply = [respond](const json& r, const std::string& error) {
            respond(error.empty() ? ToolResult::structured(r.is_null() ? json::object() : r) : ToolResult::error(error));
        };
        if (op == "open") {
            const std::string url = args.has("url") ? args.str("url") :
                                                      b->searchUrl(args.str("kind", "web"), args.str("query"));
            const std::string topic = args.str("topic", "");
            ui->researchStarting();
            b->ensure([b, url, topic, reply, ui](const std::string& error) {
                if (!error.empty()) {
                    reply(nullptr, error);
                    return;
                }
                ui->researchRunning(true);
                b->open(url, topic, reply);
            });
            return;
        }
        if (!b->running()) {
            // (it may still run from before: found again)
            b->ensure([](const std::string&) {});
        }
        if (op == "list") {
            b->list([b, reply](const json& r, const std::string& error) {
                json out = r.is_object() ? r : json::object();
                out["browser"] = assistant::ResearchBrowser::PROFILE_NAME;
                out["extension"] = b->extensionInstalled() ? "installed" :
                                                             "not installed yet (its store page opens in this browser; "
                                                             "the owner installs it once)";
                reply(out, error);
            });
        } else if (op == "read") {
            b->read(args.str("tab"), static_cast<size_t>(args.integer("max_chars", 6000, 200, 60000)), reply);
        } else if (op == "image") {
            const fs::path folder = fs::path(g_get_user_data_dir()) / "xournalai" / "research" / "images";
            b->saveImage(args.str("tab"), static_cast<int>(args.integer("n", 0, 0, 1000)), folder, reply);
        } else if (args.boolean("all", false)) {
            b->list([b, reply](const json& r, const std::string& error) {
                if (!error.empty()) {
                    reply(nullptr, error);
                    return;
                }
                json closed = json::array();
                for (const auto& tab: r.value("tabs", json::array())) {
                    if (tab.value("url", "") == "about:blank" && !tab.contains("topic")) {
                        continue;  // (its first, empty tab: the window stays)
                    }
                    b->close(tab.value("tab", ""), [](const json&, const std::string&) {});
                    closed.push_back(tab.value("tab", ""));
                }
                reply({{"closed", closed}}, "");
            });
        } else {
            b->close(args.str("tab"), [reply](const json&, const std::string& error) {
                reply({{"closed", true}}, error);
            });
        }
    };
    server.getRegistry().addTool(std::move(t));
}

}  // namespace xoj::mcp::tools
