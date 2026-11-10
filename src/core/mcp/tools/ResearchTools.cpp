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
            "The app's research browser: a Chrome of its own (its own profile, \"xournalai research\") where searches "
            "open as tabs the owner can see and open. The browser does the searching; you read only what you need. To "
            "click, type, navigate a site or switch tabs, use the Claude in Chrome tools (mcp__claude-in-chrome__*) on "
            "exactly this browser: op=list gives its extension's name and device_id (select_browser with exactly that "
            "id; several Chromes of his may be connected, all named alike); work in the extension's tab group. Never "
            "another browser unless the owner names it. This tool is the cheap "
            "way to search and read. op=open: query + kind (web | images | wiki | scholar), or url; topic (what it is for, e.g. \"T3 "
            "notations\") → its tab. op=list: the tabs (tab, url, title, topic). op=read: tab, max_chars (default "
            "6000) → title, url, the page's text, its pictures (n, src, alt, width, height) and the owner's selection. "
            "op=image: tab, n (a picture's number from read) → it is saved to a file: path, width, height, source, "
            "title; place it with create_image(path, …) and a small source line under it. "
            "Target pages and intermediate ones (the owner, 10 Nov): a target page is what he asked for or what was "
            "looked for (the article, the paper, the entity's page); search results, steps along a site and pages that "
            "turned out off topic are intermediate. op=keep: tab, or url (a page reached through the extension), "
            "topic, note (what it is) → a ★ in its title; it stays open until he closes it. op=close: tab, or "
            "all=true → closes intermediate pages; a kept page or one he looked at is refused (all=true leaves them), "
            "unless he asked for it (owner_asked=true). Close intermediate pages as soon as you are past them. "
            "op=sources: the kept pages by topic (for the «🔎 Sources» card at the end). "
            "This tool itself only opens, reads and closes tabs and saves pictures. Never log in, buy, pay or type "
            "passwords there; posting, sending, uploading or downloading anything else needs the owner's tap first "
            "(ask_owner). Close the tabs you no longer need; keep a few at a time. op=signin: opens the browser for "
            "the owner to sign in (Google, claude.ai, the extension): Google refuses to sign in while the app is "
            "connected to it, so it is restarted without that connection; reading waits until he closes that window, "
            "and then it is back, connected. Use it when he asks to sign in there, or when a sign-in is refused (\"This "
            "browser or app may not be secure\").";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("Operation",
                                        {"open", "list", "read", "image", "close", "signin", "keep", "sources"})},
             {"query", schema::string("open: what to search for")},
             {"kind", schema::enumeration("open: where to search", {"web", "images", "wiki", "scholar"})},
             {"url", schema::string("open: a page to open instead of a search")},
             {"topic", schema::string("open: what the tab is for (shown in list)")},
             {"tab", schema::string("read/image/close: the tab")},
             {"max_chars", schema::integer("read: at most this much text (default 6000)")},
             {"n", schema::integer("image: the picture's number (from read)")},
             {"all", schema::boolean("close: every intermediate tab")},
             {"note", schema::string("keep: what the page is (\"the SMILES article\")")},
             {"owner_asked", schema::boolean("close: the owner asked to close it (also a kept page or one he looked at)")}},
            {"op"});
    t.tier = Tier::Ui;
    t.asyncHandler = [srv](const json& j, Responder respond) {
        Args args(j);
        args.rejectUnknown(
                {"op", "query", "kind", "url", "topic", "tab", "max_chars", "n", "all", "note", "owner_asked"});
        const std::string op =
                args.choice("op", {"open", "list", "read", "image", "close", "signin", "keep", "sources"}, "");
        McpUi* ui = srv->getUi();
        assistant::ResearchBrowser* b = ui ? ui->research() : nullptr;
        if (!b) {
            throw ToolError("no research browser in this window");
        }
        auto reply = [respond](const json& r, const std::string& error) {
            respond(error.empty() ? ToolResult::structured(r.is_null() ? json::object() : r) : ToolResult::error(error));
        };
        if (op == "signin") {
            ui->researchSignIn();
            respond(ToolResult::structured({{"signing_in", true},
                                            {"note", "the research browser opens without the app's connection, on "
                                                     "this workspace, with a page that says what to do; when the "
                                                     "owner closes it, it is back, connected"}}));
            return;
        }
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
        if (b->signingIn()) {
            throw ToolError(assistant::ResearchBrowser::SIGN_IN_OPEN);
        }
        if (!b->running()) {
            // (it may still run from before: found again)
            b->ensure([](const std::string&) {});
        }
        if (op == "list") {
            b->list([b, reply](const json& r, const std::string& error) {
                if (!error.empty()) {
                    reply(nullptr, error);
                    return;
                }
                b->identity([r, reply](const json& id, const std::string&) {
                    json out = r.is_object() ? r : json::object();
                    out["browser"] = assistant::ResearchBrowser::PROFILE_NAME;
                    out["extension"] = id;
                    out["extension"]["use"] = "Claude in Chrome: select_browser with exactly this device_id; never "
                                              "another browser unless the owner names it";
                    reply(out, "");
                });
            });
        } else if (op == "read") {
            b->read(args.str("tab"), static_cast<size_t>(args.integer("max_chars", 6000, 200, 60000)), reply);
        } else if (op == "image") {
            const fs::path folder = fs::path(g_get_user_data_dir()) / "xournalai" / "research" / "images";
            b->saveImage(args.str("tab"), static_cast<int>(args.integer("n", 0, 0, 1000)), folder, reply);
        } else if (op == "keep") {
            if (!args.has("tab") && !args.has("url")) {
                throw ToolError("keep: tab or url");
            }
            b->keep(args.str("tab", ""), args.str("url", ""), args.str("topic", ""), args.str("note", ""), reply);
        } else if (op == "sources") {
            b->sources(reply);
        } else if (args.boolean("all", false)) {
            b->closeAll(args.boolean("owner_asked", false), reply);
        } else {
            b->close(args.str("tab"), args.boolean("owner_asked", false),
                     [reply](const json&, const std::string& error) { reply({{"closed", true}}, error); });
        }
    };
    server.getRegistry().addTool(std::move(t));
}

}  // namespace xoj::mcp::tools
