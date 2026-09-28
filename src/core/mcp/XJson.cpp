#include "XJson.h"

#include <cmath>  // for isfinite

#include "model/Font.h"         // for XojFont
#include "model/Image.h"        // for Image
#include "model/Link.h"         // for Link
#include "model/Stroke.h"       // for Stroke
#include "model/StrokeStyle.h"  // for formatStyle, parseStyle
#include "model/TexImage.h"     // for TexImage
#include "model/Text.h"         // for Text
#include "tools/ToolUtil.h"     // for colorToHex, parseColor

#include "Args.h"
#include "ElementJson.h"  // for bboxJson, elementTypeName
#include "Media.h"        // for base64Encode, base64Decode
#include "Registry.h"     // for ToolError

namespace xoj::mcp::xjson {

namespace {

json transformJson(const xoj::util::Matrix& m) {
    return {{"xx", m.xx}, {"yx", m.yx}, {"xy", m.xy}, {"yy", m.yy}, {"x", m.shift.x}, {"y", m.shift.y}};
}

const char* alignName(TextAlignment a) {
    switch (static_cast<TextAlignment::Value>(a)) {
        case TextAlignment::CENTER:
            return "center";
        case TextAlignment::RIGHT:
            return "right";
        default:
            return "left";
    }
}

TextAlignment alignFromName(const std::string& s, const std::string& where) {
    if (s == "left") {
        return TextAlignment::LEFT;
    }
    if (s == "center") {
        return TextAlignment::CENTER;
    }
    if (s == "right") {
        return TextAlignment::RIGHT;
    }
    throw ToolError(where + ": align must be left, center or right");
}

const char* capName(StrokeCapStyle c) {
    switch (static_cast<StrokeCapStyle::Value>(c)) {
        case StrokeCapStyle::BUTT:
            return "butt";
        case StrokeCapStyle::SQUARE:
            return "square";
        default:
            return "round";
    }
}

json fontJson(const XojFont& f) { return {{"name", f.getName()}, {"size", f.getSize()}}; }

XojFont fontFrom(const json& e, const std::string& where) {
    XojFont font("Sans", 12);
    if (e.contains("font")) {
        const json& f = e["font"];
        if (!f.is_object()) {
            throw ToolError(where + ": font must be {\"name\": ..., \"size\": ...}");
        }
        font.setName(f.value("name", std::string("Sans")));
        font.setSize(f.contains("size") ? Args::toNumber(f["size"], where + ".font.size") : 12.0);
    }
    return font;
}

xoj::util::Matrix matrixFrom(const json& t, const std::string& where) {
    xoj::util::Matrix m;
    auto num = [&](const char* k, double def) {
        return t.contains(k) ? Args::toNumber(t[k], where + ".transform." + k) : def;
    };
    m.xx = num("xx", 1);
    m.yx = num("yx", 0);
    m.xy = num("xy", 0);
    m.yy = num("yy", 1);
    m.shift = {num("x", 0), num("y", 0)};
    return m;
}

/// Positions a rectangular element: transform, bbox (scaled to fit) or x/y (top-left, natural size)
template <class T>
void placeRectangular(T& el, const json& e, const std::string& where, bool scaleToBbox) {
    if (e.contains("transform")) {
        el.setTransformation(matrixFrom(e["transform"], where));
        return;
    }
    if (e.contains("bbox") && scaleToBbox) {
        const json& b = e["bbox"];
        if (!b.is_array() || b.size() != 4) {
            throw ToolError(where + ": bbox must be [x, y, width, height]");
        }
        const auto nat = el.getNaturalSize();
        const double w = Args::toNumber(b[2], where + ".bbox"), h = Args::toNumber(b[3], where + ".bbox");
        const double sx = nat.width > 0 ? w / nat.width : 1, sy = nat.height > 0 ? h / nat.height : 1;
        el.setTransformation(xoj::util::Matrix::TRANSLATION(Args::toNumber(b[0], where + ".bbox"),
                                                            Args::toNumber(b[1], where + ".bbox"))
                                     .scale(sx, sy));
        return;
    }
    double x = 0, y = 0;
    if (e.contains("bbox")) {
        x = Args::toNumber(e["bbox"][0], where + ".bbox");
        y = Args::toNumber(e["bbox"][1], where + ".bbox");
    } else {
        if (!e.contains("x") || !e.contains("y")) {
            throw ToolError(where + ": needs a position: \"x\" and \"y\" (top-left, page points), \"bbox\" or "
                                    "\"transform\"");
        }
        x = Args::toNumber(e["x"], where + ".x");
        y = Args::toNumber(e["y"], where + ".y");
    }
    el.setTransformation(xoj::util::Matrix::TRANSLATION(x, y));
}

std::string requireString(const json& e, const char* key, const std::string& where) {
    if (!e.contains(key) || !e[key].is_string()) {
        throw ToolError(where + ": \"" + key + "\" (string) is required");
    }
    return e[key].get<std::string>();
}

}  // namespace

json elementToXJson(const Element* el) {
    json out = {{"type", elementTypeName(el)}};
    const auto& bb = el->getBoundingBox();
    switch (el->getType()) {
        case ELEMENT_STROKE: {
            const auto* s = static_cast<const Stroke*>(el);
            out["tool"] = s->getToolType() == StrokeTool::HIGHLIGHTER ? "highlighter" :
                          s->getToolType() == StrokeTool::ERASER      ? "eraser" :
                                                                        "pen";
            out["color"] = tools::colorToHex(s->getColor());
            out["width"] = s->getWidth();
            out["line_style"] = StrokeStyle::formatStyle(s->getLineStyle());
            out["cap"] = capName(s->getStrokeCapStyle());
            if (s->getFill() >= 0) {
                out["fill_opacity"] = s->getFill() / 255.0;
            }
            json pts = json::array();
            const bool pressure = s->hasPressure();
            for (const auto& p: s->getPointVector()) {
                pts.push_back(pressure ? json::array({p.x, p.y, p.z}) : json::array({p.x, p.y}));
            }
            out["points"] = std::move(pts);
            break;
        }
        case ELEMENT_TEXT: {
            const auto* t = static_cast<const Text*>(el);
            out["text"] = t->getText();
            out["font"] = fontJson(t->getFont());
            out["color"] = tools::colorToHex(t->getColor());
            out["align"] = alignName(t->getAlign());
            if (t->getWrap() > 0) {
                out["wrap"] = t->getWrap();
            }
            out["justify"] = t->getJustify();
            out["transform"] = transformJson(t->getTransformation());
            break;
        }
        case ELEMENT_TEXIMAGE: {
            const auto* t = static_cast<const TexImage*>(el);
            out["latex"] = t->getText();
            out["color"] = tools::colorToHex(t->getColor());
            out["data"] = base64Encode(t->getBinaryData());
            out["transform"] = transformJson(t->getTransformation());
            break;
        }
        case ELEMENT_IMAGE: {
            const auto* i = static_cast<const Image*>(el);
            out["data"] = base64Encode(i->getBinaryData());
            out["transform"] = transformJson(i->getTransformation());
            break;
        }
        case ELEMENT_LINK: {
            const auto* l = static_cast<const Link*>(el);
            out["text"] = l->getText();
            out["url"] = l->getUrl();
            out["font"] = fontJson(l->getFont());
            out["color"] = tools::colorToHex(l->getColor());
            out["align"] = alignName(l->getAlignment());
            out["transform"] = transformJson(l->getTransformation());
            break;
        }
    }
    out["bbox"] = bboxJson(bb.x, bb.y, bb.width, bb.height);
    return out;
}

json serialize(const std::vector<const Element*>& elements, json meta) {
    json list = json::array();
    for (const Element* e: elements) {
        list.push_back(elementToXJson(e));
    }
    json out = {{"format", FORMAT}, {"version", VERSION}};
    if (!meta.empty()) {
        out["source"] = std::move(meta);
    }
    out["elements"] = std::move(list);
    return out;
}

ElementPtr elementFromXJson(const json& e, const std::string& where, std::vector<std::string>& warnings) {
    if (!e.is_object()) {
        throw ToolError(where + ": element must be an object");
    }
    const std::string type = requireString(e, "type", where);
    const Color color = e.contains("color") ? tools::parseColor(e["color"], where + ".color") : Color(0, 0, 0);

    if (type == "stroke") {
        auto s = std::make_unique<Stroke>();
        const std::string tool = e.value("tool", std::string("pen"));
        if (tool == "highlighter") {
            s->setToolType(StrokeTool::HIGHLIGHTER);
        } else if (tool == "pen") {
            s->setToolType(StrokeTool::PEN);
        } else {
            throw ToolError(where + ": tool must be pen or highlighter");
        }
        s->setColor(color);
        s->setWidth(e.contains("width") ? Args::toNumber(e["width"], where + ".width") : 1.41);
        if (s->getWidth() <= 0) {
            throw ToolError(where + ": width must be positive");
        }
        if (e.contains("line_style")) {
            s->setLineStyle(StrokeStyle::parseStyle(e["line_style"].get<std::string>()));
        }
        if (e.contains("cap")) {
            const std::string cap = e["cap"].get<std::string>();
            s->setStrokeCapStyle(cap == "butt"   ? StrokeCapStyle::BUTT :
                                 cap == "square" ? StrokeCapStyle::SQUARE :
                                                   StrokeCapStyle::ROUND);
        }
        if (e.contains("fill_opacity") && !e["fill_opacity"].is_null()) {
            const double f = Args::toNumber(e["fill_opacity"], where + ".fill_opacity");
            s->setFill(static_cast<int>(std::lround(std::clamp(f, 0.0, 1.0) * 255)));
        }
        if (!e.contains("points") || !e["points"].is_array()) {
            throw ToolError(where + ": \"points\" must be an array of [x, y] or [x, y, width]");
        }
        const json& pts = e["points"];
        if (pts.size() < 2) {
            throw ToolError(where + ": a stroke needs at least 2 points");
        }
        const bool pressure = pts[0].is_array() && pts[0].size() >= 3;
        std::vector<Point> points;
        points.reserve(pts.size());
        for (size_t i = 0; i < pts.size(); i++) {
            const json& p = pts[i];
            const std::string pw = where + ".points[" + std::to_string(i) + "]";
            if (!p.is_array() || p.size() < 2 || (pressure && p.size() < 3)) {
                throw ToolError(pw + ": expected " + std::string(pressure ? "[x, y, width]" : "[x, y]") +
                                " (all points must have the same form)");
            }
            const double x = Args::toNumber(p[0], pw), y = Args::toNumber(p[1], pw);
            if (pressure) {
                const double w = Args::toNumber(p[2], pw);
                if (w <= 0 || !std::isfinite(w)) {
                    throw ToolError(pw + ": width must be positive");
                }
                points.emplace_back(x, y, w);
            } else {
                points.emplace_back(x, y);
            }
        }
        if (pressure && tool == "highlighter") {
            warnings.push_back(where + ": highlighter strokes have constant width; per-point widths ignored");
            for (auto& p: points) {
                p.z = Point::NO_PRESSURE;
            }
        }
        s->setPointVector(std::move(points));
        return s;
    }
    if (type == "text") {
        auto t = std::make_unique<Text>();
        t->setText(requireString(e, "text", where));
        t->setFont(fontFrom(e, where));
        t->setColor(color);
        if (e.contains("align")) {
            t->setAlignment(alignFromName(e["align"].get<std::string>(), where));
        }
        if (e.contains("wrap") && !e["wrap"].is_null()) {
            t->setWrap(Args::toNumber(e["wrap"], where + ".wrap"));
        }
        t->setJustify(e.value("justify", false));
        placeRectangular(*t, e, where, false);
        return t;
    }
    if (type == "link") {
        auto l = std::make_unique<Link>();
        l->setText(requireString(e, "text", where));
        l->setUrl(requireString(e, "url", where));
        l->setFont(fontFrom(e, where));
        l->setColor(e.contains("color") ? color : Color(0, 0, 0xff));
        if (e.contains("align")) {
            l->setAlignment(alignFromName(e["align"].get<std::string>(), where));
        }
        placeRectangular(*l, e, where, false);
        return l;
    }
    if (type == "image") {
        auto img = std::make_unique<Image>();
        std::string bytes = base64Decode(requireString(e, "data", where));
        if (bytes.empty()) {
            throw ToolError(where + ": \"data\" is not valid base64 image data");
        }
        img->setImage(std::move(bytes));
        img->getBoundingBox();  // decodes the image and computes its natural size
        if (img->getNaturalSize().width <= 0) {
            throw ToolError(where + ": \"data\" could not be decoded as an image (PNG, JPEG, ...)");
        }
        placeRectangular(*img, e, where, true);
        return img;
    }
    if (type == "latex") {
        if (!e.contains("data")) {
            throw ToolError(where + ": latex elements need \"data\" (the compiled PDF, as exported). To typeset a "
                                    "new formula use create_latex.");
        }
        auto t = std::make_unique<TexImage>();
        t->setText(requireString(e, "latex", where));
        GError* err = nullptr;
        if (!t->loadData(base64Decode(e["data"].get<std::string>()), &err)) {
            std::string msg = err ? err->message : "invalid data";
            if (err) {
                g_error_free(err);
            }
            throw ToolError(where + ": could not load the LaTeX PDF data: " + msg);
        }
        t->setColor(color);
        placeRectangular(*t, e, where, true);
        return t;
    }
    throw ToolError(where + ": unknown type '" + type + "' (stroke, text, latex, image or link)");
}

std::vector<ElementPtr> parse(const json& doc, std::vector<std::string>& warnings) {
    const json* list = &doc;
    if (doc.is_object()) {
        if (doc.contains("format") && doc["format"] != FORMAT) {
            throw ToolError("Not an xjson document (format must be \"" + std::string(FORMAT) + "\")");
        }
        if (doc.value("version", VERSION) > VERSION) {
            warnings.push_back("xjson version " + doc["version"].dump() + " is newer than supported (" +
                               std::to_string(VERSION) + "); unknown fields are ignored");
        }
        if (!doc.contains("elements")) {
            throw ToolError("xjson document has no \"elements\" array");
        }
        list = &doc["elements"];
    }
    if (!list->is_array()) {
        throw ToolError("\"elements\" must be an array");
    }
    std::vector<ElementPtr> out;
    for (size_t i = 0; i < list->size(); i++) {
        out.push_back(elementFromXJson((*list)[i], "elements[" + std::to_string(i) + "]", warnings));
    }
    return out;
}

}  // namespace xoj::mcp::xjson
