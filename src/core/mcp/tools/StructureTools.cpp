// Tools: page_manage, layer_manage

#include <gtk/gtk.h>

#include "api/DocumentApi.h"                         // for currentPageIndex
#include "control/Control.h"                         // for Control
#include "control/PageBackgroundChangeController.h"  // for PageBackgroundChangeController
#include "control/ScrollHandler.h"                   // for ScrollHandler
#include "control/layer/LayerController.h"           // for LayerController
#include "control/pagetype/PageTypeHandler.h"        // for PageTypeHandler
#include "gui/MainWindow.h"                          // for MainWindow
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/Document.h"   // for Document
#include "model/Layer.h"      // for Layer
#include "model/PaperSize.h"  // for PaperSize
#include "model/XojPage.h"    // for XojPage

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

void gotoPage(Control* ctrl, size_t index) {
    ctrl->getScrollHandler()->scrollToPage(index);
    ctrl->firePageSelected(index);
}

size_t pageCount(Control* ctrl) { return ctrl->getDocument()->getPageCount(); }

/// Resolves a layer argument (1-based index or name) on a page; returns the 1-based index
size_t resolveLayerArg(const PageRef& page, const json& v) {
    const auto& layers = page->getLayers();
    if (v.is_number() || (v.is_string() && !v.get<std::string>().empty() &&
                          v.get<std::string>().find_first_not_of("0123456789") == std::string::npos)) {
        const auto n = static_cast<size_t>(Args::toNumber(v, "layer"));
        if (n < 1 || n > layers.size()) {
            throw ToolError("Layer " + std::to_string(n) + " does not exist (the page has " +
                            std::to_string(layers.size()) + " layers)");
        }
        return n;
    }
    const std::string name = v.get<std::string>();
    for (size_t i = 0; i < layers.size(); i++) {
        if ((layers[i]->hasName() ? layers[i]->getName() : "Layer " + std::to_string(i + 1)) == name) {
            return i + 1;
        }
    }
    throw ToolError("No layer named '" + name + "' on this page");
}

json pageState(Control* ctrl, size_t index) {
    std::shared_lock lock(*ctrl->getDocument());
    json out = pageSummary(ctrl->getDocument()->getPage(index), index);
    out["page_count"] = pageCount(ctrl);
    out["current_page"] = api::currentPageIndex(ctrl) + 1;
    return out;
}

}  // namespace

