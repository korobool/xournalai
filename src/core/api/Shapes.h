/*
 * Xournal++ (xournalai)
 *
 * Geometry generators for shapes drawn by agents (all coordinates in page points)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>  // for vector

#include "model/Point.h"  // for Point

namespace xoj::api::shapes {

using Polyline = std::vector<Point>;

struct Shape {
    std::vector<Polyline> strokes;  ///< one polyline per stroke
    bool closed = false;            ///< may be filled
};

/// Straight line; `spacing` is the point spacing (stylus-like density)
Shape line(double x1, double y1, double x2, double y2, double spacing = 1.0);
/// Line with an open arrowhead at the end (and at the start if `both`), drawn as one stroke
Shape arrow(double x1, double y1, double x2, double y2, double headSize, bool both, double spacing = 1.0);
/// Axis-aligned rectangle, optionally with rounded corners
Shape rectangle(double x, double y, double w, double h, double cornerRadius = 0, double spacing = 1.0);
Shape ellipse(double cx, double cy, double rx, double ry, double spacing = 1.0);
/// Polygon (closed) or polyline (open) through the given vertices
Shape polygon(const std::vector<Point>& vertices, bool closed, double spacing = 1.0);
/// Cubic Bézier chain: p0, c1, c2, p1, c1, c2, p2, ... (3n+1 points)
Shape bezier(const std::vector<Point>& controlPoints, double spacing = 1.0);
/// Circular arc; angles in degrees, 0 = +x (right), positive = clockwise on the page (y points down)
Shape arc(double cx, double cy, double r, double startDeg, double endDeg, double spacing = 1.0);
/// Two axes with arrowheads: origin at (x, y), x axis to the right, y axis upwards
Shape coordinateSystem(double x, double y, double xLength, double yLength, double headSize, double spacing = 1.0);

}  // namespace xoj::api::shapes
