/*
 * xournalai (based on Xournal++)
 *
 * Geometry helpers for the application services
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <vector>  // for vector

#include "model/Point.h"     // for Point
#include "util/Rectangle.h"  // for Rectangle

namespace xoj::api {

/**
 * @brief Ramer–Douglas–Peucker simplification of a polyline.
 *
 * Keeps the first and last point and every point that deviates more than `tolerance` (page points) from the
 * simplified line. Kept points keep their pressure (z). A tolerance <= 0 returns the input unchanged.
 */
std::vector<Point> simplify(const std::vector<Point>& points, double tolerance);

/// True if two rectangles overlap (touching counts)
bool intersects(const xoj::util::Rectangle<double>& a, const xoj::util::Rectangle<double>& b);

/// Rounds to `decimals` decimal places (keeps JSON output short)
double roundTo(double v, int decimals = 2);

}  // namespace xoj::api
