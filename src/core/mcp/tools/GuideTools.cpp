// Tool: guide. Prompts: summarize, extract_text, explain_figure

#include <map>     // for map
#include <string>  // for string

#include "mcp/McpServer.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

const std::map<std::string, std::string>& topics() {
    static const std::map<std::string, std::string> t = {
            {"overview",
             "xournalai = Xournal++ with an embedded MCP server. You are connected to the user's running app; "
             "what you change appears on their screen immediately.\n"
             "Start with app_status and doc_info. Topics: coordinates, ids, understanding, summarize, "
             "extract_text, explain_figure, permissions.\n"
             "Read tools: page_elements (structure), page_render (see a page), layout_analyze (blocks), "
             "blocks_render (readable crops), pdf_text (PDF background text), shapes_recognize (sketched shapes)."},
            {"coordinates",
             "Units are page points (1/72 inch). Origin = top-left corner of each page, x to the right, y down. "
             "An A4 page is about 595 x 842 points. Pages and layers are numbered from 1 (layer 1 is the bottom "
             "layer). Regions are [x, y, width, height]. Renders report region and px_per_pt: "
             "page_x = region[0] + pixel_x / px_per_pt. Use page_render(grid=true) to see coordinates."},
            {"ids",
             "Every element has a session id like \"e42\" (from page_elements, layout_analyze with "
             "include_element_ids, or creation tools). Ids stay valid across moves, restyling and undo/redo until "
             "the element is discarded. Block ids \"b1\", \"b2\" from layout_analyze are only valid until the page "
             "changes: re-run layout_analyze after edits."},
            {"understanding",
             "To understand a page: 1) layout_analyze(page) for a map of blocks in reading order (typed text and "
             "LaTeX are included verbatim; handwriting and figures are not read). 2) blocks_render(page, "
             "block_ids, split_lines=true for handwriting) to read handwriting line by line at high resolution. "
             "3) For figures, blocks_render the figure plus its labels (blocks whose parent is the figure) and "
             "check connectors (arrows between blocks) to understand diagrams. 4) page_render(page) for the "
             "overall look. 5) pdf_text if the page annotates a PDF. Report uncertain readings as uncertain."},
            {"summarize",
             "Summarize a page: layout_analyze -> blocks_render all handwriting blocks (split_lines=true when "
             "dense) -> transcribe -> read typed text/LaTeX from layout_analyze -> interpret figures (see "
             "explain_figure) -> write a structured summary: main topic, key points in reading order, "
             "figures and what they show, open questions/todos found in the notes."},
            {"extract_text",
             "Extract text: layout_analyze(page) -> for each handwriting block in reading order, "
             "blocks_render(block_ids=[id], split_lines=true) and transcribe each line exactly (keep line breaks, "
             "mark unreadable words as [?]) -> merge with typed_text/latex blocks in reading order -> if the page "
             "has a PDF background add pdf_text. Output plain text or Markdown; formulas as LaTeX."},
            {"explain_figure",
             "Explain a figure: layout_analyze(page) -> pick the figure block (kind=figure) -> blocks_render the "
             "figure and every block whose parent is it (labels) -> list the connectors whose from_block/to_block "
             "touch it -> shapes_recognize on its strokes if shapes matter -> describe: what kind of figure "
             "(diagram, flowchart, schematic, graph, sketch), its parts and labels, relations/arrows, and the "
             "idea it expresses. For flowcharts give the steps in order."},
            {"permissions",
             "Tools belong to tiers: read, draw, ui, files, destructive. The user grants tiers in "
             "~/.config/xournalpp/mcp.json; app_status shows them. A denied call returns an error explaining "
             "how the user can grant it; ask the user instead of retrying."}};
    return t;
}

std::string topicList() {
    std::string list;
    for (const auto& [name, text]: topics()) {
        list += list.empty() ? name : ", " + name;
    }
    return list;
}

}  // namespace

void registerGuideTools(McpServer& server) {
    std::vector<std::string> names;
    for (const auto& [name, text]: topics()) {
        names.push_back(name);
    }
    ToolSpec guide;
    guide.name = "guide";
    guide.title = "Usage guide";
    guide.description = "Explains how to use xournalai: conventions and step-by-step recipes. Topics: " + topicList() +
                        ". Call with no topic for the overview.";
    guide.inputSchema =
            schema::object({{"topic", schema::withDefault(schema::enumeration("Guide topic", names), "overview")}});
    guide.readOnly = true;
    guide.idempotent = true;
    guide.handler = [](const json& j) {
        Args args(j);
        args.rejectUnknown({"topic"});
        const std::string topic = args.str("topic", "overview");
        auto it = topics().find(topic);
        if (it == topics().end()) {
            throw ToolError("Unknown topic '" + topic + "'. Topics: " + topicList());
        }
        return ToolResult::text(it->second);
    };
    server.getRegistry().addTool(std::move(guide));

    auto addPrompt = [&server](const std::string& name, const std::string& title, const std::string& description,
                               std::vector<PromptArgument> arguments, const std::string& recipe) {
        PromptSpec p;
        p.name = name;
        p.title = title;
        p.description = description;
        p.arguments = std::move(arguments);
        p.render = [name, recipe](const json& args) {
            std::string target = "the current page";
            if (args.contains("page") && args["page"].is_string() && !args["page"].get<std::string>().empty()) {
                target = "page " + args["page"].get<std::string>();
            }
            std::string text = "Using the xournalai tools, " + recipe + " Work on " + target + " of the open document.";
            if (args.contains("focus") && args["focus"].is_string() && !args["focus"].get<std::string>().empty()) {
                text += " Focus on: " + args["focus"].get<std::string>() + ".";
            }
            return text + " (Recipe details: guide(topic=\"" + name + "\").)";
        };
        server.getRegistry().addPrompt(std::move(p));
    };
    addPrompt("summarize", "Summarize notes", "Summarize what is written and drawn on a page",
              {{"page", "Page number (default: current page)", false}},
              "summarize my handwritten notes: read handwriting, typed text and figures, then give a structured "
              "summary with key points, figures and open questions.");
    addPrompt("extract_text", "Extract text", "Transcribe all handwriting and text of a page",
              {{"page", "Page number (default: current page)", false}},
              "transcribe all text on the page (handwriting line by line, typed text, LaTeX) in reading order.");
    addPrompt("explain_figure", "Explain a figure", "Explain a drawing, diagram or schematic",
              {{"page", "Page number (default: current page)", false},
               {"focus", "Which figure (e.g. 'the flowchart at the top')", false}},
              "find the figure(s) on the page and explain what they show: parts, labels, relations and the idea.");
}

}  // namespace xoj::mcp::tools
