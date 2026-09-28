// Tools: page_render

#include <shared_mutex>  // for shared_lock
#include <stdexcept>     // for invalid_argument

#include "api/DocumentApi.h"  // for locateId
#include "api/Geometry.h"     // for roundTo
#include "api/RenderApi.h"    // for renderPage
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Media.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document

#include "RenderCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void addRenderSchemaProperties(std::vector<schema::Property>& props) {
    props.push_back({"dpi", schema::withDefault(schema::number("Resolution; 72 = 1 pixel per point"), 100)});
    props.push_back({"max_px", schema::withDefault(schema::integer("Maximum size of the longer image side"), 1400)});
    props.push_back(
            {"background", schema::withDefault(schema::boolean("Draw paper, ruling and PDF background"), true)});
    props.push_back({"grid", schema::withDefault(schema::boolean("Overlay a labelled grid in page points (helps to "
                                                                 "place drawings precisely)"),
                                                 false)});
    props.push_back({"grid_step", schema::withDefault(schema::number("Grid spacing in points"), 50)});
    props.push_back({"save", schema::withDefault(schema::boolean("Also save the PNG to a file and return its path "
                                                                 "(for clients that cannot show images)"),
                                                 true)});
}

api::RenderOptions renderOptionsFromArgs(Control* ctrl, const Args& args) {
    api::RenderOptions o;
    o.dpi = args.number("dpi", 100, 10, 600);
    o.maxPixels = static_cast<int>(args.integer("max_px", 1400, 16, 8000));
    o.background = args.boolean("background", true);
    o.grid = args.boolean("grid", false);
    o.gridStep = args.number("grid_step", 50, 5, 1000);
    if (args.has("highlight")) {
        for (const auto& id: args.raw("highlight")) {
            if (!id.is_string()) {
                throw ToolError("'highlight' must be a list of element ids like [\"e12\"]");
            }
            auto loc = api::locateId(ctrl->getDocument(), id.get<std::string>());
            o.highlights.push_back({loc.element->getBoundingBox(), id.get<std::string>()});
        }
    }
    return o;
}

ToolResult renderResult(McpServer& server, const api::RenderedImage& img, size_t pageIndex, bool save,
                        const std::string& stem) {
    json meta = {{"page", pageIndex + 1},
                 {"region", bboxJson(img.region.x, img.region.y, img.region.width, img.region.height)},
                 {"width_px", img.widthPx},
                 {"height_px", img.heightPx},
                 {"px_per_pt", api::roundTo(img.scale, 4)},
                 {"to_page_coords", "page_x = region[0] + px / px_per_pt; page_y = region[1] + py / px_per_pt"}};
    ToolResult r;
    if (save) {
        const auto file = writeExportFile(server.getConfig().exportDir / "renders", stem, "png", img.png);
        meta["file"] = toUtf8(file);
    }
    r = ToolResult::structured(meta);
    r.addImage(base64Encode(img.png), "image/png");
    return r;
}

void registerRenderTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    std::vector<schema::Property> props = {
            {"page", schema::integer("Page number (1-based); default: current page")},
            {"region", schema::array("Area to render as [x, y, width, height] in page points; default: whole page",
                                     schema::number("coordinate"))},
            {"layers",
             schema::array("Only these layers (1-based); default: all visible layers", schema::integer("layer"))},
            {"highlight", schema::array("Element ids to outline (e.g. to show the user what you mean)",
                                        schema::string("element id"))}};
    addRenderSchemaProperties(props);

    ToolSpec render;
    render.name = "page_render";
    render.title = "Render page";
    render.description =
            "Renders a page, or an area of it, to a PNG image so you can SEE handwriting, drawings and diagrams. "
            "Returns the image plus metadata to map pixels back to page coordinates. Use region + a higher dpi "
            "to read small handwriting; grid=true overlays page coordinates to help you place new content; "
            "highlight outlines elements by id.";
    render.inputSchema = schema::object(std::move(props));
    render.readOnly = true;
    render.idempotent = true;
    render.handler = [ctrl, srv](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown(
                {"page", "region", "layers", "highlight", "dpi", "max_px", "background", "grid", "grid_step", "save"});
        api::RenderOptions o = renderOptionsFromArgs(ctrl, args);
        o.page = resolvePageIndex(ctrl, args);
        o.region = parseRegion(args);
        if (args.has("layers")) {
            std::vector<size_t> layers;
            for (double l: args.numbers("layers")) {
                layers.push_back(static_cast<size_t>(l));
            }
            o.layers = layers;
        }
        const auto img = api::renderPage(ctrl->getDocument(), o);
        return renderResult(*srv, img, o.page, args.boolean("save", true), "page" + std::to_string(o.page + 1));
    };
    server.getRegistry().addTool(std::move(render));
}

}  // namespace xoj::mcp::tools
