/*
 * xournalai (based on Xournal++)
 *
 * Converting SVG drawings into editable strokes and text boxes
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/Point.h"     // for Point
#include "util/Color.h"      // for Color
#include "util/Rectangle.h"  // for Rectangle

namespace xoj::api::svg {

struct Placement {
    std::optional<xoj::util::Rectangle<double>> target;  ///< fit the drawing into this page rectangle
    std::string fit = "contain";                         ///< contain | cover | none (scale 1, top-left of target)
    double x = 0, y = 0;                                 ///< without target: page position of SVG (0,0)
    double scale = 1;                                    ///< without target: SVG user units → page points
};

struct SvgStroke {
    std::vector<Point> points;  ///< page points, no pressure
    Color color;
    double width = 1;
    std::optional<int> fill;  ///< 0..255 fill alpha (closed paths)
    bool dashed = false;
    std::string id;  ///< SVG id of the shape, if any
};

struct SvgText {
    std::string text;
    double x = 0, y = 0;  ///< top-left, page points
    double size = 12;
    std::string font = "Sans";
    Color color;
};

struct SvgResult {
    std::vector<SvgStroke> strokes;
    std::vector<SvgText> texts;
    std::vector<std::string> warnings;
    xoj::util::Rectangle<double> bounds;  ///< page area covered by the drawing
};

/**
 * @brief Converts an SVG document. 1 SVG user unit = 1 page point before placement scaling.
 * Throws std::invalid_argument for unparsable input or empty drawings.
 */
SvgResult convert(const std::string& svgText, const Placement& placement, double spacing = 1.0);

}  // namespace xoj::api::svg
