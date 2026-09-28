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
             "extract_text, explain_figure, drawing, pressure, drafts, draw, pen, permissions.\n"
             "Read tools: page_elements (structure), page_render (see a page), layout_analyze (blocks), "
             "blocks_render (readable crops), pdf_text (PDF background text), shapes_recognize (sketched shapes).\n"
             "Draw: create_* tools, pen_draw, draft, find_free_space; edit: elements_edit/delete/select, undo, "
             "redo, history. Files: file_*, export, import."},
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
            {"drawing",
             "Two ways to draw. DIRECT (create_strokes, create_shapes, create_from_svg, create_text, create_latex, "
             "create_image, create_link): precise, fast, one undo step per call, content goes to the \"AI\" layer by "
             "default (layer=\"current\" or a name to change). PEN (pen_draw): a simulated stylus through the "
             "app's input pipeline - like the user's own pen, visible at hand speed; also erases and selects. "
             "Every creation returns element ids and an operation id; elements_edit/elements_delete accept both. "
             "Use find_free_space to avoid covering the user's notes; new_page=true draws on a fresh page."},
            {"pressure",
             "Pen strokes vary in width like a stylus. Per point: 'pressure' 0..1 (mapped exactly like the user's "
             "stylus: max(min_pressure, p * multiplier) * width) or absolute 'widths'. Or a 'profile': ink (default, "
             "tapered), brush (swells), pencil (light), calligraphy (width depends on direction, nib_angle), "
             "marker/constant (uniform), none. profile_options tune base/min/taper_in/taper_out/variation/seed; "
             "'times' (ms per point) make fast segments thinner; 'tremor' adds a hand-drawn wobble. The "
             "highlighter never has pressure."},
            {"drafts",
             "draft(op=begin) → a hidden draft layer; draw into it with layer=<its name>; draft(op=render) to look at "
             "the result and fix it (elements_edit/elements_delete work on draft content); draft(op=commit) makes it "
             "visible in the target layer as ONE undo step (animate=true to draw it progressively); "
             "draft(op=discard) throws it away."},
            {"draw",
             "Text-to-drawing recipe: 1) Plan: list the parts, their layout (boxes in page points) and a palette. "
             "2) Reserve room: find_free_space(width, height) or new_page=true. 3) draft(op=begin). 4) Draw into "
             "the draft: create_from_svg for illustrations (write an SVG with viewBox, fit it with target=[x,y,w,h]; "
             "profile=\"ink\" and tremor≈0.25 for a hand-drawn look), create_shapes for diagrams, create_text / "
             "create_latex for labels and formulas. 5) draft(op=render) and critique: overlaps, alignment, legibility, "
             "colors; fix with elements_edit or redraw. 6) draft(op=commit, animate=true)."},
            {"pen",
             "pen_draw(tool, strokes, speed): tools pen, highlighter, recognizer (snaps sketches to shapes), "
             "eraser, select_lasso (closed loop around items), select_rect (drag a diagonal), shape_line/rect/"
             "ellipse/arrow/double_arrow/axes (drag from start to end). speed 1 = hand speed (visible), 0 = "
             "instant. The user's tool settings are restored; a selection stays active. It waits while the user "
             "is drawing. It cannot draw into drafts."},
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
            std::string text = "Using the xournalai tools, " + recipe;
            if (args.contains("description") && args["description"].is_string()) {
                text += "\"" + args["description"].get<std::string>() + "\".";
            }
            text += " Work on " + target + " of the open document.";
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
    addPrompt("draw", "Draw from a description", "Plan and draw a picture or diagram from a text description",
              {{"description", "What to draw", true}, {"page", "Page number (default: current page)", false}},
              "draw the following as a clean, well-composed drawing, following the draw recipe (plan, reserve room, "
              "draft, render and critique, commit): " );
}

}  // namespace xoj::mcp::tools
