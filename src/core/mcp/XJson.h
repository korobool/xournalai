/*
 * Xournal++ (xournalai)
 *
 * xjson: lossless JSON interchange format for document elements
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "model/Element.h"  // for Element, ElementPtr

#include "Json.h"

namespace xoj::mcp::xjson {

constexpr const char* FORMAT = "xournalai.xjson";
constexpr int VERSION = 1;

/**
 * @brief Serializes elements losslessly.
 *
 * {"format": "xournalai.xjson", "version": 1, "elements": [...]} where each element is
 *  - stroke: tool, color, width, points [[x,y] | [x,y,w]], line_style, cap, fill_opacity (optional)
 *  - text:   text, font {name,size}, color, align, wrap, justify, transform
 *  - latex:  latex (source), data (base64 of the compiled PDF), color, transform
 *  - image:  data (base64 of the original file bytes), transform
 *  - link:   text, url, font, color, align, transform
 * transform = {xx, yx, xy, yy, x, y}: maps element-local coordinates to page points. A "bbox" is added for
 * information (ignored when reading if a transform is present).
 */
json serialize(const std::vector<const Element*>& elements, json meta = json::object());

json elementToXJson(const Element* e);

/**
 * @brief Parses an xjson document (or a bare array of elements) into new elements.
 *
 * Lenient for hand-written input: sensible defaults for missing style fields; text/link/image may use
 * "x","y" (top-left) or "bbox" [x,y,w,h] instead of a transform. Throws ToolError with the element index and the
 * problem for invalid input.
 */
std::vector<ElementPtr> parse(const json& doc, std::vector<std::string>& warnings);

ElementPtr elementFromXJson(const json& e, const std::string& where, std::vector<std::string>& warnings);

}  // namespace xoj::mcp::xjson
