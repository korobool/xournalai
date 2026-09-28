#include "SvgImport.h"

#include <algorithm>  // for min, max
#include <cmath>      // for hypot, ceil
#include <cstdio>     // for sscanf
#include <cstring>    // for strlen
#include <regex>      // for regex
#include <stdexcept>  // for invalid_argument

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

namespace xoj::api::svg {

namespace {

Color fromNsvg(unsigned int abgr, float opacity) {
    const auto a = static_cast<uint8_t>(std::clamp(((abgr >> 24) & 0xffU) * opacity, 0.0f, 255.0f));
    return Color(static_cast<uint8_t>(abgr & 0xffU), static_cast<uint8_t>((abgr >> 8) & 0xffU),
                 static_cast<uint8_t>((abgr >> 16) & 0xffU), a);
}

Color paintColor(const NSVGpaint& paint, float opacity, std::vector<std::string>& warnings, const std::string& id) {
    if (paint.type == NSVG_PAINT_COLOR) {
        return fromNsvg(paint.color, opacity);
    }
    if ((paint.type == NSVG_PAINT_LINEAR_GRADIENT || paint.type == NSVG_PAINT_RADIAL_GRADIENT) && paint.gradient &&
        paint.gradient->nstops > 0) {
        warnings.push_back("Gradient in '" + id + "' replaced by its first color (strokes have one color)");
        return fromNsvg(paint.gradient->stops[0].color, opacity);
    }
    return Color(0, 0, 0);
}

std::string attr(const std::string& tag, const std::string& name) {
    const std::regex re("\\b" + name + "\\s*=\\s*[\"']([^\"']*)[\"']");
    std::smatch m;
    return std::regex_search(tag, m, re) ? m[1].str() : std::string();
}

/// Extracts <text> elements (nanosvg ignores them). Transforms on text elements are not supported.
std::vector<SvgText> extractTexts(const std::string& svg, std::vector<std::string>& warnings) {
    std::vector<SvgText> out;
    const std::regex re("<text\\b([^>]*)>([\\s\\S]*?)</text>");
    for (auto it = std::sregex_iterator(svg.begin(), svg.end(), re); it != std::sregex_iterator(); ++it) {
        const std::string tag = (*it)[1].str();
        std::string content = std::regex_replace((*it)[2].str(), std::regex("<[^>]*>"), "");
        content = std::regex_replace(content, std::regex("&lt;"), "<");
        content = std::regex_replace(content, std::regex("&gt;"), ">");
        content = std::regex_replace(content, std::regex("&amp;"), "&");
        content = std::regex_replace(content, std::regex("\\s+"), " ");
        if (content.find_first_not_of(' ') == std::string::npos) {
            continue;
        }
        if (!attr(tag, "transform").empty()) {
            warnings.push_back("Transform on <text> \"" + content + "\" ignored");
        }
        SvgText t;
        t.text = content;
        t.x = std::atof(attr(tag, "x").c_str());
        t.size = attr(tag, "font-size").empty() ? 12 : std::atof(attr(tag, "font-size").c_str());
        if (t.size <= 0) {
            t.size = 12;
        }
        t.y = std::atof(attr(tag, "y").c_str()) - 0.8 * t.size;  // baseline → top
        const std::string family = attr(tag, "font-family");
        if (!family.empty()) {
            t.font = family.substr(0, family.find(','));
            t.font.erase(std::remove(t.font.begin(), t.font.end(), '\''), t.font.end());
        }
        const std::string anchor = attr(tag, "text-anchor");
        if (anchor == "middle" || anchor == "end") {
            const double approxWidth = 0.55 * t.size * static_cast<double>(content.size());
            t.x -= anchor == "middle" ? approxWidth / 2 : approxWidth;
        }
        t.color = Color(0, 0, 0);
        const std::string fill = attr(tag, "fill");
        unsigned r = 0, g = 0, b = 0;
        if (fill.size() == 7 && fill[0] == '#' && std::sscanf(fill.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            t.color = Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        } else if (!fill.empty() && fill != "black") {
            warnings.push_back("Text color '" + fill + "' not understood (use #rrggbb); black used");
        }
        out.push_back(t);
    }
    return out;
}

}  // namespace

SvgResult convert(const std::string& svgText, const Placement& placement, double spacing) {
    if (svgText.find("<svg") == std::string::npos) {
        throw std::invalid_argument("Not an SVG document (no <svg> element)");
    }
    SvgResult result;
    std::string copy = svgText;  // nsvgParse modifies its input
    NSVGimage* image = nsvgParse(copy.data(), "px", 96.0f);
    if (!image) {
        throw std::invalid_argument("Could not parse the SVG");
    }
    auto texts = extractTexts(svgText, result.warnings);

    // Drawing extent in SVG units: the viewport if given, else the union of shapes and texts
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (NSVGshape* s = image->shapes; s; s = s->next) {
        minX = std::min(minX, static_cast<double>(s->bounds[0]));
        minY = std::min(minY, static_cast<double>(s->bounds[1]));
        maxX = std::max(maxX, static_cast<double>(s->bounds[2]));
        maxY = std::max(maxY, static_cast<double>(s->bounds[3]));
    }
    for (const auto& t: texts) {
        minX = std::min(minX, t.x);
        minY = std::min(minY, t.y);
        maxX = std::max(maxX, t.x + 0.55 * t.size * static_cast<double>(t.text.size()));
        maxY = std::max(maxY, t.y + t.size);
    }
    if (minX > maxX) {
        nsvgDelete(image);
        throw std::invalid_argument("The SVG contains nothing drawable (paths, shapes or text)");
    }
    double originX = minX, originY = minY, width = maxX - minX, height = maxY - minY;
    if (image->width > 0 && image->height > 0) {  // explicit viewport / viewBox
        originX = 0;
        originY = 0;
        width = image->width;
        height = image->height;
    }

    // SVG units → page points
    double scale = placement.scale, offX = placement.x, offY = placement.y;
    if (placement.target) {
        const auto& t = *placement.target;
        if (placement.fit == "none") {
            scale = 1;
        } else {
            const double sx = t.width / std::max(width, 1e-9), sy = t.height / std::max(height, 1e-9);
            scale = placement.fit == "cover" ? std::max(sx, sy) : std::min(sx, sy);
        }
        offX = t.x + (placement.fit == "none" ? 0 : (t.width - width * scale) / 2) - originX * scale;
        offY = t.y + (placement.fit == "none" ? 0 : (t.height - height * scale) / 2) - originY * scale;
    }
    auto map = [&](double x, double y) { return Point(offX + x * scale, offY + y * scale); };

    for (NSVGshape* s = image->shapes; s; s = s->next) {
        if (!(s->flags & NSVG_FLAGS_VISIBLE)) {
            continue;
        }
        const std::string id = s->id[0] ? s->id : "shape";
        const bool hasStroke = s->stroke.type != NSVG_PAINT_NONE && s->strokeWidth > 0;
        const bool hasFill = s->fill.type != NSVG_PAINT_NONE;
        if (!hasStroke && !hasFill) {
            continue;
        }
        const Color strokeColor = hasStroke ? paintColor(s->stroke, s->opacity, result.warnings, id) : Color(0, 0, 0);
        const Color fillColor = hasFill ? paintColor(s->fill, s->opacity, result.warnings, id) : strokeColor;
        for (NSVGpath* p = s->paths; p; p = p->next) {
            std::vector<Point> pts;
            pts.push_back(map(p->pts[0], p->pts[1]));
            for (int i = 0; i + 3 < p->npts; i += 3) {
                const float* c = &p->pts[i * 2];
                const Point p0 = map(c[0], c[1]), c1 = map(c[2], c[3]), c2 = map(c[4], c[5]), p1 = map(c[6], c[7]);
                const double approx = std::hypot(c1.x - p0.x, c1.y - p0.y) + std::hypot(c2.x - c1.x, c2.y - c1.y) +
                                      std::hypot(p1.x - c2.x, p1.y - c2.y);
                const int n = std::max(1, static_cast<int>(std::ceil(approx / std::max(spacing, 0.2))));
                for (int k = 1; k <= n; k++) {
                    const double t = static_cast<double>(k) / n, u = 1 - t;
                    pts.emplace_back(u * u * u * p0.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * p1.x,
                                     u * u * u * p0.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * p1.y);
                }
            }
            if (p->closed && pts.size() > 1 &&
                std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y) > 1e-6) {
                pts.push_back(pts.front());
            }
            if (pts.size() < 2) {
                continue;
            }
            const bool closed =
                    p->closed || std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y) < 1e-6;
            // Xournal++ fills with the stroke's own color: a differently colored fill needs its own stroke
            if (hasFill && closed && (!hasStroke || fillColor != strokeColor)) {
                SvgStroke f;
                f.points = pts;
                f.color = fillColor;
                f.width = hasStroke ? 0.3 : std::max(0.3, 0.5 * scale);
                f.fill = static_cast<int>(fillColor.alpha);
                f.color.alpha = 0xff;
                f.id = id;
                result.strokes.push_back(std::move(f));
            } else if (hasFill && !closed && !hasStroke) {
                result.warnings.push_back("Open filled path in '" + id + "' drawn as a line");
            }
            if (hasStroke) {
                SvgStroke st;
                st.points = std::move(pts);
                st.color = strokeColor;
                st.width = std::max(0.1, s->strokeWidth * scale);
                if (hasFill && closed && fillColor == strokeColor) {
                    st.fill = static_cast<int>(fillColor.alpha);
                    st.color.alpha = 0xff;
                }
                st.dashed = s->strokeDashCount > 0;
                st.id = id;
                result.strokes.push_back(std::move(st));
            } else if (!hasFill || !closed) {
                SvgStroke st;
                st.points = std::move(pts);
                st.color = fillColor;
                st.width = std::max(0.3, 0.5 * scale);
                st.id = id;
                result.strokes.push_back(std::move(st));
            }
        }
    }
    nsvgDelete(image);

    for (auto t: texts) {
        const Point p = map(t.x, t.y);
        t.x = p.x;
        t.y = p.y;
        t.size *= scale;
        result.texts.push_back(t);
    }
    if (result.strokes.empty() && result.texts.empty()) {
        throw std::invalid_argument("The SVG contains nothing drawable");
    }
    const Point a = map(originX, originY);
    result.bounds = {a.x, a.y, width * scale, height * scale};
    return result;
}

}  // namespace xoj::api::svg
