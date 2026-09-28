// Tools: page_elements, pdf_text

#include <algorithm>     // for min
#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"  // for elementsOnPage
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/Document.h"       // for Document
#include "model/XojPage.h"        // for XojPage
#include "pdf/base/XojPdfPage.h"  // for XojPdfPage

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::optional<xoj::util::Rectangle<double>> regionArg(const Args& args, const std::string& key = "region") {
    auto r = args.numbersOpt(key);
    if (!r) {
        return std::nullopt;
    }
    if (r->size() != 4 || (*r)[2] < 0 || (*r)[3] < 0) {
        throw ToolError("'" + key + "' must be [x, y, width, height] in page points");
    }
    return xoj::util::Rectangle<double>((*r)[0], (*r)[1], (*r)[2], (*r)[3]);
}

json regionSchema(const std::string& what) {
    return schema::array(what + " as [x, y, width, height] in page points", schema::number("coordinate"));
}

}  // namespace

std::optional<xoj::util::Rectangle<double>> parseRegion(const Args& args, const std::string& key) {
    return regionArg(args, key);
}

void registerReadTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec elements;
    elements.name = "page_elements";
    elements.title = "Page elements";
    elements.description =
            "Lists the content of a page, bottom to top: strokes (handwriting and drawings), typed text, LaTeX, "
            "images and links. Each element has a stable id (e.g. \"e42\") usable by other tools, its layer, "
            "bbox [x,y,w,h] and color. detail=bbox is a cheap overview (text content included); simplified adds "
            "style and stroke points simplified to 'tolerance' points; full returns every point. Stroke points "
            "are [x,y] or [x,y,w] where w is the pressure-dependent width. Use region to focus on an area and "
            "offset/limit to page through busy pages. To *see* handwriting, use page_render instead.";
    elements.inputSchema = schema::object(
            {{"page", schema::integer("Page number (1-based); default: current page")},
             {"layer", schema::integer("Only this layer (1-based)")},
             {"region", regionSchema("Only elements intersecting this area")},
             {"detail", schema::withDefault(schema::enumeration("Level of detail", {"bbox", "simplified", "full"}),
                                            "simplified")},
             {"tolerance",
              schema::withDefault(schema::number("Simplification tolerance in points (detail=simplified)"), 0.5)},
             {"types",
              schema::array("Only these element types",
                            schema::enumeration("Element type", {"stroke", "text", "latex", "image", "link"}))},
             {"offset", schema::withDefault(schema::integer("Skip this many matching elements"), 0)},
             {"limit", schema::withDefault(schema::integer("Maximum number of elements to return"), 200)}});
    elements.readOnly = true;
    elements.idempotent = true;
    elements.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "layer", "region", "detail", "tolerance", "types", "offset", "limit"});
        Document* doc = ctrl->getDocument();
        std::shared_lock lock(*doc);
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        PageRef page = doc->getPage(pageIndex);
        std::optional<size_t> layer;
        if (args.has("layer")) {
            layer = static_cast<size_t>(
                    args.integer("layer", 1, 1, static_cast<int64_t>(std::max<size_t>(page->getLayerCount(), 1))));
        }
        const Detail detail = detailFromName(args.str("detail", "simplified"));
        const double tolerance = args.number("tolerance", 0.5, 0, 100);
        const auto offset = static_cast<size_t>(args.integer("offset", 0, 0, 1000000));
        const auto limit = static_cast<size_t>(args.integer("limit", 200, 1, 5000));
        std::vector<std::string> types;
        if (args.has("types")) {
            for (const auto& t: args.raw("types")) {
                types.push_back(t.get<std::string>());
            }
        }

        json list = json::array();
        size_t matching = 0;
        for (const auto& loc: api::elementsOnPage(page, pageIndex, layer, regionArg(args))) {
            if (!types.empty() && std::find(types.begin(), types.end(), elementTypeName(loc.element)) == types.end()) {
                continue;
            }
            if (matching >= offset && list.size() < limit) {
                list.push_back(elementToJson(loc, detail, tolerance));
            }
            matching++;
        }
        json out = {{"page", pageIndex + 1},   {"page_size", {page->getWidth(), page->getHeight()}},
                    {"total", matching},       {"offset", offset},
                    {"returned", list.size()}, {"elements", std::move(list)}};
        if (offset + out["returned"].get<size_t>() < matching) {
            out["next_offset"] = offset + out["returned"].get<size_t>();
        }
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(elements));

    ToolSpec pdf;
    pdf.name = "pdf_text";
    pdf.title = "PDF background text";
    pdf.description =
            "Returns the text of the PDF page used as the background of a page (for annotated PDFs). With "
            "positions=true the text is split into lines, each with its bbox in page points. Use region to read "
            "only part of the page. Handwritten notes are not included: use page_render/page_elements for those.";
    pdf.inputSchema = schema::object(
            {{"page", schema::integer("Page number (1-based); default: current page")},
             {"positions", schema::withDefault(schema::boolean("Return lines with their bounding boxes"), false)},
             {"region", regionSchema("Only text inside this area")}});
    pdf.readOnly = true;
    pdf.idempotent = true;
    pdf.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "positions", "region"});
        Document* doc = ctrl->getDocument();
        std::shared_lock lock(*doc);
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        PageRef page = doc->getPage(pageIndex);
        if (!page->getBackgroundType().isPdfPage()) {
            throw ToolError("Page " + std::to_string(pageIndex + 1) + " has no PDF background");
        }
        auto pdfPage = doc->getPdfPage(page->getPdfPageNr());
        if (!pdfPage) {
            throw ToolError("The background PDF of page " + std::to_string(pageIndex + 1) + " is not available");
        }
        const auto region = regionArg(args);
        const XojPdfRectangle area =
                region ? XojPdfRectangle(region->x, region->y, region->x + region->width, region->y + region->height) :
                         XojPdfRectangle(0, 0, pdfPage->getWidth(), pdfPage->getHeight());
        json out = {{"page", pageIndex + 1}, {"pdf_page", page->getPdfPageNr() + 1}};
        const auto style = region ? XojPdfPageSelectionStyle::Area : XojPdfPageSelectionStyle::Linear;
        out["text"] = pdfPage->selectText(area, style);
        if (args.boolean("positions", false)) {
            json lines = json::array();
            auto selection = pdfPage->selectTextLines(area, style);
            for (const auto& r: selection.rects) {
                if (r.x2 - r.x1 < 0.01 || r.y2 - r.y1 < 0.01) {
                    continue;  // empty line break markers
                }
                // Pad slightly so the line's glyphs are fully inside the rectangle
                const std::string text =
                        pdfPage->selectText(XojPdfRectangle(r.x1 - 0.5, r.y1 + 0.5, r.x2 + 0.5, r.y2 - 0.5),
                                            XojPdfPageSelectionStyle::Area);
                lines.push_back({{"text", text}, {"bbox", bboxJson(r.x1, r.y1, r.x2 - r.x1, r.y2 - r.y1)}});
            }
            out["lines"] = std::move(lines);
        }
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(pdf));
}

}  // namespace xoj::mcp::tools
