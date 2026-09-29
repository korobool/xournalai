#include "DrawCommon.h"

#include <cmath>         // for lround
#include <shared_mutex>  // for shared_lock

#include "api/DocumentApi.h"      // for currentPageIndex
#include "api/Drafts.h"           // for Drafts
#include "api/ElementIds.h"       // for ElementIds
#include "api/Geometry.h"         // for roundTo
#include "api/Placement.h"        // for appendPage
#include "control/Control.h"      // for Control
#include "control/ToolHandler.h"  // for ToolHandler
#include "mcp/ElementJson.h"
#include "mcp/McpServer.h"
#include "mcp/McpUi.h"
#include "model/Document.h"     // for Document
#include "model/Layer.h"        // for Layer
#include "model/StrokeStyle.h"  // for parseStyle
#include "model/XojPage.h"      // for XojPage

#include "ToolUtil.h"

namespace xoj::mcp::tools {

StrokeStyleSpec defaultStyle(Control* ctrl, bool handDrawn) {
    StrokeStyleSpec s;
    if (ctrl && ctrl->getToolHandler()) {
        Color c = ctrl->getToolHandler()->getTool(TOOL_PEN).getColor();
        c.alpha = 0xff;
        s.color = c;
    }
    if (handDrawn) {
        s.profile = api::PressureProfile{};
    }
    return s;
}

StrokeStyleSpec readStyle(const json& obj, StrokeStyleSpec s, const std::string& where) {
    if (!obj.is_object()) {
        return s;
    }
    auto num = [&](const char* k) { return Args::toNumber(obj[k], where + k); };
    if (obj.contains("color")) {
        s.color = parseColor(obj["color"], where + "color");
        if (s.tool == StrokeTool::HIGHLIGHTER) {
            s.color.alpha = 0xff;
        }
    }
    if (obj.contains("width")) {
        s.width = num("width");
        if (s.width <= 0 || s.width > 200) {
            throw ToolError(where + "width must be between 0 and 200 points");
        }
    }
    if (obj.contains("tool")) {
        const std::string t = obj["tool"].get<std::string>();
        if (t == "highlighter") {
            s.tool = StrokeTool::HIGHLIGHTER;
            if (!obj.contains("width")) {
                s.width = 8.5;
            }
            s.profile.reset();
        } else if (t == "pen") {
            s.tool = StrokeTool::PEN;
        } else {
            throw ToolError(where + "tool must be pen or highlighter");
        }
    }
    if (obj.contains("line_style")) {
        s.lineStyle = obj["line_style"].get<std::string>();
        if (s.lineStyle != "plain" && s.lineStyle != "dash" && s.lineStyle != "dot" && s.lineStyle != "dashdot") {
            throw ToolError(where + "line_style must be plain, dash, dot or dashdot");
        }
    }
    if (obj.contains("fill_opacity")) {
        if (obj["fill_opacity"].is_null()) {
            s.fill.reset();
        } else {
            const double f = num("fill_opacity");
            if (f < 0 || f > 1) {
                throw ToolError(where + "fill_opacity must be between 0 and 1");
            }
            s.fill = static_cast<int>(std::lround(f * 255));
        }
    }
    if (obj.contains("profile")) {
        const std::string p = obj["profile"].get<std::string>();
        if (p == "none") {
            s.profile.reset();
        } else {
            api::PressureProfile pr;
            pr.preset = p;
            s.profile = pr;
        }
    }
    if (obj.contains("profile_options")) {
        const json& o = obj["profile_options"];
        if (!o.is_object()) {
            throw ToolError(where + "profile_options must be an object");
        }
        if (!s.profile) {
            s.profile = api::PressureProfile{};
        }
        auto opt = [&](const char* k) -> std::optional<double> {
            return o.contains(k) ? std::optional<double>(Args::toNumber(o[k], where + "profile_options." + k)) :
                                   std::nullopt;
        };
        if (auto v = opt("base")) {
            s.profile->base = v;
        }
        if (auto v = opt("min")) {
            s.profile->min = v;
        }
        if (auto v = opt("taper_in")) {
            s.profile->taperIn = v;
        }
        if (auto v = opt("taper_out")) {
            s.profile->taperOut = v;
        }
        if (auto v = opt("variation")) {
            s.profile->variation = v;
        }
        if (auto v = opt("nib_angle")) {
            s.profile->nibAngle = *v;
        }
        if (auto v = opt("seed")) {
            s.profile->seed = static_cast<unsigned>(*v);
        }
        if (o.contains("speed_aware")) {
            s.profile->speedAware = Args::toBool(o["speed_aware"], where + "profile_options.speed_aware");
        }
    }
    if (s.profile) {
        try {
            s.profile->validate();
        } catch (const std::invalid_argument& e) {
            throw ToolError(where + e.what());
        }
    }
    if (obj.contains("tremor")) {
        s.tremor = std::clamp(num("tremor"), 0.0, 5.0);
    }
    if (obj.contains("spacing")) {
        s.spacing = std::clamp(num("spacing"), 0.0, 20.0);
    }
    return s;
}

void addStyleSchema(std::vector<schema::Property>& props) {
    props.push_back({"color", schema::string("Stroke color: \"#rrggbb\", \"#rrggbbaa\", \"rgb(r,g,b)\" or a name; "
                                             "default: the user's pen color")});
    props.push_back({"width", schema::number("Base stroke width in points (pen default 1.41, highlighter 8.5)")});
    props.push_back({"tool", schema::enumeration("Stroke tool", {"pen", "highlighter"})});
    props.push_back({"line_style", schema::enumeration("Dash pattern", {"plain", "dash", "dot", "dashdot"})});
    props.push_back({"fill_opacity", schema::number("Fill closed shapes with the stroke color, 0..1")});
    props.push_back(
            {"profile", schema::enumeration("Pressure profile (stylus-like width variation); none = constant",
                                            {"none", "constant", "ink", "brush", "pencil", "calligraphy", "marker"})});
    props.push_back({"profile_options",
                     schema::object({{"base", schema::number("Main pressure 0..1")},
                                     {"min", schema::number("Pressure at the ends 0..1")},
                                     {"taper_in", schema::number("Start taper length in points")},
                                     {"taper_out", schema::number("End taper length in points")},
                                     {"variation", schema::number("Random variation 0..1")},
                                     {"nib_angle", schema::number("Calligraphy nib angle in degrees")},
                                     {"seed", schema::integer("Random seed")},
                                     {"speed_aware", schema::boolean("Faster segments get thinner (needs times)")}},
                                    {}, "Fine-tuning of the pressure profile")});
    props.push_back(
            {"tremor", schema::number("Hand tremor amplitude in points (0..5), e.g. 0.3 for a hand-drawn look")});
    props.push_back(
            {"spacing", schema::number("Point spacing in points for stylus-like density (default 1, 0 = off)")});
}

void addTargetSchema(std::vector<schema::Property>& props) {
    props.push_back({"page", schema::integer("Page (1-based); default: current page")});
    props.push_back({"new_page", schema::boolean("Append a new page and draw there (ignores 'page')")});
    props.push_back(
            {"layer",
             schema::string("Target layer: \"current\" (the user's selected layer), \"AI\" (a separate layer "
                            "created on top, which the user can accept, hide or clear), a layer name or \"#<n>\". "
                            "Default: the user's setting (app_status default_layer)")});
    props.push_back({"animate", schema::boolean("Draw progressively so the user sees it being drawn (default from "
                                                "settings)")});
    props.push_back({"speed", schema::number("Animation speed factor (1 = about hand speed)")});
}

std::unique_ptr<Stroke> buildStroke(std::vector<Point> points, const StrokeStyleSpec& style,
                                    const api::PressureSettings& pressure, const std::vector<double>* widths,
                                    const std::vector<double>* pressures, const std::vector<double>* times) {
    if (points.size() < 2) {
        throw ToolError("A stroke needs at least 2 points");
    }
    auto s = std::make_unique<Stroke>();
    s->setToolType(style.tool);
    s->setColor(style.color);
    s->setWidth(style.width);
    s->setLineStyle(StrokeStyle::parseStyle(style.lineStyle));
    if (style.fill) {
        s->setFill(*style.fill);
    }
    const bool pen = style.tool == StrokeTool::PEN;
    const bool explicitValues = (widths && !widths->empty()) || (pressures && !pressures->empty());
    if (explicitValues) {
        const auto& v = widths && !widths->empty() ? *widths : *pressures;
        if (v.size() != points.size()) {
            throw ToolError("widths/pressure must have one value per point (" + std::to_string(points.size()) + ")");
        }
        for (size_t i = 0; i < points.size(); i++) {
            if (!pen) {
                points[i].z = Point::NO_PRESSURE;
            } else if (widths && !widths->empty()) {
                if (v[i] <= 0) {
                    throw ToolError("widths must be positive");
                }
                points[i].z = v[i];
            } else {
                points[i].z = api::hardwareWidth(std::clamp(v[i], 0.0, 1.0), style.width, pressure);
            }
        }
        if (style.tremor > 0) {
            api::addTremor(points, style.tremor, style.profile ? style.profile->seed : 1);
        }
    } else {
        std::vector<double> t;
        const bool haveTimes = times && times->size() == points.size();
        if (style.spacing > 0 && !haveTimes) {
            points = api::resample(points, style.spacing);
        }
        if (style.tremor > 0) {
            api::addTremor(points, style.tremor, style.profile ? style.profile->seed : 1);
        }
        if (pen && style.profile) {
            auto p = api::profilePressures(points, *style.profile, haveTimes ? times : nullptr);
            api::applyPressures(points, p, style.width, pressure);
        } else {
            for (auto& p: points) {
                p.z = Point::NO_PRESSURE;
            }
        }
    }
    s->setPointVector(std::move(points));
    return s;
}

namespace {
struct UserSample {
    std::optional<api::LearnedStyle> style;
    std::optional<Color> color;
};

UserSample sampleUserStrokes(Control* ctrl) {
    std::vector<std::pair<std::vector<Point>, double>> strokes;
    std::optional<Color> color;
    Document* doc = ctrl->getDocument();
    std::shared_lock lock(*doc);
    const size_t current = api::currentPageIndex(ctrl);
    std::vector<size_t> order = {current};
    for (size_t i = 0; i < doc->getPageCount(); i++) {
        if (i != current) {
            order.push_back(i);
        }
    }
    for (size_t pi: order) {
        PageRef page = doc->getPage(pi);
        for (const Layer* l: page->getLayersView()) {
            for (const Element* e: l->getElementsView()) {
                if (e->getType() != ELEMENT_STROKE || api::ElementIds::get().origin(e)) {
                    continue;  // only the user's own strokes
                }
                const auto* st = static_cast<const Stroke*>(e);
                if (st->getToolType() != StrokeTool::PEN || !st->hasPressure()) {
                    continue;
                }
                strokes.emplace_back(st->getPointVector(), st->getWidth());
                color = st->getColor();
            }
        }
        if (strokes.size() >= 40) {
            break;
        }
    }
    if (strokes.size() > 60) {
        strokes.erase(strokes.begin(), strokes.end() - 60);  // the most recent ones
    }
    return {api::learnStyle(strokes), color};
}
}  // namespace

json learnedUserStyle(Control* ctrl) {
    auto sample = sampleUserStrokes(ctrl);
    if (!sample.style) {
        throw ToolError("No pen strokes with pressure from the user yet: draw a few strokes with the stylus, or "
                        "use another profile");
    }
    const auto& p = sample.style->profile;
    json out = {{"strokes_analyzed", sample.style->strokes},
                {"width", api::roundTo(sample.style->width)},
                {"profile", "ink"},
                {"profile_options",
                 {{"base", api::roundTo(*p.base, 3)},
                  {"min", api::roundTo(*p.min, 3)},
                  {"taper_in", api::roundTo(*p.taperIn)},
                  {"taper_out", api::roundTo(*p.taperOut)},
                  {"variation", api::roundTo(*p.variation, 3)}}},
                {"pressure_curve", sample.style->sample}};
    if (sample.color) {
        out["color"] = colorToHex(*sample.color);
    }
    return out;
}

json resolveMatchUser(Control* ctrl, const json& args) {
    if (!args.is_object() || args.value("profile", "") != "match_user") {
        return args;
    }
    const json learned = learnedUserStyle(ctrl);
    json out = args;
    out["profile"] = "ink";
    out["profile_options"] = learned["profile_options"];
    if (!out.contains("width")) {
        out["width"] = learned["width"];
    }
    if (!out.contains("color") && learned.contains("color")) {
        out["color"] = learned["color"];
    }
    return out;
}

json drawResultJson(const api::DrawResult& r) {
    json created = json::array();
    for (const Element* e: r.elements) {
        const auto& bb = e->getBoundingBox();
        created.push_back({{"id", api::ElementIds::get().idOf(e)},
                           {"type", elementTypeName(e)},
                           {"bbox", bboxJson(bb.x, bb.y, bb.width, bb.height)}});
    }
    return {{"operation", r.operation},
            {"page", r.page + 1},
            {"layer", r.layer},
            {"layer_name", r.layerName},
            {"layer_created", r.layerCreated},
            {"created", std::move(created)},
            {"undo", "One undo step (the user can press Ctrl+Z); remove with elements_delete(operation=\"" +
                             r.operation + "\")"}};
}

std::string drawingLayer(McpServer& server, const Args& args, json* note) {
    const std::string preferred = server.getConfig().defaultLayer;
    const std::string asked = args.str("layer", "");
    if (asked.empty() || asked == preferred || server.getConfig().agentsChooseLayer) {
        return asked.empty() ? preferred : asked;
    }
    for (const auto& [id, draft]: api::Drafts::get().all()) {
        if (draft.layerName == asked) {
            return asked;  // an agent's own hidden draft layer
        }
    }
    if (note) {
        *note = "The user's settings put AI drawings on layer \"" + preferred + "\"; \"" + asked + "\" was ignored.";
    }
    return preferred;
}

void insertAndRespond(McpServer& server, const Args& args, std::vector<ElementPtr> elements, Responder respond,
                      json extra) {
    Control* ctrl = server.getControl();
    api::DrawTarget target;
    if (args.boolean("new_page", false)) {
        target.page = api::appendPage(ctrl);
    } else {
        target.page = resolvePageIndex(ctrl, args);
    }
    json layerNote;
    target.layer = drawingLayer(server, args, &layerNote);
    if (!layerNote.is_null()) {
        extra["layer_note"] = layerNote;
    }
    api::AnimationOptions anim;
    anim.enabled = args.boolean("animate", server.getConfig().animate);
    anim.speed = args.number("speed", 1.0, 0.05, 100);
    auto alive = server.aliveToken();
    McpServer* srv = &server;
    api::DrawApi(ctrl).insert(target, std::move(elements), anim,
                              [respond, extra, alive, srv](const api::DrawResult& r) {
                                  if (*alive && srv->getUi() && !r.elements.empty()) {
                                      xoj::util::Rectangle<double> area = r.elements.front()->getBoundingBox();
                                      for (const Element* e: r.elements) {
                                          area.unite(e->getBoundingBox());
                                      }
                                      srv->getUi()->flash(r.page, area);
                                  }
                                  json out = drawResultJson(r);
                                  for (const auto& [k, v]: extra.items()) {
                                      out[k] = v;
                                  }
                                  respond(ToolResult::structured(std::move(out)));
                              });
}

}  // namespace xoj::mcp::tools
