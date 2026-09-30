// Tools: layout_analyze, blocks_render, shapes_recognize

#include <cmath>         // for hypot
#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"                          // for elementsOnPage
#include "api/ElementIds.h"                           // for ElementIds
#include "api/Layout.h"                               // for analyzeLayout
#include "api/RenderApi.h"                            // for renderPage
#include "control/Control.h"                          // for Control
#include "control/settings/Settings.h"                // for Settings
#include "control/shaperecognizer/ShapeRecognizer.h"  // for ShapeRecognizer
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/OffUi.h"  // for runOffUi
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document
#include "model/Stroke.h"    // for Stroke
#include "model/TexImage.h"  // for TexImage
#include "model/Text.h"      // for Text
#include "model/XojPage.h"   // for XojPage

#include "LayoutCommon.h"
#include "RenderCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

std::vector<api::LayoutItem> layoutItems(const std::vector<api::ElementLocation>& elements) {
    std::vector<api::LayoutItem> items;
    items.reserve(elements.size());
    for (size_t i = 0; i < elements.size(); i++) {
        const Element* e = elements[i].element;
        api::LayoutItem it;
        it.index = i;
        it.bbox = e->getBoundingBox();
        switch (e->getType()) {
            case ELEMENT_STROKE: {
                const auto* s = static_cast<const Stroke*>(e);
                it.kind = s->getToolType() == StrokeTool::HIGHLIGHTER ? api::LayoutItem::Kind::Highlighter :
                                                                        api::LayoutItem::Kind::PenStroke;
                const auto& pts = s->getPointVector();
                if (!pts.empty()) {
                    it.first = {pts.front().x, pts.front().y};
                    it.last = {pts.back().x, pts.back().y};
                }
                for (size_t k = 1; k < pts.size(); k++) {
                    it.length += std::hypot(pts[k].x - pts[k - 1].x, pts[k].y - pts[k - 1].y);
                }
                break;
            }
            case ELEMENT_TEXT:
                it.kind = api::LayoutItem::Kind::Text;
                break;
            case ELEMENT_TEXIMAGE:
                it.kind = api::LayoutItem::Kind::Latex;
                break;
            case ELEMENT_IMAGE:
                it.kind = api::LayoutItem::Kind::Image;
                break;
            case ELEMENT_LINK:
                it.kind = api::LayoutItem::Kind::Link;
                break;
        }
        items.push_back(it);
    }
    return items;
}

PageLayout analyzePage(Control* ctrl, size_t pageIndex, std::optional<size_t> layer,
                       std::optional<xoj::util::Rectangle<double>> region) {
    PageLayout out;
    PageRef page = ctrl->getDocument()->getPage(pageIndex);
    out.elements = api::elementsOnPage(page, pageIndex, layer, region, true);
    out.layout = api::analyzeLayout(layoutItems(out.elements));
    return out;
}

std::string blockId(size_t index) { return "b" + std::to_string(index + 1); }

size_t parseBlockId(const std::string& id, size_t count) {
    size_t n = 0;
    try {
        n = std::stoul(id.substr(!id.empty() && id[0] == 'b' ? 1 : 0));
    } catch (const std::exception&) {
        throw ToolError("'" + id + "' is not a block id (expected \"b1\", \"b2\", ... from layout_analyze)");
    }
    if (n < 1 || n > count) {
        throw ToolError("Block " + id + " does not exist; layout_analyze found " + std::to_string(count) +
                        " block(s). Run it again if the page changed.");
    }
    return n - 1;
}

void registerLayoutTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec layout;
    layout.name = "layout_analyze";
    layout.title = "Analyze page layout";
    layout.description =
            "Groups the content of a page into blocks in reading order: handwriting (with text line boxes), figure "
            "(drawings, diagrams, schematics), highlight, typed_text, latex, image, link; plus connectors (long "
            "straight strokes such as arrows linking blocks, with the blocks at each end). Blocks inside a figure "
            "(labels) name it as parent. Typed text and LaTeX sources are included verbatim. This is a heuristic "
            "map to decide what to look at: then call blocks_render to see the blocks and read the handwriting.";
    layout.inputSchema =
            schema::object({{"page", schema::integer("Page number (1-based); default: current page")},
                            {"layer", schema::integer("Only this layer (1-based)")},
                            {"region", schema::array("Only this area [x, y, width, height] in page points",
                                                     schema::number("coordinate"))},
                            {"include_element_ids",
                             schema::withDefault(schema::boolean("List the element ids of every block"), false)}});
    layout.readOnly = true;
    layout.idempotent = true;
    // Off the UI thread: the document lock is held only to collect the elements' geometry (and ids, texts); the
    // analysis runs without it
    layout.asyncHandler = [ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "layer", "region", "include_element_ids"});
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        std::optional<size_t> layer;
        if (args.has("layer")) {
            layer = static_cast<size_t>(args.integer("layer", 1, 1, 1000));
        }
        const auto region = parseRegion(args);
        const bool withIds = args.boolean("include_element_ids", false);
        Document* doc = ctrl->getDocument();

        runOffUi(
                ctrl,
                [=]() {
                    std::vector<api::LayoutItem> items;
                    std::vector<std::string> ids;    // per element
                    std::vector<std::string> texts;  // per element: typed text or LaTeX source
                    {
                        std::shared_lock lock(*doc);
                        if (pageIndex >= doc->getPageCount()) {
                            throw ToolError("Page " + std::to_string(pageIndex + 1) + " no longer exists");
                        }
                        const auto elements =
                                api::elementsOnPage(doc->getPage(pageIndex), pageIndex, layer, region, true);
                        items = layoutItems(elements);
                        for (const auto& loc: elements) {
                            ids.push_back(api::ElementIds::get().idOf(loc.element));
                            const Element* e = loc.element;
                            texts.push_back(e->getType() == ELEMENT_TEXT ? static_cast<const Text*>(e)->getText() :
                                            e->getType() == ELEMENT_TEXIMAGE ?
                                                                           static_cast<const TexImage*>(e)->getText() :
                                                                           std::string());
                        }
                    }
                    const api::Layout pl = api::analyzeLayout(items);

                    json blocks = json::array();
                    for (size_t i = 0; i < pl.blocks.size(); i++) {
                        const auto& b = pl.blocks[i];
                        json jb = {{"id", blockId(i)},
                                   {"kind", b.kind},
                                   {"bbox", bboxJson(b.bbox.x, b.bbox.y, b.bbox.width, b.bbox.height)},
                                   {"element_count", b.items.size()}};
                        if (!b.lines.empty() && b.lines.size() > 1) {
                            json lines = json::array();
                            for (const auto& l: b.lines) {
                                lines.push_back(bboxJson(l.x, l.y, l.width, l.height));
                            }
                            jb["lines"] = std::move(lines);
                        }
                        if (b.parent) {
                            jb["parent"] = blockId(*b.parent);
                        }
                        if (b.kind == "typed_text") {
                            jb["text"] = texts[b.items.front()];
                        } else if (b.kind == "latex") {
                            jb["latex"] = texts[b.items.front()];
                        }
                        if (withIds) {
                            json list = json::array();
                            for (size_t k: b.items) {
                                list.push_back(ids[k]);
                            }
                            jb["element_ids"] = std::move(list);
                        }
                        blocks.push_back(std::move(jb));
                    }
                    json connectors = json::array();
                    for (const auto& c: pl.connectors) {
                        json jc = {{"element_id", ids[c.item]},
                                   {"from", {api::roundTo(c.from.x), api::roundTo(c.from.y)}},
                                   {"to", {api::roundTo(c.to.x), api::roundTo(c.to.y)}}};
                        jc["from_block"] = c.fromBlock ? json(blockId(*c.fromBlock)) : json(nullptr);
                        jc["to_block"] = c.toBlock ? json(blockId(*c.toBlock)) : json(nullptr);
                        connectors.push_back(std::move(jc));
                    }
                    json out = {{"page", pageIndex + 1},
                                {"typical_stroke_height", api::roundTo(pl.typicalHeight)},
                                {"block_count", blocks.size()},
                                {"blocks", std::move(blocks)},
                                {"connectors", std::move(connectors)},
                                {"next", "Call blocks_render(page, block_ids) to see blocks at readable resolution."}};
                    return ToolResult::structured(std::move(out));
                },
                std::move(respond));
    };
    server.getRegistry().addTool(std::move(layout));

    McpServer* srv = &server;
    std::vector<schema::Property> props = {
            {"page", schema::integer("Page number (1-based); default: current page")},
            {"block_ids",
             schema::array("Blocks from layout_analyze, e.g. [\"b1\", \"b3\"]", schema::string("block id"))},
            {"element_ids",
             schema::array("Render the area covering these elements instead", schema::string("element id"))},
            {"split_lines",
             schema::withDefault(schema::boolean("Render each text line of handwriting blocks as its own "
                                                 "image (best for reading handwriting)"),
                                 false)},
            {"padding", schema::withDefault(schema::number("Margin around each crop in points"), 6)},
            {"max_images", schema::withDefault(schema::integer("Maximum number of images returned"), 12)}};
    addRenderSchemaProperties(props);
    for (auto& [name, s]: props) {
        if (name == "dpi") {
            s["default"] = 200;
        }
    }

    ToolSpec blocks;
    blocks.name = "blocks_render";
    blocks.title = "Render content blocks";
    blocks.description =
            "Renders tight, high-resolution crops of layout blocks (from layout_analyze) or of the area covering "
            "given elements, one image per block, each captioned with its block id, kind and bbox. Use "
            "split_lines=true to get one image per handwritten text line for accurate transcription.";
    blocks.inputSchema = schema::object(std::move(props));
    blocks.readOnly = true;
    blocks.idempotent = true;
    // Off the UI thread: finding the blocks and rendering crops at a readable resolution takes long on big pages
    blocks.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "block_ids", "element_ids", "split_lines", "padding", "max_images", "dpi", "max_px",
                            "background", "grid", "grid_step", "save"});
        if (!args.has("block_ids") && !args.has("element_ids")) {
            throw ToolError("Give block_ids (from layout_analyze) or element_ids");
        }
        const double pad = args.number("padding", 6, 0, 200);
        const auto maxImages = static_cast<size_t>(args.integer("max_images", 12, 1, 50));
        const bool splitLines = args.boolean("split_lines", false);
        const bool save = args.boolean("save", true);
        api::RenderOptions base = renderOptionsFromArgs(ctrl, args);
        if (!args.has("dpi")) {
            base.dpi = 200;
        }
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        std::vector<std::string> blockIds, elementIds;
        for (const auto& id: args.has("block_ids") ? args.raw("block_ids") : json::array()) {
            blockIds.push_back(id.get<std::string>());
        }
        for (const auto& id: args.has("element_ids") ? args.raw("element_ids") : json::array()) {
            elementIds.push_back(id.get<std::string>());
        }
        Document* doc = ctrl->getDocument();

        runOffUi(
                ctrl,
                [=, exportDir = srv->getConfig().exportDir]() {
                    struct Crop {
                        std::string caption;
                        xoj::util::Rectangle<double> rect;
                    };
                    std::vector<Crop> crops;
                    if (!blockIds.empty()) {
                        std::vector<api::LayoutItem> items;
                        {
                            std::shared_lock lock(*doc);
                            if (pageIndex >= doc->getPageCount()) {
                                throw ToolError("Page " + std::to_string(pageIndex + 1) + " no longer exists");
                            }
                            items = layoutItems(api::elementsOnPage(doc->getPage(pageIndex), pageIndex, std::nullopt,
                                                                    std::nullopt, true));
                        }
                        const api::Layout pl = api::analyzeLayout(items);
                        for (const auto& id: blockIds) {
                            const size_t bi = parseBlockId(id, pl.blocks.size());
                            const auto& b = pl.blocks[bi];
                            if (splitLines && b.kind == "handwriting" && b.lines.size() > 1) {
                                for (size_t l = 0; l < b.lines.size(); l++) {
                                    crops.push_back({blockId(bi) + " line " + std::to_string(l + 1), b.lines[l]});
                                }
                            } else {
                                crops.push_back({blockId(bi) + " (" + b.kind + ")", b.bbox});
                            }
                        }
                    } else {
                        std::shared_lock lock(*doc);
                        std::optional<xoj::util::Rectangle<double>> area;
                        for (const auto& id: elementIds) {
                            auto loc = api::locateId(doc, id);
                            if (loc.page != pageIndex) {
                                throw ToolError("Element " + id + " is on page " + std::to_string(loc.page + 1) +
                                                ", not " + std::to_string(pageIndex + 1));
                            }
                            const auto& bb = loc.element->getBoundingBox();
                            if (!area) {
                                area = bb;
                            } else {
                                const double x1 = std::min(area->x, bb.x), y1 = std::min(area->y, bb.y);
                                const double x2 = std::max(area->x + area->width, bb.x + bb.width);
                                const double y2 = std::max(area->y + area->height, bb.y + bb.height);
                                area = xoj::util::Rectangle<double>(x1, y1, x2 - x1, y2 - y1);
                            }
                        }
                        crops.push_back({"elements", *area});
                    }
                    if (crops.size() > maxImages) {
                        crops.resize(maxImages);
                    }

                    json list = json::array();
                    std::vector<std::pair<std::string, std::string>> images;  // caption, png
                    for (const auto& crop: crops) {
                        api::RenderOptions o = base;
                        o.page = pageIndex;
                        const auto& r = crop.rect;
                        o.region = xoj::util::Rectangle<double>(r.x - pad, r.y - pad, r.width + 2 * pad,
                                                                r.height + 2 * pad);
                        const auto img = api::renderPage(doc, o);  // copies what it renders under a short lock
                        json entry = {
                                {"caption", crop.caption},
                                {"region", bboxJson(img.region.x, img.region.y, img.region.width, img.region.height)},
                                {"px_per_pt", api::roundTo(img.scale, 4)}};
                        if (save) {
                            entry["file"] = toUtf8(writeExportFile(exportDir / "renders",
                                                                   "page" + std::to_string(pageIndex + 1) + "-crop",
                                                                   "png", img.png));
                        }
                        list.push_back(std::move(entry));
                        images.emplace_back(crop.caption, img.png);
                    }
                    ToolResult result = ToolResult::structured({{"page", pageIndex + 1}, {"images", list}});
                    for (const auto& [caption, png]: images) {
                        result.addText("Image: " + caption);
                        result.addImage(base64Encode(png), "image/png");
                    }
                    return result;
                },
                std::move(respond));
    };
    server.getRegistry().addTool(std::move(blocks));

    ToolSpec shapes;
    shapes.name = "shapes_recognize";
    shapes.title = "Recognize sketched shapes";
    shapes.description =
            "Runs the app's shape recognizer (the one behind the 'shape recognizer' pen mode) on hand-drawn "
            "strokes, in the given order, and reports which ones are lines, triangles, rectangles, quadrilaterals "
            "or circles/ellipses, with their clean geometry. Strokes that together form a polygon (e.g. a "
            "triangle drawn as three lines) are recognized on the stroke that completes it. Read-only.";
    shapes.inputSchema = schema::object(
            {{"element_ids", schema::array("Stroke ids in drawing order", schema::string("element id"))}},
            {"element_ids"});
    shapes.readOnly = true;
    shapes.idempotent = true;
    shapes.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"element_ids"});
        std::shared_lock lock(*ctrl->getDocument());
        ShapeRecognizer reco;
        const double minSize = ctrl->getSettings()->getStrokeRecognizerMinSize();
        json results = json::array();
        for (const auto& idv: args.raw("element_ids")) {
            const std::string id = idv.get<std::string>();
            auto loc = api::locateId(ctrl->getDocument(), id);
            if (loc.element->getType() != ELEMENT_STROKE) {
                throw ToolError(id + " is not a stroke");
            }
            auto copy = static_cast<const Stroke*>(loc.element)->cloneStroke();
            auto shape = reco.recognizePatterns(copy.get(), minSize);
            json r = {{"id", id}};
            if (!shape) {
                r["shape"] = nullptr;
            } else {
                const auto& pts = shape->getPointVector();
                const bool closed =
                        pts.size() > 2 && std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y) < 0.5;
                const size_t corners = closed ? pts.size() - 1 : pts.size();
                std::string kind = pts.size() == 2 ? "line" :
                                   corners == 3    ? "triangle" :
                                   corners == 4    ? "quadrilateral" :
                                   corners > 8     ? "ellipse" :
                                                     "polygon";
                if (kind == "quadrilateral") {  // right angles -> rectangle
                    auto dot = [&](size_t a, size_t b, size_t c) {
                        const double ux = pts[a].x - pts[b].x, uy = pts[a].y - pts[b].y;
                        const double vx = pts[c].x - pts[b].x, vy = pts[c].y - pts[b].y;
                        return std::abs(ux * vx + uy * vy) / (std::hypot(ux, uy) * std::hypot(vx, vy) + 1e-9);
                    };
                    if (dot(0, 1, 2) < 0.1 && dot(1, 2, 3) < 0.1) {
                        kind = "rectangle";
                    }
                }
                const auto& bb = shape->getBoundingBox();
                if (kind == "ellipse" && std::abs(bb.width - bb.height) < 0.08 * std::max(bb.width, bb.height)) {
                    kind = "circle";
                }
                r["shape"] = kind;
                r["bbox"] = bboxJson(bb.x, bb.y, bb.width, bb.height);
                if (kind != "ellipse" && kind != "circle") {
                    json vertices = json::array();
                    for (size_t k = 0; k < corners; k++) {
                        vertices.push_back({api::roundTo(pts[k].x), api::roundTo(pts[k].y)});
                    }
                    r["vertices"] = std::move(vertices);
                } else {
                    r["center"] = {api::roundTo(bb.x + bb.width / 2), api::roundTo(bb.y + bb.height / 2)};
                    r["radii"] = {api::roundTo(bb.width / 2), api::roundTo(bb.height / 2)};
                }
            }
            results.push_back(std::move(r));
        }
        return ToolResult::structured({{"results", std::move(results)}});
    };
    server.getRegistry().addTool(std::move(shapes));
}

}  // namespace xoj::mcp::tools
