/*
 * Xournal++ (xournalai)
 *
 * Finding room for new content, and adding pages
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/PageRef.h"   // for PageRef
#include "util/Rectangle.h"  // for Rectangle

class Control;

namespace xoj::api {

using Rect = xoj::util::Rectangle<double>;

/**
 * @brief Finds an empty w x h area on a page (all visible content counts as occupied, grown by `margin`; the page
 * border keeps `margin` too).
 * Without `near`, returns the first free position in reading order (top to bottom, left to right). With `near`,
 * returns the free position closest to it, restricted to `side` (right | below | left | above | any).
 */
std::optional<Rect> findFreeSpace(const std::vector<Rect>& occupied, double pageWidth, double pageHeight, double w,
                                  double h, double margin, std::optional<Rect> near = std::nullopt,
                                  const std::string& side = "any");

/// Bounding boxes of all elements on visible layers of a page
std::vector<Rect> occupiedAreas(const PageRef& page);

/**
 * @brief Appends a new page after the last one (same size; same paper background unless that is a PDF/image page,
 * then plain). One undo step. Does not scroll. Returns its 0-based index.
 */
size_t appendPage(Control* control);

}  // namespace xoj::api
