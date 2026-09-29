// Tool: notes

#include <shared_mutex>  // for shared_lock

#include "control/Control.h"  // for Control
#include "mcp/McpServer.h"
#include "mcp/NotesStore.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {
std::string nowIso() {
    GDateTime* t = g_date_time_new_now_local();
    gchar* s = g_date_time_format_iso8601(t);
    std::string out = s;
    g_free(s);
    g_date_time_unref(t);
    return out;
}

bool overlaps(const xoj::util::Rectangle<double>& a, const xoj::util::Rectangle<double>& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}
}  // namespace

void registerNoteTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    ToolSpec notes;
    notes.name = "notes";
    notes.title = "Notes memory";
    notes.description =
            "Remembers what you worked out about the document so you (or another agent) need not re-read it: "
            "transcripts of handwriting, summaries, the meaning of figures. op=set stores text for the document, a "
            "page, or a region of a page (one note per place and kind; set again to replace, empty text deletes); "
            "op=get returns the notes, filtered by page, kind and region (overlapping). Notes are saved next to "
            "the document file (<file>.ai-notes.json) and follow its pages when they are moved.";
    notes.inputSchema = schema::object(
            {{"op", schema::enumeration("set | get", {"set", "get"})},
             {"page", schema::integer("Page (1-based); omit for notes about the whole document (set) / all (get)")},
             {"region", schema::array("Area on the page [x, y, width, height]", schema::number("coordinate"))},
             {"kind", schema::enumeration("What the note is", {"transcript", "summary", "meaning", "note"})},
             {"text", schema::string("set: the note text (empty deletes it)")}},
            {"op"});
    notes.tier = Tier::Read;
    notes.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "page", "region", "kind", "text"});
        NotesStore* store = srv->getNotes();
        if (!store) {
            throw ToolError("Notes are not available");
        }
        store->sync();
        const std::string op = args.choice("op", {"set", "get"}, "");
        const auto region = parseRegion(args);
        PageRef page;
        if (args.has("page")) {
            const size_t index = resolvePageIndex(ctrl, args);
            Document* doc = ctrl->getDocument();
            std::shared_lock lock(*doc);
            page = doc->getPage(index);
        } else if (region && op == "set") {
            throw ToolError("A region needs a 'page'");
        }
        if (op == "set") {
            srv->requireTier(Tier::Draw, "storing notes");
            Note note;
            note.page = page;
            note.region = region;
            note.kind = args.choice("kind", {"transcript", "summary", "meaning", "note"}, "note");
            note.text = args.str("text", "");
            note.updated = nowIso();
            const bool changed = store->set(std::move(note));
            return ToolResult::structured(
                    {{"changed", changed},
                     {"deleted", args.str("text", "").empty()},
                     {"file", store->file().empty() ? json(nullptr) : json(toUtf8(store->file()))},
                     {"saved", !store->file().empty()}});
        }
        const std::string kind = args.str("kind", "");
        json list = json::array();
        Document* doc = ctrl->getDocument();
        for (const auto& n: store->all()) {
            if ((args.has("page") && n.page != page) || (!kind.empty() && n.kind != kind)) {
                continue;
            }
            if (region && n.region && !overlaps(*region, *n.region)) {
                continue;
            }
            json o = {{"kind", n.kind}, {"text", n.text}, {"updated", n.updated}};
            if (n.page) {
                std::shared_lock lock(*doc);
                o["page"] = doc->indexOf(n.page) + 1;
            }
            if (n.region) {
                o["region"] = {n.region->x, n.region->y, n.region->width, n.region->height};
            }
            list.push_back(std::move(o));
        }
        return ToolResult::structured({{"count", list.size()}, {"notes", std::move(list)}});
    };
    server.getRegistry().addTool(std::move(notes));
}

}  // namespace xoj::mcp::tools
