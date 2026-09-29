// Tool: create_from_svg

#include <fstream>   // for ifstream
#include <iterator>  // for istreambuf_iterator

#include "api/DrawApi.h"      // for DrawApi
#include "api/SvgImport.h"    // for convert
#include "control/Control.h"  // for Control
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"
#include "model/Font.h"  // for XojFont
#include "model/Text.h"  // for Text

#include "DrawCommon.h"
#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerSvgTools(McpServer& server) {
    Control* ctrl = server.getControl();
    McpServer* srv = &server;

    std::vector<schema::Property> props = {
            {"svg", schema::string("SVG document text (<svg ...>...</svg>)")},
            {"path", schema::string("Or: an .svg file to read")},
            {"target",
             schema::array("Fit the drawing into this page area [x, y, width, height]", schema::number("coordinate"))},
            {"fit", schema::withDefault(schema::enumeration("How to fit into target", {"contain", "cover", "none"}),
                                        "contain")},
            {"x", schema::number("Without target: page x of the SVG origin (default 0)")},
            {"y", schema::number("Without target: page y of the SVG origin (default 0)")},
            {"scale", schema::number("Without target: SVG units per page point (default 1)")},
            {"profile", schema::enumeration(
                                "Optional pressure profile for a hand-drawn look (default none)",
                                {"none", "constant", "ink", "brush", "pencil", "calligraphy", "marker", "match_user"})},
            {"tremor", schema::number("Hand tremor amplitude in points (default 0)")},
            {"color", schema::string("Draw everything in this color instead of the SVG colors")}};
    addTargetSchema(props);
    ToolSpec svg;
    svg.name = "create_from_svg";
    svg.title = "Draw an SVG";
    svg.description =
            "Converts an SVG drawing into editable strokes (paths, rect, circle, ellipse, line, polyline, "
            "polygon, groups and transforms; stroke/fill colors, opacity, dashes) and <text> into text boxes, "
            "placed on the page (1 SVG unit = 1 point, or fitted into 'target'). Best way to draw complex "
            "illustrations. Optional pressure 'profile' and 'tremor' make it look hand-drawn. Gradients use their "
            "first color. One undo step, the default layer (the user's current layer unless they chose e.g. \"AI\"; "
            "see app_status); returns ids and warnings.";
    svg.inputSchema = schema::object(std::move(props));
    svg.tier = Tier::Draw;
    svg.asyncHandler = [ctrl, srv](const json& jIn, Responder respond) {
        // match_user: only the user's pressure profile; the SVG keeps its own colors and stroke widths
        json j = resolveMatchUser(ctrl, jIn);
        for (const char* k: {"width", "color"}) {
            if (!jIn.contains(k)) {
                j.erase(k);
            }
        }
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"svg", "path", "target", "fit", "x", "y", "scale", "profile", "profile_options", "tremor",
                            "color", "page", "new_page", "layer", "animate", "speed"});
        std::string text;
        if (args.has("svg")) {
            text = args.str("svg");
        } else if (args.has("path")) {
            srv->requireTier(Tier::Files, "reading an SVG file");
            std::ifstream in(pathFromUtf8(args.str("path")), std::ios::binary);
            if (!in) {
                throw ToolError("Cannot read " + args.str("path"));
            }
            text.assign(std::istreambuf_iterator<char>(in), {});
        } else {
            throw ToolError("Give 'svg' (text) or 'path'");
        }
        if (text.size() > 5'000'000) {
            throw ToolError("SVG too large (max 5 MB)");
        }
        api::svg::Placement placement;
        placement.target = parseRegion(args, "target");
        placement.fit = args.choice("fit", {"contain", "cover", "none"}, "contain");
        placement.x = args.number("x", 0);
        placement.y = args.number("y", 0);
        placement.scale = args.number("scale", 1, 0.001, 1000);
        const api::svg::SvgResult r = api::svg::convert(text, placement);
        if (r.strokes.size() > 20000) {
            throw ToolError("The SVG produces too many strokes (" + std::to_string(r.strokes.size()) + ")");
        }

        StrokeStyleSpec base = defaultStyle(ctrl, false);
        base = readStyle(j, base, "");  // profile, tremor, color
        const auto pressure = api::DrawApi(ctrl).pressureSettings();
        std::vector<ElementPtr> elements;
        for (const auto& s: r.strokes) {
            StrokeStyleSpec style = base;
            style.color = args.has("color") ? base.color : s.color;
            style.width = s.width;
            style.fill = s.fill;
            style.lineStyle = s.dashed ? "dash" : "plain";
            if (s.fill && !args.has("profile")) {
                style.profile.reset();  // filled areas keep a clean outline
            }
            style.spacing = 0;  // already flattened at stylus density
            elements.push_back(buildStroke(s.points, style, pressure));
        }
        for (const auto& t: r.texts) {
            auto el = std::make_unique<Text>();
            el->setText(t.text);
            el->setFont(XojFont(t.font, t.size));
            el->setColor(args.has("color") ? base.color : t.color);
            el->setTransformation(xoj::util::Matrix::TRANSLATION(t.x, t.y));
            elements.push_back(std::move(el));
        }
        json extra = {{"bounds", bboxJson(r.bounds.x, r.bounds.y, r.bounds.width, r.bounds.height)},
                      {"strokes", r.strokes.size()},
                      {"texts", r.texts.size()}};
        if (!r.warnings.empty()) {
            extra["warnings"] = r.warnings;
        }
        insertAndRespond(*srv, args, std::move(elements), respond, extra);
    };
    server.getRegistry().addTool(std::move(svg));
}

}  // namespace xoj::mcp::tools
