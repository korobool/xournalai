// Tools: create_strokes, create_shapes

#include "api/DrawApi.h"      // for DrawApi
#include "api/Shapes.h"       // for shapes
#include "control/Control.h"  // for Control
#include "mcp/McpServer.h"
#include "mcp/Schema.h"

#include "DrawCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

namespace {

std::vector<double> numberList(const json& v, const std::string& what) {
    if (!v.is_array()) {
        throw ToolError(what + " must be an array of numbers");
    }
    std::vector<double> out;
    for (size_t i = 0; i < v.size(); i++) {
        out.push_back(Args::toNumber(v[i], what + "[" + std::to_string(i) + "]"));
    }
    return out;
}

Point pointFrom(const json& v, const std::string& what) {
    if (!v.is_array() || v.size() < 2) {
        throw ToolError(what + " must be [x, y]");
    }
    return Point(Args::toNumber(v[0], what), Args::toNumber(v[1], what));
}

std::vector<Point> pointList(const json& v, const std::string& what) {
    if (!v.is_array()) {
        throw ToolError(what + " must be an array of [x, y] points");
    }
    std::vector<Point> out;
    for (size_t i = 0; i < v.size(); i++) {
        out.push_back(pointFrom(v[i], what + "[" + std::to_string(i) + "]"));
    }
    return out;
}

double num(const json& o, const char* key, const std::string& where) {
    if (!o.contains(key)) {
        throw ToolError(where + ": \"" + key + "\" is required");
    }
    return Args::toNumber(o[key], where + "." + key);
}

}  // namespace

void registerDrawTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    // ---- create_strokes ----
    std::vector<schema::Property> strokeItem = {
            {"points", schema::array("Polyline [[x, y], ...] in page points",
                                     schema::array("[x, y]", schema::number("coordinate")))},
            {"pressure", schema::array("Optional normalized pressure 0..1 per point, mapped exactly like the user's "
                                       "stylus (pen only)",
                                       schema::number("pressure"))},
            {"widths",
             schema::array("Optional absolute width in points per point (pen only)", schema::number("width"))},
            {"times",
             schema::array("Optional timestamp in ms per point (speed-aware profiles)", schema::number("ms"))}};
    addStyleSchema(strokeItem);
    std::vector<schema::Property> props = {
            {"strokes", schema::array("Strokes to create", schema::object(strokeItem, {"points"},
                                                                          "One stroke; its "
                                                                          "style keys "
                                                                          "override the "
                                                                          "shared ones"))}};
    addStyleSchema(props);
    addTargetSchema(props);
    ToolSpec strokes;
    strokes.name = "create_strokes";
    strokes.title = "Create strokes";
    strokes.description =
            "Creates freehand strokes (pen or highlighter) directly in the document with stylus-like pressure: "
            "give per-point 'pressure' (0..1, mapped like the user's own stylus) or 'widths', or let a "
            "'profile' generate it (default: ink; brush, pencil, calligraphy, marker, constant, none). Points "
            "are resampled to stylus density. Content goes to the \"AI\" layer by default and the whole call is "
            "one undo step. Returns ids of the created strokes.";
    strokes.inputSchema = schema::object(std::move(props), {"strokes"});
    strokes.tier = Tier::Draw;
    strokes.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"strokes", "color", "width", "tool", "line_style", "fill_opacity", "profile",
                            "profile_options", "tremor", "spacing", "page", "new_page", "layer", "animate", "speed"});
        const StrokeStyleSpec shared = readStyle(j, defaultStyle(ctrl, true), "");
        const auto pressure = api::DrawApi(ctrl).pressureSettings();
        const json& list = args.raw("strokes");
        if (!list.is_array() || list.empty()) {
            throw ToolError("'strokes' must be a non-empty array");
        }
        if (list.size() > 2000) {
            throw ToolError("At most 2000 strokes per call");
        }
        std::vector<ElementPtr> elements;
        for (size_t i = 0; i < list.size(); i++) {
            const std::string where = "strokes[" + std::to_string(i) + "]";
            const json& item = list[i];
            if (!item.is_object()) {
                throw ToolError(where + " must be an object with \"points\"");
            }
            const StrokeStyleSpec style = readStyle(item, shared, where + ".");
            if (!item.contains("points")) {
                throw ToolError(where + ": \"points\" is required");
            }
            auto points = pointList(item["points"], where + ".points");
            std::vector<double> widths, pressures, times;
            if (item.contains("widths")) {
                widths = numberList(item["widths"], where + ".widths");
            }
            if (item.contains("pressure")) {
                pressures = numberList(item["pressure"], where + ".pressure");
            }
            if (item.contains("times")) {
                times = numberList(item["times"], where + ".times");
            }
            try {
                elements.push_back(buildStroke(std::move(points), style, pressure, &widths, &pressures, &times));
            } catch (const ToolError& e) {
                throw ToolError(where + ": " + e.what());
            }
        }
        insertAndRespond(*srv, args, std::move(elements), respond);
    };
    server.getRegistry().addTool(std::move(strokes));

    // ---- create_shapes ----
    std::vector<schema::Property> shapeItem = {
            {"type",
             schema::enumeration("Shape type", {"line", "arrow", "double_arrow", "rectangle", "ellipse", "circle",
                                                "polygon", "polyline", "bezier", "arc", "coordinate_system"})},
            {"from", schema::array("line/arrow: start [x, y]", schema::number("coordinate"))},
            {"to", schema::array("line/arrow: end [x, y]", schema::number("coordinate"))},
            {"x", schema::number("rectangle: left / coordinate_system: origin x")},
            {"y", schema::number("rectangle: top / coordinate_system: origin y")},
            {"w", schema::number("rectangle: width")},
            {"h", schema::number("rectangle: height")},
            {"corner_radius", schema::number("rectangle: rounded corner radius")},
            {"center", schema::array("ellipse/circle/arc: center [x, y]", schema::number("coordinate"))},
            {"r", schema::number("circle/arc: radius")},
            {"rx", schema::number("ellipse: horizontal radius")},
            {"ry", schema::number("ellipse: vertical radius")},
            {"points", schema::array("polygon/polyline: vertices; bezier: start, control1, control2, end, ...",
                                     schema::array("[x, y]", schema::number("coordinate")))},
            {"start_angle", schema::number("arc: start in degrees (0 = right, clockwise)")},
            {"end_angle", schema::number("arc: end in degrees")},
            {"head_size", schema::number("arrow/coordinate_system: arrowhead size in points")},
            {"x_length", schema::number("coordinate_system: x axis length")},
            {"y_length", schema::number("coordinate_system: y axis length (drawn upwards)")}};
    addStyleSchema(shapeItem);
    std::vector<schema::Property> shapeProps = {
            {"shapes", schema::array("Shapes to create", schema::object(shapeItem, {"type"}, "One shape"))},
            {"hand_drawn", schema::withDefault(schema::boolean("Hand-drawn look: ink pressure and slight tremor "
                                                               "instead of clean constant lines"),
                                               false)}};
    addStyleSchema(shapeProps);
    addTargetSchema(shapeProps);
    ToolSpec shapesTool;
    shapesTool.name = "create_shapes";
    shapesTool.title = "Create shapes";
    shapesTool.description =
            "Creates geometric shapes as editable strokes: line, arrow, double_arrow, rectangle (corner_radius), "
            "ellipse, circle, polygon, polyline, bezier, arc, coordinate_system. Closed shapes can be filled "
            "(fill_opacity). Clean lines by default; hand_drawn=true gives a sketched look. Goes to the \"AI\" "
            "layer by default; one undo step.";
    shapesTool.inputSchema = schema::object(std::move(shapeProps), {"shapes"});
    shapesTool.tier = Tier::Draw;
    shapesTool.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"shapes", "hand_drawn", "color", "width", "tool", "line_style", "fill_opacity", "profile",
                            "profile_options", "tremor", "spacing", "page", "new_page", "layer", "animate", "speed"});
        const bool hand = args.boolean("hand_drawn", false);
        StrokeStyleSpec base = defaultStyle(ctrl, hand);
        if (hand) {
            base.tremor = 0.35;
        }
        const StrokeStyleSpec shared = readStyle(j, base, "");
        const auto pressure = api::DrawApi(ctrl).pressureSettings();
        const json& list = args.raw("shapes");
        if (!list.is_array() || list.empty()) {
            throw ToolError("'shapes' must be a non-empty array");
        }
        std::vector<ElementPtr> elements;
        for (size_t i = 0; i < list.size(); i++) {
            const std::string where = "shapes[" + std::to_string(i) + "]";
            const json& o = list[i];
            if (!o.is_object() || !o.contains("type")) {
                throw ToolError(where + " must be an object with a \"type\"");
            }
            StrokeStyleSpec style = readStyle(o, shared, where + ".");
            const double spacing = style.spacing > 0 ? style.spacing : 1.0;
            const std::string type = o["type"].get<std::string>();
            const double head = o.contains("head_size") ? num(o, "head_size", where) : 6 + 2.5 * style.width;
            api::shapes::Shape shape;
            try {
                if (type == "line" || type == "arrow" || type == "double_arrow") {
                    const Point a = pointFrom(o.value("from", json()), where + ".from");
                    const Point b = pointFrom(o.value("to", json()), where + ".to");
                    shape = type == "line" ?
                                    api::shapes::line(a.x, a.y, b.x, b.y, spacing) :
                                    api::shapes::arrow(a.x, a.y, b.x, b.y, head, type == "double_arrow", spacing);
                } else if (type == "rectangle") {
                    shape = api::shapes::rectangle(
                            num(o, "x", where), num(o, "y", where), num(o, "w", where), num(o, "h", where),
                            o.contains("corner_radius") ? num(o, "corner_radius", where) : 0, spacing);
                } else if (type == "ellipse" || type == "circle") {
                    const Point c = pointFrom(o.value("center", json()), where + ".center");
                    const double rx = type == "circle" ? num(o, "r", where) : num(o, "rx", where);
                    const double ry = type == "circle" ? rx : num(o, "ry", where);
                    shape = api::shapes::ellipse(c.x, c.y, rx, ry, spacing);
                } else if (type == "polygon" || type == "polyline") {
                    shape = api::shapes::polygon(pointList(o.value("points", json()), where + ".points"),
                                                 type == "polygon", spacing);
                } else if (type == "bezier") {
                    shape = api::shapes::bezier(pointList(o.value("points", json()), where + ".points"), spacing);
                } else if (type == "arc") {
                    const Point c = pointFrom(o.value("center", json()), where + ".center");
                    shape = api::shapes::arc(c.x, c.y, num(o, "r", where), num(o, "start_angle", where),
                                             num(o, "end_angle", where), spacing);
                } else if (type == "coordinate_system") {
                    shape = api::shapes::coordinateSystem(num(o, "x", where), num(o, "y", where),
                                                          num(o, "x_length", where), num(o, "y_length", where), head,
                                                          spacing);
                } else {
                    throw ToolError(where + ": unknown shape type '" + type + "'");
                }
            } catch (const std::invalid_argument& e) {
                throw ToolError(where + ": " + e.what());
            }
            if (!shape.closed) {
                style.fill.reset();
            }
            style.spacing = 0;  // generators already produce stylus-density points
            for (auto& poly: shape.strokes) {
                elements.push_back(buildStroke(std::move(poly), style, pressure));
            }
        }
        insertAndRespond(*srv, args, std::move(elements), respond);
    };
    server.getRegistry().addTool(std::move(shapesTool));
}

}  // namespace xoj::mcp::tools