void registerStructureTools(McpServer& server) {
    Control* ctrl = server.getControl();

    std::vector<std::string> formats = {"plain", "ruled",  "lined",     "staves",
                                        "graph", "dotted", "isodotted", "isograph"};
    ToolSpec page;
    page.name = "page_manage";
    page.title = "Manage pages";
    page.description =
            "Page operations (undoable like in the app): insert (after 'page', 0 = at the beginning; optional "
            "background), delete, duplicate, move (to position 'to'), background (paper type and/or color), size "
            "(width/height in points; A4 = 595 x 842), goto (show a page). Only goto moves the user's view: the "
            "others change the page where it is and leave the user on theirs (an inserted page does not become the "
            "current one: pass its number as 'page' to later calls). show=true also takes the user there; use it "
            "only when the user asked to be taken there.";
    page.inputSchema = schema::object({{"op", schema::enumeration("Operation", {"insert", "delete", "duplicate", "move",
                                                                                "background", "size", "goto"})},
                                       {"page", schema::integer("Page (1-based); default: current page")},
                                       {"to", schema::integer("move: new position (1-based)")},
                                       {"background", schema::enumeration("insert/background: paper type", formats)},
                                       {"color", schema::string("background: paper color")},
                                       {"width", schema::number("size: width in points")},
                                       {"height", schema::number("size: height in points")},
                                       {"show", schema::withDefault(
                                                        schema::boolean("Also take the user's view to the page (only "
                                                                        "when they asked for it)"),
                                                        false)}},
                                      {"op"});
    page.tier = Tier::Ui;
    page.asyncHandler = [ctrl, formats, srv = &server](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "page", "to", "background", "color", "width", "height", "show"});
        const std::string op =
                args.choice("op", {"insert", "delete", "duplicate", "move", "background", "size", "goto"}, "");
        auto format = [&]() -> std::optional<PageType> {
            if (!args.has("background")) {
                return std::nullopt;
            }
            const std::string f = args.str("background");
            if (std::find(formats.begin(), formats.end(), f) == formats.end()) {
                throw ToolError("background must be one of plain, ruled, lined, staves, graph, dotted, isodotted, "
                                "isograph");
            }
            return PageType(PageTypeHandler::getPageTypeFormatForString(f));
        };
        // (on the document, without the window's page actions: they work on the current page, and making a page
        // current moved the user's view, 2026-10-20)
        PageBackgroundChangeController* backgrounds = ctrl->getPageBackgroundChangeController();
        const bool show = args.boolean("show", false);
        // Taking the user there waits until they are between gestures
        auto go = [ctrl, respond](size_t index, std::string text) {
            PageRef page;
            {
                std::shared_lock lock(*ctrl->getDocument());
                page = ctrl->getDocument()->getPage(index);
            }
            whenUserBetweenGestures(ctrl, [ctrl, page, text, respond] {
                size_t now = ctrl->getDocument()->indexOf(page);  // (pages may have moved meanwhile)
                if (now != npos) {
                    gotoPage(ctrl, now);
                }
                respond(ToolResult::structured(pageState(ctrl, now == npos ? 0 : now), text));
            });
        };
        auto done = [&](size_t index, const std::string& text = "") {
            if (show) {
                go(index, text);
            } else {
                respond(ToolResult::structured(pageState(ctrl, index), text));
            }
        };
        if (op == "insert") {
            const auto after =
                    static_cast<size_t>(args.integer("page", static_cast<int64_t>(api::currentPageIndex(ctrl) + 1), 0,
                                                     static_cast<int64_t>(pageCount(ctrl))));
            PageRef like;
            {
                std::shared_lock lock(*ctrl->getDocument());
                like = ctrl->getDocument()->getPage(after == 0 ? 0 : after - 1);
            }
            PageRef page = backgrounds->newPageLike(like);
            if (auto f = format()) {
                page->setBackgroundType(*f);
            }
            ctrl->insertPage(page, after, false);  // after = the 0-based index of the new page
            done(after, "Inserted page " + std::to_string(after + 1));
            return;
        }
        const size_t index = resolvePageIndex(ctrl, args);
        PageRef target;
        {
            std::shared_lock lock(*ctrl->getDocument());
            target = ctrl->getDocument()->getPage(index);
        }
        if (op == "goto") {
            go(index, "");
            return;
        }
        ctrl->clearSelectionEndTextOn(target);
        if (op == "delete") {
            const std::string backup = srv->backup("before-delete-page");
            if (!ctrl->deletePageAt(index)) {
                throw ToolError("The app does not allow deleting it (the last page cannot be deleted)");
            }
            json out = {{"deleted_page", index + 1}, {"page_count", pageCount(ctrl)}};
            if (!backup.empty()) {
                out["backup"] = backup;
            }
            respond(ToolResult::structured(std::move(out)));
            return;
        }
        if (op == "duplicate") {
            ctrl->insertPage(std::make_shared<XojPage>(*target), index + 1, false);
            done(index + 1, "Duplicated as page " + std::to_string(index + 2));
            return;
        }
        if (op == "move") {
            const auto to = static_cast<size_t>(args.integer("to", 0, 1, static_cast<int64_t>(pageCount(ctrl)))) - 1;
            ctrl->movePage(index, to);
            done(to, "Moved to position " + std::to_string(to + 1));
            return;
        }
        if (op == "background") {
            if (!args.has("background") && !args.has("color")) {
                throw ToolError("Give 'background' (paper type) and/or 'color'");
            }
            if (auto f = format()) {
                backgrounds->changePageBackground(index, *f);
            }
            if (args.has("color")) {
                target->setBackgroundColor(parseColor(args.raw("color")));
                ctrl->firePageChanged(index);
            }
            done(index);
            return;
        }
        // size
        const double w = args.number("width", 0, 10, 20000), h = args.number("height", 0, 10, 20000);
        backgrounds->changePageSize(index, PaperSize(w, h));
        done(index);
    };
    server.getRegistry().addTool(std::move(page));

    ToolSpec layer;
    layer.name = "layer_manage";
    layer.title = "Manage layers";
    layer.description =
            "Layer operations on a page (like the layer panel): add (optional name, above the selected layer), "
            "rename, show, hide, select (make it the drawing layer), delete, copy, merge_down, move_up, move_down. "
            "Identify the layer by number (1 = bottom) or name. The user's view stays where it is.";
    layer.inputSchema = schema::object(
            {{"op", schema::enumeration("Operation", {"add", "rename", "show", "hide", "select", "delete", "copy",
                                                      "merge_down", "move_up", "move_down"})},
             {"page", schema::integer("Page (1-based); default: current page")},
             {"layer", schema::string("Layer number (1 = bottom) or name; default: the selected layer")},
             {"name", schema::string("add/rename: the (new) name")}},
            {"op"});
    layer.tier = Tier::Ui;
    layer.handler = [ctrl, srv = &server](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "page", "layer", "name"});
        const std::string op = args.choice(
                "op",
                {"add", "rename", "show", "hide", "select", "delete", "copy", "merge_down", "move_up", "move_down"},
                "");
        const size_t index = resolvePageIndex(ctrl, args);
        PageRef p = ctrl->getDocument()->getPage(index);
        ctrl->clearSelectionEndTextOn(p);
        LayerController* lc = ctrl->getLayerController();
        // (on that page, without making it current: that moved the user's view, 2026-10-20)
        lc->workOnPage(index);
        struct Back {
            LayerController* lc;
            ~Back() { lc->workOnCurrentPage(); }
        } back{lc};
        size_t layerIndex = std::max<size_t>(p->getSelectedLayerId(), 1);
        if (args.has("layer")) {
            layerIndex = resolveLayerArg(p, args.raw("layer"));
        }
        if (op != "add") {
            lc->switchToLay(layerIndex, false, false);
        }
        if (op == "add") {
            lc->addNewLayer(false);
            if (args.has("name")) {
                lc->setCurrentLayerName(args.str("name"));
            }
        } else if (op == "rename") {
            lc->setCurrentLayerName(args.str("name"));
        } else if (op == "show" || op == "hide") {
            lc->setLayerVisible(layerIndex, op == "show");
        } else if (op == "delete") {
            if (p->getLayerCount() <= 1) {
                throw ToolError("A page needs at least one layer");
            }
            srv->backup("before-delete-layer");
            lc->deleteCurrentLayer();
        } else if (op == "copy") {
            lc->copyCurrentLayer();
        } else if (op == "merge_down") {
            if (layerIndex <= 1) {
                throw ToolError("The bottom layer cannot be merged down");
            }
            lc->mergeCurrentLayerDown();
        } else if (op == "move_up" || op == "move_down") {
            lc->moveCurrentLayer(op == "move_up");
        }
        std::shared_lock lock(*ctrl->getDocument());
        return ToolResult::structured(pageSummary(p, index));
    };
    server.getRegistry().addTool(std::move(layer));
}

}  // namespace xoj::mcp::tools
