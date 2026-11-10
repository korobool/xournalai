// Tool: ask_owner (the question panel: an agent asks, the owner answers with a tap)

#include <memory>  // for shared_ptr

#include "mcp/McpProtocol.h"  // for callingWorker
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"  // for McpUi
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerQuestionTools(McpServer& server) {
    McpServer* srv = &server;
    ToolSpec t;
    t.name = "ask_owner";
    t.title = "Ask the owner (a tap answers)";
    t.description =
            "Ask the owner a short question he answers with a tap, instead of asking in the terminal: the question "
            "panel shows it left of the AI terminal (or at the bottom left of the canvas when the terminal is "
            "closed), with your options as big buttons (only yours: offer \"No\" / \"Not now\" yourself when he may decline; all of it in the app's interface language, English, like the rest of its interface). Use it for a confirmation before "
            "something he may not want (changing his own strokes, deleting, a big restructure), or a real choice "
            "(\"a mind map or a timeline?\"). op=ask: question (one line, in his language), options (up to 6 short "
            "labels, e.g. [\"Yes, draw it\", \"No\"]; none: a yes/no question with the buttons Yes / No, answered "
            "\"yes\" or \"no\"), default "
            "(the answer if he doesn't answer in time: make it the safe one; default \"no\"), timeout_s (10-600, "
            "default 90), wait (default true: the call returns his answer; false: it returns the id at once, and "
            "you are woken with the answer, or a worker looks with op=get). The answer is the label he tapped (yes / "
            "no for a yes/no question); by: owner | timeout | cancelled. A spoken \"yes\" is never an answer: only a tap is. "
            "op=get (id), op=cancel (id: no longer needed), op=list. Ask rarely, one thing at a time.";
    t.inputSchema = schema::object(
            {{"op", schema::enumeration("Operation", {"ask", "get", "cancel", "list"})},
             {"question", schema::string("ask: the question, short")},
             {"options", schema::array("ask: the answers to offer as buttons (none: yes/no)", schema::string("label"))},
             {"default", schema::string("ask: the answer when nobody answers in time (safe; default \"no\")")},
             {"timeout_s", schema::integer("ask: seconds to wait (10-600, default 90)")},
             {"wait", schema::boolean("ask: wait for the answer (default true)")},
             {"id", schema::integer("get/cancel: the question")}},
            {"op"});
    t.tier = Tier::Ui;
    t.asyncHandler = [srv](const json& j, Responder respond) {
        Args args(j);
        args.rejectUnknown({"op", "question", "options", "default", "timeout_s", "wait", "id"});
        const std::string op = args.choice("op", {"ask", "get", "cancel", "list"}, "");
        McpUi* ui = srv->getUi();
        if (!ui) {
            throw ToolError("no window to ask in");
        }
        if (op == "list") {
            respond(ToolResult::structured(ui->questions()));
            return;
        }
        if (op == "get" || op == "cancel") {
            const int id = static_cast<int>(args.integer("id", 0, 1, 1 << 30));
            if (op == "cancel") {
                ui->cancelQuestion(id);
            }
            if (auto a = ui->questionAnswer(id)) {
                json out = *a;
                out["id"] = id;
                out["state"] = "answered";
                respond(ToolResult::structured(std::move(out)));
            } else {
                respond(ToolResult::structured({{"id", id}, {"state", "waiting"}}));
            }
            return;
        }
        const std::string question = args.str("question", "");
        if (question.empty() || question.size() > 600) {
            throw ToolError("question: one short line (at most 600 characters)");
        }
        std::vector<std::pair<std::string, std::string>> options;
        if (args.has("options")) {
            const json& list = args.raw("options");
            if (!list.is_array() || list.size() > 6) {
                throw ToolError("options: up to 6 short labels");
            }
            for (const auto& o: list) {
                if (!o.is_string() || o.get<std::string>().empty() || o.get<std::string>().size() > 120) {
                    throw ToolError("options: each a short label");
                }
                options.emplace_back(o.get<std::string>(), o.get<std::string>());
            }
        }
        const std::string fallback = args.str("default", "no");
        const int timeout = static_cast<int>(args.integer("timeout_s", 90, 10, 600));
        const bool wait = args.boolean("wait", true);
        const std::string& worker = callingWorker();
        const std::string who = worker.empty() ? "Claude" : "⚙ " + worker;
        auto idHolder = std::make_shared<int>(0);
        std::function<void(const std::string&, const std::string&)> done;
        if (wait) {
            done = [respond, idHolder](const std::string& value, const std::string& by) {
                respond(ToolResult::structured({{"id", *idHolder}, {"answer", value}, {"by", by}}));
            };
        } else if (worker.empty()) {
            // (the serving session hears the answer as a wake-up; a worker, which gets none, looks with op=get)
            done = [ui, question, idHolder](const std::string& value, const std::string& by) {
                ui->wakeServing("The owner's answer to your question " + std::to_string(*idHolder) + " «" +
                                question + "»: «" + value + "» (" + by + ").");
            };
        }
        const int id = ui->askOwner(who, question, options, fallback, timeout, std::move(done));
        if (!id) {
            throw ToolError("no question panel in this window");
        }
        *idHolder = id;
        if (!wait) {
            respond(ToolResult::structured({{"id", id}, {"state", "waiting"}}));
        }
    };
    server.getRegistry().addTool(std::move(t));
}

}  // namespace xoj::mcp::tools
