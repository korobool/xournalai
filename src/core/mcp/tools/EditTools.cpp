// Tools: elements_select, elements_edit, elements_delete, undo, redo, history

#include "api/EditApi.h"                 // for EditApi
#include "api/ElementIds.h"              // for ElementIds
#include "control/Control.h"             // for Control
#include "control/UndoRedoController.h"  // for UndoRedoController
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"
#include "model/StrokeStyle.h"     // for parseStyle
#include "undo/UndoRedoHandler.h"  // for UndoRedoHandler

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::vector<std::string> idsFrom(const Args& args) {
    std::vector<std::string> ids;
    if (args.has("element_ids")) {
        for (const auto& v: args.raw("element_ids")) {
            if (!v.is_string()) {
                throw ToolError("element_ids must be strings like \"e12\"");
            }
            ids.push_back(v.get<std::string>());
        }
    }
    if (args.has("operation")) {
        const auto elements = api::ElementIds::get().elementsOfOperation(args.str("operation"));
        if (elements.empty()) {
            throw ToolError("No elements from operation '" + args.str("operation") + "' remain");
        }
        for (const Element* e: elements) {
            ids.push_back(api::ElementIds::get().idOf(e));
        }
    }
    if (ids.empty()) {
        throw ToolError("Give element_ids or an operation id (e.g. \"op3\" from a drawing tool)");
    }
    return ids;
}

json targetSchema(std::vector<schema::Property> more) {
    std::vector<schema::Property> props = {
            {"element_ids", schema::array("Elements to act on", schema::string("element id"))},
            {"operation", schema::string("Or: every element created by this operation (e.g. \"op3\")")}};
    props.insert(props.end(), more.begin(), more.end());
    return schema::object(std::move(props));
}

json groupJson(const api::ElementGroup& g) {
    return {{"page", g.pageIndex + 1},
            {"count", g.all.size()},
            {"bbox", bboxJson(g.x1, g.y1, g.x2 - g.x1, g.y2 - g.y1)}};
}

}  // namespace

void registerEditTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec select;
    select.name = "elements_select";
    select.title = "Select elements";
    select.description =
            "Shows elements as the user's selection in the app (switching to their page and layer), e.g. to point "
            "something out or before app actions like copy. All elements must be on one layer.";
    select.inputSchema = targetSchema({});
    select.tier = Tier::Ui;
    select.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"element_ids", "operation"});
        api::EditApi edit(ctrl);
        auto g = edit.resolve(idsFrom(args));
        edit.select(g);
        return ToolResult::structured(groupJson(g), "Selected.");
    };
    server.getRegistry().addTool(std::move(select));

    ToolSpec editTool;
    editTool.name = "elements_edit";
    editTool.title = "Edit elements";
    editTool.description =
            "Changes existing elements (one undo step): op=move (dx, dy), scale (factor or fx/fy; about 'origin', "
            "default the center; keep_line_width), rotate (degrees clockwise about 'origin'), restyle (color, "
            "width, fill_opacity 0..1 or -1 for none, line_style), reorder (to front/back), to_layer (layer name, "
            "\"#n\" or \"current\"). Target elements by ids or by the operation that created them.";
    editTool.inputSchema = targetSchema(
            {{"op", schema::enumeration("Edit", {"move", "scale", "rotate", "restyle", "reorder", "to_layer"})},
             {"dx", schema::number("move: x offset in points")},
             {"dy", schema::number("move: y offset in points")},
             {"factor", schema::number("scale: uniform factor")},
             {"fx", schema::number("scale: horizontal factor")},
             {"fy", schema::number("scale: vertical factor")},
             {"keep_line_width", schema::withDefault(schema::boolean("scale: keep stroke widths"), true)},
             {"degrees", schema::number("rotate: clockwise angle")},
             {"origin", schema::array("scale/rotate: center [x, y] (default: center of the elements)",
                                      schema::number("coordinate"))},
             {"color", schema::string("restyle: new color")},
             {"width", schema::number("restyle: new stroke width in points")},
             {"fill_opacity", schema::number("restyle: fill 0..1, or -1 to remove the fill")},
             {"line_style", schema::enumeration("restyle: dash pattern", {"plain", "dash", "dot", "dashdot"})},
             {"to", schema::enumeration("reorder: where", {"front", "back"})},
             {"layer", schema::string("to_layer: target layer")}});
    editTool.inputSchema["required"] = {"op"};
    editTool.tier = Tier::Draw;
    editTool.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"element_ids", "operation", "op", "dx", "dy", "factor", "fx", "fy", "keep_line_width",
                            "degrees", "origin", "color", "width", "fill_opacity", "line_style", "to", "layer"});
        const std::string op = args.choice("op", {"move", "scale", "rotate", "restyle", "reorder", "to_layer"}, "");
        api::EditApi edit(ctrl);
        auto g = edit.resolve(idsFrom(args));
        double ox = (g.x1 + g.x2) / 2, oy = (g.y1 + g.y2) / 2;
        if (auto o = args.numbersOpt("origin")) {
            if (o->size() != 2) {
                throw ToolError("origin must be [x, y]");
            }
            ox = (*o)[0];
            oy = (*o)[1];
        }
        json out = {{"op", op}};
        if (op == "move") {
            edit.move(g, args.number("dx", 0), args.number("dy", 0));
        } else if (op == "scale") {
            const double f = args.number("factor", 1);
            edit.scale(g, args.number("fx", f), args.number("fy", f), ox, oy, args.boolean("keep_line_width", true));
        } else if (op == "rotate") {
            edit.rotate(g, args.number("degrees"), ox, oy);
        } else if (op == "restyle") {
            api::Restyle st;
            if (args.has("color")) {
                st.color = parseColor(args.raw("color"));
            }
            if (args.has("width")) {
                st.width = args.number("width", 1, 0.01, 200);
            }
            if (args.has("fill_opacity")) {
                const double f = args.number("fill_opacity");
                st.fill = f < 0 ? -1 : static_cast<int>(std::lround(std::min(f, 1.0) * 255));
            }
            if (args.has("line_style")) {
                st.lineStyle = StrokeStyle::parseStyle(
                        args.choice("line_style", {"plain", "dash", "dot", "dashdot"}, "plain"));
            }
            out["changed"] = edit.restyle(g, st);
        } else if (op == "reorder") {
            edit.reorder(g, args.choice("to", {"front", "back"}, "front"));
        } else {
            out["layer"] = edit.toLayer(g, args.str("layer"));
        }
        // report the new geometry
        auto after = edit.resolve([&] {
            std::vector<std::string> ids;
            for (Element* e: g.all) {
                ids.push_back(api::ElementIds::get().idOf(e));
            }
            return ids;
        }());
        const json geometry = groupJson(after);  // named: items() of a temporary would dangle
        for (const auto& [k, v]: geometry.items()) {
            out[k] = v;
        }
        return ToolResult::structured(std::move(out));
    };
    server.getRegistry().addTool(std::move(editTool));

    ToolSpec del;
    del.name = "elements_delete";
    del.title = "Delete elements";
    del.description = "Deletes elements by ids or by the operation that created them (undoable: the ids come back "
                      "with undo).";
    del.inputSchema = targetSchema({});
    del.tier = Tier::Draw;
    del.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"element_ids", "operation"});
        api::EditApi edit(ctrl);
        auto g = edit.resolve(idsFrom(args));
        const size_t n = edit.remove(g);
        return ToolResult::structured({{"deleted", n}, {"page", g.pageIndex + 1}});
    };
    server.getRegistry().addTool(std::move(del));

    for (const bool isUndo: {true, false}) {
        ToolSpec t;
        t.name = isUndo ? "undo" : "redo";
        t.title = isUndo ? "Undo" : "Redo";
        t.description = isUndo ? "Undoes the last step(s) in the app's history (the same as the user pressing "
                                 "Ctrl+Z; includes the user's own steps - check 'history' first)." :
                                 "Redoes step(s) that were undone.";
        t.inputSchema = schema::object({{"steps", schema::withDefault(schema::integer("How many steps"), 1)}});
        t.tier = Tier::Draw;
        t.handler = [ctrl, isUndo](const json& j) {
            requireDocument(ctrl);
            Args args(j);
            args.rejectUnknown({"steps"});
            const auto steps = args.integer("steps", 1, 1, 100);
            UndoRedoHandler* h = ctrl->getUndoRedoHandler();
            json done = json::array();
            for (int64_t i = 0; i < steps; i++) {
                if (isUndo ? !h->canUndo() : !h->canRedo()) {
                    break;
                }
                done.push_back(isUndo ? h->undoDescription() : h->redoDescription());
                if (isUndo) {
                    UndoRedoController::undo(ctrl);
                } else {
                    UndoRedoController::redo(ctrl);
                }
            }
            return ToolResult::structured(
                    {{isUndo ? "undone" : "redone", done}, {"can_undo", h->canUndo()}, {"can_redo", h->canRedo()}});
        };
        server.getRegistry().addTool(std::move(t));
    }

    ToolSpec hist;
    hist.name = "history";
    hist.title = "Undo history";
    hist.description = "The most recent undo and redo steps (descriptions, most recent first).";
    hist.inputSchema = schema::object({{"max", schema::withDefault(schema::integer("Entries per list"), 20)}});
    hist.readOnly = true;
    hist.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"max"});
        const auto max = static_cast<size_t>(args.integer("max", 20, 1, 500));
        UndoRedoHandler* h = ctrl->getUndoRedoHandler();
        return ToolResult::structured({{"undo", h->history(false, max)}, {"redo", h->history(true, max)}});
    };
    server.getRegistry().addTool(std::move(hist));
}

}  // namespace xoj::mcp::tools
