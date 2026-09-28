// Tools: layout_analyze

#include <cmath>         // for hypot
#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"  // for elementsOnPage
#include "api/ElementIds.h"   // for ElementIds
#include "api/Layout.h"       // for analyzeLayout
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document
#include "model/Stroke.h"    // for Stroke
#include "model/TexImage.h"  // for TexImage
#include "model/Text.h"      // for Text
#include "model/XojPage.h"   // for XojPage

#include "LayoutCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

PageLayout analyzePage(Control* ctrl, size_t pageIndex, std::optional<size_t> layer,
                       std::optional<xoj::util::Rectangle<double>> region) {
    PageLayout out;
    PageRef page = ctrl->getDocument()->getPage(pageIndex);
    out.elements = api::elementsOnPage(page, pageIndex, layer, region, true);
    std::vector<api::LayoutItem> items;
    items.reserve(out.elements.size());
    for (size_t i = 0; i < out.elements.size(); i++) {
        const Element* e = out.elements[i].element;
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
    out.layout = api::analyzeLayout(items);
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
    layout.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "layer", "region", "include_element_ids"});
        std::shared_lock lock(*ctrl->getDocument());
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        std::optional<size_t> layer;
        if (args.has("layer")) {
            layer = static_cast<size_t>(args.integer("layer", 1, 1, 1000));
        }
        const PageLayout pl = analyzePage(ctrl, pageIndex, layer, parseRegion(args));
        const bool withIds = args.boolean("include_element_ids", false);
        auto& ids = api::ElementIds::get();

        json blocks = json::array();
        for (size_t i = 0; i < pl.layout.blocks.size(); i++) {
            const auto& b = pl.layout.blocks[i];
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
            const Element* first = pl.elements[b.items.front()].element;
            if (b.kind == "typed_text") {
                jb["text"] = static_cast<const Text*>(first)->getText();
            } else if (b.kind == "latex") {
                jb["latex"] = static_cast<const TexImage*>(first)->getText();
            }
            if (withIds) {
                json list = json::array();
                for (size_t k: b.items) {
                    list.push_back(ids.idOf(pl.elements[k].element));
                }
                jb["element_ids"] = std::move(list);
            }
            blocks.push_back(std::move(jb));
        }
        json connectors = json::array();
        for (const auto& c: pl.layout.connectors) {
            json jc = {{"element_id", ids.idOf(pl.elements[c.item].element)},
                       {"from", {api::roundTo(c.from.x), api::roundTo(c.from.y)}},
                       {"to", {api::roundTo(c.to.x), api::roundTo(c.to.y)}}};
            jc["from_block"] = c.fromBlock ? json(blockId(*c.fromBlock)) : json(nullptr);
            jc["to_block"] = c.toBlock ? json(blockId(*c.toBlock)) : json(nullptr);
            connectors.push_back(std::move(jc));
        }
        json out = {{"page", pageIndex + 1},
                    {"typical_stroke_height", api::roundTo(pl.layout.typicalHeight)},
                    {"block_count", blocks.size()},
                    {"blocks", std::move(blocks)},
                    {"connectors", std::move(connectors)},
                    {"next", "Call blocks_render(page, block_ids) to see blocks at readable resolution."}};
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(layout));
}

}  // namespace xoj::mcp::tools
