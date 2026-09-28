// Tool: find_free_space

#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"  // for locateId
#include "api/Placement.h"    // for findFreeSpace
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document
#include "model/XojPage.h"   // for XojPage

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerPlacementTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec free;
    free.name = "find_free_space";
    free.title = "Find free space";
    free.description =
            "Finds an empty area of the given size on a page, so new content doesn't overlap the user's notes. "
            "Without 'near' it returns the first free spot in reading order; with near (an element id, or an "
            "area [x,y,w,h]) the closest free spot, optionally on one side (right, below, left, above). If "
            "nothing fits, draw on a new page (drawing tools accept new_page=true).";
    free.inputSchema = schema::object(
            {{"page", schema::integer("Page (1-based); default: current page")},
             {"width", schema::number("Needed width in points")},
             {"height", schema::number("Needed height in points")},
             {"margin", schema::withDefault(schema::number("Distance to existing content and page edges"), 12)},
             {"near_element", schema::string("Element id to place next to")},
             {"near_area", schema::array("Area [x, y, width, height] to place next to", schema::number("coordinate"))},
             {"side", schema::withDefault(schema::enumeration("Where relative to 'near'",
                                                              {"any", "right", "below", "left", "above"}),
                                          "any")}},
            {"width", "height"});
    free.readOnly = true;
    free.idempotent = true;
    free.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"page", "width", "height", "margin", "near_element", "near_area", "side"});
        std::shared_lock lock(*ctrl->getDocument());
        const size_t pageIndex = resolvePageIndex(ctrl, args);
        PageRef page = ctrl->getDocument()->getPage(pageIndex);
        std::optional<xoj::util::Rectangle<double>> near = parseRegion(args, "near_area");
        if (args.has("near_element")) {
            auto loc = api::locateId(ctrl->getDocument(), args.str("near_element"));
            if (loc.page != pageIndex) {
                throw ToolError("Element " + args.str("near_element") + " is on page " + std::to_string(loc.page + 1));
            }
            near = loc.element->getBoundingBox();
        }
        auto found = api::findFreeSpace(api::occupiedAreas(page), page->getWidth(), page->getHeight(),
                                        args.number("width", 0, 0.1, 100000), args.number("height", 0, 0.1, 100000),
                                        args.number("margin", 12, 0, 500), near, args.str("side", "any"));
        if (!found) {
            return ToolResult::structured(
                    {{"found", false},
                     {"page", pageIndex + 1},
                     {"suggestion", "No room on this page: use a smaller size, another page, or new_page=true"}});
        }
        return ToolResult::structured({{"found", true},
                                       {"page", pageIndex + 1},
                                       {"region", bboxJson(found->x, found->y, found->width, found->height)}});
    };
    server.getRegistry().addTool(std::move(free));
}

}  // namespace xoj::mcp::tools
