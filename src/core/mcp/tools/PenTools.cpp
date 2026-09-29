// Tool: pen_draw

#include <map>  // for map

#include "api/ElementIds.h"   // for ElementIds
#include "api/PenEngine.h"    // for runPen
#include "api/Pressure.h"     // for profilePressures, resample
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/Schema.h"

#include "DrawCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {
struct PenTool {
    ToolType type;
    std::optional<DrawingType> drawing;
};

const std::map<std::string, PenTool>& penTools() {
    // Shapes are drawing modes of the pen in Xournal++ (drag from start to end corner)
    static const std::map<std::string, PenTool> t = {{"pen", {TOOL_PEN, DRAWING_TYPE_DEFAULT}},
                                                     {"highlighter", {TOOL_HIGHLIGHTER, DRAWING_TYPE_DEFAULT}},
                                                     {"eraser", {TOOL_ERASER, std::nullopt}},
                                                     {"select_lasso", {TOOL_SELECT_REGION, std::nullopt}},
                                                     {"select_rect", {TOOL_SELECT_RECT, std::nullopt}},
                                                     {"recognizer", {TOOL_PEN, DRAWING_TYPE_SHAPE_RECOGNIZER}},
                                                     {"shape_line", {TOOL_PEN, DRAWING_TYPE_LINE}},
                                                     {"shape_rect", {TOOL_PEN, DRAWING_TYPE_RECTANGLE}},
                                                     {"shape_ellipse", {TOOL_PEN, DRAWING_TYPE_ELLIPSE}},
                                                     {"shape_arrow", {TOOL_PEN, DRAWING_TYPE_ARROW}},
                                                     {"shape_double_arrow", {TOOL_PEN, DRAWING_TYPE_DOUBLE_ARROW}},
                                                     {"shape_axes", {TOOL_PEN, DRAWING_TYPE_COORDINATE_SYSTEM}}};
    return t;
}

const std::map<std::string, ToolSize>& sizes() {
    static const std::map<std::string, ToolSize> s = {{"very_fine", TOOL_SIZE_VERY_FINE},
                                                      {"fine", TOOL_SIZE_FINE},
                                                      {"medium", TOOL_SIZE_MEDIUM},
                                                      {"thick", TOOL_SIZE_THICK},
                                                      {"very_thick", TOOL_SIZE_VERY_THICK}};
    return s;
}
}  // namespace

void registerPenTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;
    std::vector<std::string> toolNames, sizeNames;
    for (const auto& [n, t]: penTools()) {
        toolNames.push_back(n);
    }
    for (const auto& [n, s]: sizes()) {
        sizeNames.push_back(n);
    }

    std::vector<schema::Property> strokeItem = {
            {"points", schema::array("Pen trajectory [[x, y], ...] in page points",
                                     schema::array("[x, y]", schema::number("coordinate")))},
            {"pressure", schema::array("Pen pressure 0..1 per point (default: from 'profile')", schema::number("p"))},
            {"times", schema::array("Milliseconds since the stroke start per point (default: hand speed)",
                                    schema::number("ms"))}};
    std::vector<schema::Property> props = {
            {"tool", schema::withDefault(schema::enumeration("Tool used by the pen: drawing, erasing, selecting or "
                                                             "the app's shape tools (drag from corner to corner)",
                                                             toolNames),
                                         "pen")},
            {"strokes", schema::array("Pen strokes (pen down, move, pen up)",
                                      schema::object(strokeItem, {"points"}, "One pen stroke"))},
            {"color", schema::string("Tool color for this drawing (the user's setting is restored afterwards)")},
            {"size", schema::enumeration("Tool size", sizeNames)},
            {"profile",
             schema::enumeration("Pressure profile when no pressure is given (default ink)",
                                 {"constant", "ink", "brush", "pencil", "calligraphy", "marker", "match_user"})},
            {"page", schema::integer("Page (1-based); default: current page")},
            {"layer",
             schema::string("Layer to act on: drawing tools default to the user's setting (app_status "
                            "default_layer), eraser and selection to \"current\"; or \"AI\", a name, \"#<n>\"")},
            {"speed", schema::withDefault(schema::number("1 = hand speed (the user watches it being drawn), 3 = "
                                                         "faster, 0 = instant"),
                                          1)}};
    ToolSpec pen;
    pen.name = "pen_draw";
    pen.title = "Draw with the pen";
    pen.description =
            "Moves a simulated stylus through the app's real input pipeline, exactly like the user's own pen: the "
            "chosen tool with the user's pressure settings, stabilizer and shape recognizer. Can draw (pen, "
            "highlighter), ERASE (eraser swipes), SELECT (select_lasso: a closed loop around items; select_rect: "
            "drag a diagonal) and use the shape tools. Strokes are visibly drawn at hand speed unless speed=0. "
            "Each stroke is its own undo step, like the user's. The user's tool is restored afterwards (a "
            "selection stays active). Returns created ids and erased/selected counts. For precise or bulk content "
            "prefer create_strokes/create_shapes.";
    pen.inputSchema = schema::object(std::move(props), {"strokes"});
    pen.tier = Tier::Draw;
    pen.asyncHandler = [ctrl, srv](const json& jIn, Responder respond) {
        const json j = resolveMatchUser(ctrl, jIn);
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown(
                {"tool", "strokes", "color", "size", "profile", "profile_options", "width", "page", "layer", "speed"});
        api::PenJob job;
        const std::string toolName = args.str("tool", "pen");
        auto t = penTools().find(toolName);
        if (t == penTools().end()) {
            throw ToolError("Unknown pen tool '" + toolName + "'");
        }
        job.tool = t->second.type;
        job.drawingType = t->second.drawing;
        job.page = resolvePageIndex(ctrl, args);
        if (args.has("color")) {
            job.color = parseColor(args.raw("color"));
        }
        if (args.has("size")) {
            auto s = sizes().find(args.str("size"));
            if (s == sizes().end()) {
                throw ToolError("size must be very_fine, fine, medium, thick or very_thick");
            }
            job.size = s->second;
        }
        const bool acting = job.tool == TOOL_ERASER || job.tool == TOOL_SELECT_REGION || job.tool == TOOL_SELECT_RECT;
        job.layer = acting ? args.str("layer", "current") : drawingLayer(*srv, args, nullptr);
        job.speed = args.number("speed", 1, 0, 50);
        api::PressureProfile profile;
        profile.preset = args.str("profile", "ink");
        if (args.has("profile_options")) {
            const json& o = args.raw("profile_options");
            auto opt = [&](const char* k) {
                return o.contains(k) ? std::optional<double>(Args::toNumber(o[k], k)) : std::nullopt;
            };
            profile.base = opt("base");
            profile.min = opt("min");
            profile.taperIn = opt("taper_in");
            profile.taperOut = opt("taper_out");
            profile.variation = opt("variation");
        }
        try {
            profile.validate();
        } catch (const std::invalid_argument& e) {
            throw ToolError(e.what());
        }

        const json& list = args.raw("strokes");
        if (!list.is_array() || list.empty()) {
            throw ToolError("'strokes' must be a non-empty array");
        }
        for (size_t i = 0; i < list.size(); i++) {
            const std::string where = "strokes[" + std::to_string(i) + "]";
            const json& item = list[i];
            if (!item.is_object() || !item.contains("points") || !item["points"].is_array() ||
                item["points"].size() < 2) {
                throw ToolError(where + " needs \"points\" with at least 2 [x, y] points");
            }
            std::vector<Point> pts;
            for (const auto& p: item["points"]) {
                if (!p.is_array() || p.size() < 2) {
                    throw ToolError(where + ".points: each point must be [x, y]");
                }
                pts.emplace_back(Args::toNumber(p[0], where), Args::toNumber(p[1], where));
            }
            std::vector<double> pressure, times;
            if (item.contains("pressure")) {
                for (const auto& v: item["pressure"]) {
                    pressure.push_back(std::clamp(Args::toNumber(v, where + ".pressure"), 0.0, 1.0));
                }
                if (pressure.size() != pts.size()) {
                    throw ToolError(where + ".pressure needs one value per point");
                }
            }
            if (item.contains("times")) {
                for (const auto& v: item["times"]) {
                    times.push_back(Args::toNumber(v, where + ".times"));
                }
                if (times.size() != pts.size()) {
                    throw ToolError(where + ".times needs one value per point");
                }
            }
            if (pressure.empty() && times.empty()) {
                pts = api::resample(pts, 1.0);  // stylus-like event density
            }
            if (pressure.empty()) {
                pressure = api::profilePressures(pts, profile, times.empty() ? nullptr : &times);
            }
            std::vector<api::PenSample> samples;
            for (size_t k = 0; k < pts.size(); k++) {
                samples.push_back({pts[k].x, pts[k].y, pressure[k], times.empty() ? -1 : times[k]});
            }
            job.strokes.push_back(std::move(samples));
        }

        api::runPen(ctrl, std::move(job), [respond, toolName](api::PenResult r) {
            if (!r.error.empty()) {
                respond(ToolResult::error(r.error));
                return;
            }
            json created = json::array();
            for (const Element* e: r.created) {
                const auto& bb = e->getBoundingBox();
                created.push_back({{"id", api::ElementIds::get().idOf(e)},
                                   {"type", elementTypeName(e)},
                                   {"bbox", bboxJson(bb.x, bb.y, bb.width, bb.height)}});
            }
            json out = {{"tool", toolName},          {"layer", r.layer},
                        {"layer_name", r.layerName}, {"created", std::move(created)},
                        {"erased", r.erased},        {"selected", r.selected}};
            if (r.selected > 0) {
                out["note"] = "The selection is active in the app (the selection tool stays selected)";
            }
            respond(ToolResult::structured(std::move(out)));
        });
    };
    server.getRegistry().addTool(std::move(pen));
}

}  // namespace xoj::mcp::tools
