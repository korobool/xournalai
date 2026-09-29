/*
 * xournalai (based on Xournal++)
 *
 * Rendering pages (or parts of them) to PNG images for agents
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>   // for size_t
#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "util/Color.h"      // for Color
#include "util/Rectangle.h"  // for Rectangle

class Document;

namespace xoj::api {

struct Highlight {
    xoj::util::Rectangle<double> rect;  ///< page points
    std::string label;                  ///< drawn next to the box (e.g. an element id); may be empty
};

struct RenderOptions {
    size_t page = 0;                                     ///< 0-based
    std::optional<xoj::util::Rectangle<double>> region;  ///< page points; whole page if empty
    double dpi = 100;                                    ///< requested resolution (72 dpi = 1 px per point)
    int maxPixels = 1400;                                ///< cap for the longer image side
    bool background = true;                              ///< paper, ruling, PDF and image backgrounds
    std::optional<std::vector<size_t>> layers;           ///< 1-based layers to draw (all visible if empty)
    bool grid = false;                                   ///< overlay a labelled coordinate grid
    double gridStep = 50;                                ///< grid spacing in page points
    std::vector<Highlight> highlights;                   ///< boxes to draw on top
    Color highlightColor = Color(0xe0, 0x1b, 0x84);      ///< magenta: rarely used for real ink
};

struct RenderedImage {
    std::string png;
    int widthPx = 0;
    int heightPx = 0;
    double scale = 1;                     ///< pixels per page point
    xoj::util::Rectangle<double> region;  ///< the rendered area in page points
};

/// Renders a page area. Must be called on the main thread. Throws std::invalid_argument for bad options.
RenderedImage renderPage(Document* doc, const RenderOptions& options);

/// Renders a page area as a vector SVG document (1 SVG unit = 1 page point). Ignores dpi, grid and highlights.
std::string renderSvg(Document* doc, const RenderOptions& options);

}  // namespace xoj::api
