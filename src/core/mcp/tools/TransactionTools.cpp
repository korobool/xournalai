// Tools: transaction_begin, transaction_commit, transaction_abort, transaction_list

#include "control/Control.h"  // for Control
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "mcp/Transactions.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerTransactionTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec begin;
    begin.name = "transaction_begin";
    begin.title = "Begin an edit transaction";
    begin.description =
            "Starts preparing one change of the canvas while others work in parallel (up to 5 at once). Claims an area "
            "of a page (no two transactions may overlap) and returns a private draft layer: draw the new content "
            "there with the normal tools (layer=<draft_layer>). Nothing is visible until transaction_commit, which "
            "plays everything in order as ONE undo step. List in base_ids the user's strokes your change depends "
            "on (e.g. the formula you will replace): if they change meanwhile, the commit is refused so you never "
            "overwrite newer work. Transactions not committed within 10 minutes are aborted.";
    begin.inputSchema = schema::object(
            {{"page", schema::integer("Page (1-based); default: current page")},
             {"region", schema::array("The area you'll work in [x, y, width, height] (default: the whole page)",
                                      schema::number("coordinate"))},
             {"base_ids", schema::array("Element ids your change depends on", schema::string("element id"))},
             {"label", schema::string("What it does, shown to the user, e.g. \"formula → LaTeX\"")},
             {"zone", schema::integer("The thinking zone of this request (from the wake-up line), if any")}},
            {"label"});
    begin.tier = Tier::Draw;
    begin.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "region", "base_ids", "label", "zone"});
        std::vector<std::string> base;
        if (args.has("base_ids")) {
            for (const auto& v: args.raw("base_ids")) {
                base.push_back(v.get<std::string>());
            }
        }
        const auto& t =
                srv->transactions().begin(resolvePageIndex(ctrl, args), parseRegion(args), base, args.str("label", ""),
                                          static_cast<int>(args.integer("zone", 0, 0, 1 << 30)));
        return ToolResult::structured(
                {{"transaction", t.id},
                 {"draft_layer", t.draftLayer},
                 {"page", t.page + 1},
                 {"next", "Draw into layer=\"" + t.draftLayer + "\", then transaction_commit(transaction=\"" + t.id +
                                  "\", ops=[…])."}});
    };
    server.getRegistry().addTool(std::move(begin));

    ToolSpec commit;
    commit.name = "transaction_commit";
    commit.title = "Commit an edit transaction";
    commit.description =
            "Applies a transaction as ONE undo step: its operations are played in order, each commit after the "
            "previous one (never interleaved with other agents' edits). ops is an ordered list of: "
            "{\"op\":\"draw\", \"animate\":true|false, \"speed\":1} (the draft's content: stylus-like when animate, "
            "else instant), {\"op\":\"delete\",\"ids\":[…]}, {\"op\":\"restyle\",\"ids\":[…],\"color\":\"#rrggbb\","
            "\"width\":1.4,\"fill\":-1..255}, {\"op\":\"move\",\"ids\":[…],\"dx\":0,\"dy\":0}. Example for improving "
            "strokes: [{\"op\":\"draw\",\"animate\":true},{\"op\":\"delete\",\"ids\":[the user's old strokes]}]. "
            "If the draft has content and no draw op is given, it is drawn at the end. When strokes you depend on "
            "changed since transaction_begin, the commit is refused with a reason (conflict): re-read, adjust, commit "
            "again, or transaction_abort. The drawing goes to the layer set in the user's settings.";
    commit.inputSchema =
            schema::object({{"transaction", schema::string("Transaction id (from transaction_begin)")},
                            {"ops", schema::array("Ordered operations (see the description)", schema::object({}))}},
                           {"transaction"});
    commit.tier = Tier::Draw;
    commit.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"transaction", "ops"});
        json ops = args.has("ops") ? args.raw("ops") : json::array();
        srv->transactions().commit(args.str("transaction"), ops, [respond](json r) {
            if (r.contains("error")) {
                std::string msg = r["error"].get<std::string>();
                if (r.contains("hint")) {
                    msg += " " + r["hint"].get<std::string>();
                }
                respond(ToolResult::error(msg));
            } else {
                respond(ToolResult::structured(std::move(r)));
            }
        });
    };
    server.getRegistry().addTool(std::move(commit));

    ToolSpec abort;
    abort.name = "transaction_abort";
    abort.title = "Abort an edit transaction";
    abort.description = "Drops a transaction and its draft without changing the canvas (and frees its area).";
    abort.inputSchema = schema::object(
            {{"transaction", schema::string("Transaction id")}, {"reason", schema::string("Why (shown to the user)")}},
            {"transaction"});
    abort.tier = Tier::Draw;
    abort.handler = [srv](const json& j) {
        Args args(j);
        args.rejectUnknown({"transaction", "reason"});
        srv->transactions().abort(args.str("transaction"), args.str("reason", "aborted by the agent"));
        return ToolResult::structured({{"aborted", args.str("transaction")}});
    };
    server.getRegistry().addTool(std::move(abort));

    ToolSpec list;
    list.name = "transaction_list";
    list.title = "Open edit transactions";
    list.description = "The open transactions: id, page, claimed area, label, age (to avoid claiming the same area).";
    list.inputSchema = schema::object({});
    list.readOnly = true;
    list.idempotent = true;
    list.handler = [srv](const json&) {
        json out = json::array();
        const gint64 now = g_get_monotonic_time();
        for (const auto& t: srv->transactions().list()) {
            json o = {{"transaction", t.id},
                      {"page", t.page + 1},
                      {"label", t.label},
                      {"age_s", (now - t.beganUs) / G_USEC_PER_SEC},
                      {"committing", t.committing}};
            if (t.region) {
                o["region"] = {t.region->x, t.region->y, t.region->width, t.region->height};
            }
            out.push_back(o);
        }
        return ToolResult::structured(
                {{"transactions", out}, {"max", std::clamp(srv->getConfig().assistant.maxParallel, 1, 5)}});
    };
    server.getRegistry().addTool(std::move(list));
}

}  // namespace xoj::mcp::tools
