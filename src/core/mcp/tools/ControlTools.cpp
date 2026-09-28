// Tools: tool_get, tool_set, view, clipboard

#include <set>  // for set

#include <gtk/gtk.h>

#include "api/EditApi.h"               // for EditApi
#include "api/ElementIds.h"            // for ElementIds
#include "control/Control.h"           // for Control
#include "control/ScrollHandler.h"     // for ScrollHandler
#include "control/Tool.h"              // for Tool
#include "control/ToolEnums.h"         // for toolTypeToString
#include "control/ToolHandler.h"       // for ToolHandler
#include "control/zoom/ZoomControl.h"  // for ZoomControl
#include "gui/MainWindow.h"            // for MainWindow
#include "gui/XournalView.h"           // for XournalView
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "mcp/Variant.h"
#include "model/Document.h"       // for Document
#include "model/Layer.h"          // for Layer
#include "model/StrokeStyle.h"    // for parseStyle, formatStyle
#include "model/XojPage.h"        // for XojPage
#include "pdf/base/XojPdfPage.h"  // for XojPdfRectangle

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

const std::map<std::string, ToolSize>& sizeNames() {
    static const std::map<std::string, ToolSize> s = {{"very_fine", TOOL_SIZE_VERY_FINE},
                                                      {"fine", TOOL_SIZE_FINE},
                                                      {"medium", TOOL_SIZE_MEDIUM},
                                                      {"thick", TOOL_SIZE_THICK},
                                                      {"very_thick", TOOL_SIZE_VERY_THICK}};
    return s;
}

json toolJson(Control* ctrl) {
    ToolHandler* th = ctrl->getToolHandler();
    const ToolType type = th->getToolType();
    json out = {{"tool", std::string(toolTypeToString(type))},
                {"color", colorToHex(th->getColor())},
                {"size", std::string(toolSizeToString(th->getSize()))},
                {"thickness", th->getThickness()},
                {"drawing_type", std::string(drawingTypeToString(th->getDrawingType()))}};
    if (type == TOOL_PEN || type == TOOL_HIGHLIGHTER) {
        out["line_style"] = StrokeStyle::formatStyle(th->getLineStyle());
        out["fill"] = th->getFill() >= 0 ? json(th->getFill() / 255.0) : json(nullptr);
    }
    if (type == TOOL_ERASER) {
        out["eraser_type"] = std::string(eraserTypeToString(th->getEraserType()));
    }
    return out;
}

GActionGroup* win(Control* ctrl) { return G_ACTION_GROUP(ctrl->getWindow()->getWindow()); }

json actionState(Control* ctrl, const char* name) {
    GVariant* st = g_action_group_get_action_state(win(ctrl), name);
    json out = variantToJson(st);
    if (st) {
        g_variant_unref(st);
    }
    return out;
}

void setActionState(Control* ctrl, const char* name, bool value) {
    g_action_group_change_action_state(win(ctrl), name, g_variant_new_boolean(value));
}

json viewJson(Control* ctrl) {
    const size_t page = ctrl->getCurrentPageNo();
    json out = {{"current_page", page + 1},
                {"zoom", ctrl->getZoomControl()->getZoomReal()},
                {"fullscreen", actionState(ctrl, "fullscreen")},
                {"presentation_mode", actionState(ctrl, "presentation-mode")},
                {"sidebar", actionState(ctrl, "show-sidebar")}};
    std::unique_ptr<xoj::util::Rectangle<double>> rect(ctrl->getWindow()->getXournal()->getVisibleRect(page));
    if (rect) {
        out["visible_region"] = bboxJson(rect->x, rect->y, rect->width, rect->height);
    }
    return out;
}

}  // namespace

void registerControlTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec get;
    get.name = "tool_get";
    get.title = "Current tool";
    get.description = "The user's active tool and its settings (color, size, thickness, drawing type, line style, "
                      "fill, eraser type).";
    get.inputSchema = schema::object({});
    get.readOnly = true;
    get.handler = [ctrl](const json&) {
        requireDocument(ctrl);
        return ToolResult::structured(toolJson(ctrl));
    };
    server.getRegistry().addTool(std::move(get));

    std::vector<std::string> toolNames, sizes;
    for (size_t t = TOOL_PEN; t < TOOL_END_ENTRY; t++) {
        toolNames.emplace_back(toolTypeToString(static_cast<ToolType>(t)));
    }
    for (const auto& [n, s]: sizeNames()) {
        sizes.push_back(n);
    }
    ToolSpec set;
    set.name = "tool_set";
    set.title = "Change tool";
    set.description =
            "Changes the user's active tool and its settings, like the toolbar: tool (pen, highlighter, eraser, "
            "text, selectRect, selectRegion, hand, ...), color, size, drawing_type (default, line, rectangle, "
            "ellipse, arrow, doubleArrow, drawCoordinateSystem, strokeRecognizer, spline), line_style, "
            "fill_opacity (0..1 or -1 off), eraser_type (default, whiteout, deleteStroke). This changes what the "
            "user draws with next - to draw yourself use pen_draw/create_* instead.";
    set.inputSchema = schema::object(
            {{"tool", schema::enumeration("Tool", toolNames)},
             {"color", schema::string("Tool color")},
             {"size", schema::enumeration("Tool size", sizes)},
             {"drawing_type", schema::enumeration("Pen/highlighter mode",
                                                  {"default", "line", "rectangle", "ellipse", "arrow", "doubleArrow",
                                                   "drawCoordinateSystem", "strokeRecognizer", "spline"})},
             {"line_style", schema::enumeration("Pen dash pattern", {"plain", "dash", "dot", "dashdot"})},
             {"fill_opacity", schema::number("Fill for pen/highlighter shapes 0..1, or -1 for no fill")},
             {"eraser_type", schema::enumeration("Eraser", {"default", "whiteout", "deleteStroke"})}});
    set.tier = Tier::Ui;
    set.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"tool", "color", "size", "drawing_type", "line_style", "fill_opacity", "eraser_type"});
        ToolHandler* th = ctrl->getToolHandler();
        if (args.has("tool")) {
            const ToolType t = toolTypeFromString(args.str("tool"));
            if (t == TOOL_NONE) {
                throw ToolError("Unknown tool '" + args.str("tool") + "'");
            }
            ctrl->selectTool(t);
        }
        if (args.has("color")) {
            th->setColor(parseColor(args.raw("color")), true);
        }
        if (args.has("size")) {
            auto s = sizeNames().find(args.str("size"));
            if (s == sizeNames().end()) {
                throw ToolError("size must be very_fine, fine, medium, thick or very_thick");
            }
            th->setSize(s->second);
        }
        if (args.has("drawing_type")) {
            th->setDrawingType(drawingTypeFromString(args.str("drawing_type")));
        }
        if (args.has("line_style")) {
            th->setLineStyle(StrokeStyle::parseStyle(args.str("line_style")));
        }
        if (args.has("fill_opacity")) {
            const double f = args.number("fill_opacity");
            th->setFillEnabled(f >= 0);
            if (f >= 0) {
                th->setPenFill(static_cast<int>(std::lround(std::min(f, 1.0) * 255)));
            }
        }
        if (args.has("eraser_type")) {
            th->setEraserType(eraserTypeFromString(args.str("eraser_type")));
        }
        th->fireToolChanged();
        return ToolResult::structured(toolJson(ctrl));
    };
    server.getRegistry().addTool(std::move(set));

    ToolSpec view;
    view.name = "view";
    view.title = "View";
    view.description = "The user's view: op=get (current page, zoom, visible region of the page in points, fullscreen, "
                       "presentation mode, sidebar), zoom (factor, 1 = 100%), zoom_fit, zoom_100, scroll (show 'page', "
                       "optionally 'region'), fullscreen / presentation / sidebar (on=true/false).";
    view.inputSchema = schema::object(
            {{"op",
              schema::withDefault(schema::enumeration("Operation", {"get", "zoom", "zoom_fit", "zoom_100", "scroll",
                                                                    "fullscreen", "presentation", "sidebar"}),
                                  "get")},
             {"factor", schema::number("zoom: 1 = 100%")},
             {"page", schema::integer("scroll: page (1-based)")},
             {"region", schema::array("scroll: area to show [x, y, width, height]", schema::number("coordinate"))},
             {"on", schema::boolean("fullscreen/presentation/sidebar: on or off")}});
    view.tier = Tier::Ui;
    view.readOnly = false;
    view.asyncHandler = [ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "factor", "page", "region", "on"});
        const std::string op = args.choice(
                "op", {"get", "zoom", "zoom_fit", "zoom_100", "scroll", "fullscreen", "presentation", "sidebar"},
                "get");
        if (op == "zoom") {
            g_action_group_change_action_state(win(ctrl), "zoom",
                                               g_variant_new_double(args.number("factor", 1, 0.1, 10)));
        } else if (op == "zoom_fit") {
            g_action_group_activate_action(win(ctrl), "zoom-fit", nullptr);
        } else if (op == "zoom_100") {
            g_action_group_activate_action(win(ctrl), "zoom-100", nullptr);
        } else if (op == "scroll") {
            const size_t page = resolvePageIndex(ctrl, args);
            if (auto r = parseRegion(args)) {
                ctrl->getScrollHandler()->scrollToPage(page,
                                                       XojPdfRectangle(r->x, r->y, r->x + r->width, r->y + r->height));
            } else {
                ctrl->getScrollHandler()->scrollToPage(page);
            }
            ctrl->firePageSelected(page);
        } else if (op != "get") {
            const char* action = op == "fullscreen"   ? "fullscreen" :
                                 op == "presentation" ? "presentation-mode" :
                                                        "show-sidebar";
            setActionState(ctrl, action, args.boolean("on", true));
        }
        // Layout changes settle asynchronously
        whenReady([] { return false; }, [ctrl, respond](bool) { respond(ToolResult::structured(viewJson(ctrl))); },
                  op == "get" ? 0 : 150);
    };
    server.getRegistry().addTool(std::move(view));

    ToolSpec clip;
    clip.name = "clipboard";
    clip.title = "Clipboard";
    clip.description =
            "System clipboard: op=copy / cut (element_ids: they are selected and copied like Ctrl+C / Ctrl+X), "
            "paste (like Ctrl+V: pastes into the current page; returns the new element ids), get_text, set_text.";
    clip.inputSchema =
            schema::object({{"op", schema::enumeration("Operation", {"copy", "cut", "paste", "get_text", "set_text"})},
                            {"element_ids", schema::array("copy/cut: elements", schema::string("element id"))},
                            {"text", schema::string("set_text: the text")}},
                           {"op"});
    clip.tier = Tier::Ui;
    clip.asyncHandler = [ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "element_ids", "text"});
        const std::string op = args.choice("op", {"copy", "cut", "paste", "get_text", "set_text"}, "");
        GtkClipboard* cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        if (op == "set_text") {
            const std::string text = args.str("text");
            gtk_clipboard_set_text(cb, text.c_str(), static_cast<gint>(text.size()));
            respond(ToolResult::structured({{"ok", true}}));
            return;
        }
        if (op == "get_text") {
            gtk_clipboard_request_text(
                    cb,
                    [](GtkClipboard*, const gchar* text, gpointer data) {
                        auto* r = static_cast<Responder*>(data);
                        (*r)(ToolResult::structured({{"text", text ? json(text) : json(nullptr)}}));
                        delete r;
                    },
                    new Responder(respond));
            return;
        }
        if (op == "copy" || op == "cut") {
            std::vector<std::string> ids;
            for (const auto& v: args.raw("element_ids")) {
                ids.push_back(v.get<std::string>());
            }
            api::EditApi edit(ctrl);
            auto g = edit.resolve(ids);
            edit.select(g);
            g_action_group_activate_action(win(ctrl), op.c_str(), nullptr);
            ctrl->clearSelectionEndText();  // put copied elements back into their layer
            respond(ToolResult::structured({{op == "copy" ? "copied" : "cut", ids.size()}}));
            return;
        }
        // paste: report what appeared on the current page
        ctrl->clearSelectionEndText();
        const size_t page = ctrl->getCurrentPageNo();
        auto snapshot = [ctrl, page] {
            std::set<const Element*> s;
            std::shared_lock lock(*ctrl->getDocument());
            for (const Layer* l: ctrl->getDocument()->getPage(page)->getLayersView()) {
                for (const Element* e: l->getElementsView()) {
                    s.insert(e);
                }
            }
            return s;
        };
        auto before = std::make_shared<std::set<const Element*>>(snapshot());
        g_action_group_activate_action(win(ctrl), "paste", nullptr);
        whenReady([] { return false; },
                  [ctrl, before, snapshot, page, respond](bool) {
                      ctrl->clearSelectionEndText();  // drop the pasted selection into the layer
                      json created = json::array();
                      for (const Element* e: snapshot()) {
                          if (!before->count(e)) {
                              created.push_back(api::ElementIds::get().idOf(e));
                          }
                      }
                      respond(ToolResult::structured({{"page", page + 1}, {"pasted", created}}));
                  },
                  300);
    };
    server.getRegistry().addTool(std::move(clip));
}

}  // namespace xoj::mcp::tools
