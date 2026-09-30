// Tool: draft

#include <mutex>         // for unique_lock
#include <shared_mutex>  // for shared_lock

#include "api/Drafts.h"       // for Drafts
#include "api/DrawApi.h"      // for DrawApi
#include "api/RenderApi.h"    // for renderPage
#include "control/Control.h"  // for Control
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/Document.h"  // for Document
#include "model/Layer.h"     // for Layer
#include "model/XojPage.h"   // for XojPage

#include "DrawCommon.h"
#include "RenderCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerDraftTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    std::vector<schema::Property> props = {
            {"op", schema::enumeration("begin | render | commit | discard | list",
                                       {"begin", "render", "commit", "discard", "list"})},
            {"draft", schema::string("Draft id (from begin), for render/commit/discard")},
            {"page", schema::integer("begin: page (1-based); default: current page")},
            {"with_page", schema::withDefault(schema::boolean("render: show the draft over the page content"), true)},
            {"region", schema::array("render: area [x, y, width, height]", schema::number("coordinate"))},
            {"layer", schema::string("commit: target layer (default: the user's setting, see app_status)")},
            {"animate", schema::boolean("commit: draw the result progressively")},
            {"speed", schema::number("commit: animation speed")}};
    addRenderSchemaProperties(props);
    ToolSpec draft;
    draft.name = "draft";
    draft.title = "Drafts";
    draft.description =
            "Iterate on a drawing privately before showing it: op=begin creates a hidden draft layer on a page and "
            "returns its layer name - pass it as 'layer' to create_strokes/create_shapes/create_from_svg/"
            "create_text. op=render shows the draft (over the page) so you can check and fix it; drafts have no "
            "undo history, and element tools (elements_edit/elements_delete) work on draft content. op=commit "
            "moves everything into the target layer as ONE undo step (optionally animated); op=discard throws the "
            "draft away.";
    draft.inputSchema = schema::object(std::move(props), {"op"});
    draft.tier = Tier::Draw;
    draft.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "draft", "page", "with_page", "region", "layer", "animate", "speed", "dpi", "max_px",
                            "background", "grid", "grid_step", "save"});
        const std::string op = args.choice("op", {"begin", "render", "commit", "discard", "list"}, "");
        auto& drafts = api::Drafts::get();
        if (op == "list") {
            json list = json::array();
            for (const auto& [id, d]: drafts.all()) {
                list.push_back(
                        {{"draft", id}, {"layer", d.layerName}, {"elements", d.layer->getElementsView().size()}});
            }
            respond(ToolResult::structured({{"drafts", list}}));
            return;
        }
        if (op == "begin") {
            size_t pageIndex = 0;
            {
                std::shared_lock lock(*ctrl->getDocument());
                pageIndex = resolvePageIndex(ctrl, args);
            }
            const auto& d = drafts.begin(ctrl, pageIndex);
            respond(ToolResult::structured(
                    {{"draft", d.id},
                     {"page", pageIndex + 1},
                     {"layer", d.layerName},
                     {"next", "Draw with layer=\"" + d.layerName + "\", check with draft(op=\"render\", draft=\"" +
                                      d.id + "\"), then commit or discard"}}));
            return;
        }
        const std::string id = args.str("draft");
        api::Draft& d = drafts.find(ctrl, id);
        size_t pageIndex = 0;
        size_t draftIndex = 0;
        {
            std::shared_lock lock(*ctrl->getDocument());
            pageIndex = ctrl->getDocument()->indexOf(d.page);
            const auto& layers = d.page->getLayers();
            draftIndex = static_cast<size_t>(std::find(layers.begin(), layers.end(), d.layer) - layers.begin()) + 1;
        }
        if (op == "render") {
            api::RenderOptions o = renderOptionsFromArgs(ctrl, args);
            o.page = pageIndex;
            o.region = parseRegion(args);
            std::vector<size_t> layers;
            if (args.boolean("with_page", true)) {
                for (size_t i = 0; i < d.page->getLayerCount(); i++) {
                    if (d.page->getLayers()[i]->isVisible()) {
                        layers.push_back(i + 1);
                    }
                }
            } else {
                o.background = false;
            }
            layers.push_back(draftIndex);
            o.layers = layers;
            const auto img = api::renderPage(ctrl->getDocument(), o);
            respond(renderResult(srv->getConfig().exportDir, img, pageIndex, args.boolean("save", false),
                                 "draft-" + id));
            return;
        }
        if (op == "discard") {
            const size_t count = d.layer->getElementsView().size();
            drafts.close(ctrl, id);
            respond(ToolResult::structured({{"draft", id}, {"discarded_elements", count}}));
            return;
        }
        // commit: take the content out of the draft, remove the draft, insert as one undo step
        std::vector<ElementPtr> elements;
        {
            std::unique_lock lock(*ctrl->getDocument());
            elements = d.layer->clearNoFree();
        }
        if (elements.empty()) {
            drafts.close(ctrl, id);
            throw ToolError("Draft " + id + " is empty; nothing committed (the draft was closed)");
        }
        drafts.close(ctrl, id);
        json extra = {{"draft", id}};
        insertAndRespond(*srv, args, std::move(elements), respond, extra);
    };
    server.getRegistry().addTool(std::move(draft));
}

}  // namespace xoj::mcp::tools
