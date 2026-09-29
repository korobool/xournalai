/*
 * xournalai (based on Xournal++)
 *
 * Finds the user's handwritten markers (*!, **!, *w!, *c!, *r!) among fresh strokes, cheaply and without a model:
 * an asterisk is 2-4 short straight strokes crossing each other; "!" is a short upright bar with a dot below it.
 * Strokes between the asterisk(s) and the "!" are the letter, which the serving session reads from a crop.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "model/Point.h"     // for Point
#include "util/Rectangle.h"  // for Rectangle

namespace xoj::assistant {

struct StrokeShape {
    std::string id;
    std::vector<Point> points;
};

struct MarkerCandidate {
    std::string kind;                   ///< "*!", "**!", or "*?!" (a letter between: w, c, r, … read from the image)
    std::vector<std::string> ids;       ///< all strokes of the marker (remove them after acting)
    xoj::util::Rectangle<double> area;  ///< the marker's bounding box
};

/// Markers among `strokes` (recent strokes of one page, in drawing order); each stroke belongs to one marker at most
std::vector<MarkerCandidate> findMarkers(const std::vector<StrokeShape>& strokes);

}  // namespace xoj::assistant
