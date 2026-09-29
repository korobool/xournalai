/*
 * xournalai (based on Xournal++)
 *
 * JSON view of document elements for agents
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include "api/DocumentApi.h"  // for ElementLocation

#include "Json.h"

class Element;

namespace xoj::mcp {

enum class Detail {
    Bbox,        ///< id, type, layer, bbox, color; text content for text/LaTeX/links
    Simplified,  ///< + style and RDP-simplified stroke points
    Full         ///< + every stroke point
};

Detail detailFromName(const std::string& name);

/**
 * @brief JSON description of an element.
 *
 * Stroke points are [x, y] or, when the stroke has pressure, [x, y, w] where w is the stroke width in points at
 * that point. Coordinates are page points rounded to 0.01.
 */
json elementToJson(const api::ElementLocation& loc, Detail detail, double tolerance);

/// "stroke", "text", "latex", "image" or "link"
const char* elementTypeName(const Element* e);

/// [x, y, width, height] rounded to 0.01
json bboxJson(double x, double y, double w, double h);

}  // namespace xoj::mcp
