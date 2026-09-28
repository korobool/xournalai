/*
 * Xournal++ (xournalai)
 *
 * Heuristic layout analysis: groups page content into blocks (handwriting, figures, connectors, ...)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>   // for size_t
#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "util/Point.h"      // for Point
#include "util/Rectangle.h"  // for Rectangle

namespace xoj::api {

struct LayoutItem {
    enum class Kind { PenStroke, Highlighter, Text, Latex, Image, Link };
    size_t index = 0;  ///< must equal the item's position in the input vector (returned in blocks)
    Kind kind = Kind::PenStroke;
    xoj::util::Rectangle<double> bbox;
    double length = 0;                 ///< path length (strokes)
    xoj::util::Point<double> first{};  ///< first point (strokes)
    xoj::util::Point<double> last{};   ///< last point (strokes)
};

struct LayoutBlock {
    std::string kind;  ///< handwriting | figure | highlight | typed_text | latex | image | link
    xoj::util::Rectangle<double> bbox;
    std::vector<size_t> items;                        ///< caller indices, in input order
    std::vector<xoj::util::Rectangle<double>> lines;  ///< handwriting: one bbox per text line, top to bottom
    std::optional<size_t> parent;                     ///< index of the figure block this block lies inside
};

struct LayoutConnector {
    size_t item = 0;  ///< the stroke
    xoj::util::Point<double> from{};
    xoj::util::Point<double> to{};
    std::optional<size_t> fromBlock;  ///< block index near the start point
    std::optional<size_t> toBlock;    ///< block index near the end point
};

struct Layout {
    std::vector<LayoutBlock> blocks;  ///< in reading order (top to bottom, left to right)
    std::vector<LayoutConnector> connectors;
    double typicalHeight = 0;  ///< estimated handwriting x-height scale in points
};

/**
 * @brief Groups items into blocks.
 *
 * Pen strokes are clustered by proximity (scaled by the typical stroke height) into text lines and shapes. Lines
 * are classified as handwriting (flat clusters of small strokes) or figures, handwriting lines are merged into
 * paragraphs, and nearby figure parts are merged. Long, nearly straight strokes that end at blocks become
 * connectors (arrows/lines of a diagram). Highlighter strokes, text, LaTeX, images and links become their own
 * blocks. This is a heuristic: it helps an agent choose what to look at, it does not "read" the content.
 */
Layout analyzeLayout(const std::vector<LayoutItem>& items);

}  // namespace xoj::api
