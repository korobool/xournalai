/*
 * Xournal++ (xournalai)
 *
 * Read access to the document model: locating elements and listing page content
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>   // for size_t
#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/Element.h"   // for Element
#include "model/PageRef.h"   // for PageRef
#include "util/Rectangle.h"  // for Rectangle

class Control;
class Document;

namespace xoj::api {

/// Where an element lives in the document
struct ElementLocation {
    size_t page = 0;   ///< 0-based page index
    size_t layer = 0;  ///< 1-based layer index (as shown in the UI)
    Element::Index index = 0;
    Element* element = nullptr;
};

/// Finds an element in the document (scans all pages); std::nullopt if it is not part of the document
std::optional<ElementLocation> locate(Document* doc, const Element* e);

/// The page the user is on (0-based), always a valid index: right after pages are removed the application may
/// still point past the end for a moment
size_t currentPageIndex(Control* ctrl);

/**
 * @brief Resolves an element id ("e42") to its location.
 * Throws std::invalid_argument with an agent-friendly message if the id is unknown or the element is not in the
 * document (e.g. deleted - it may come back with undo).
 */
ElementLocation locateId(Document* doc, const std::string& id);

/**
 * @brief Elements of one page, bottom to top (layer by layer, then z-order within the layer).
 * @param layer 1-based layer to restrict to (all layers if empty)
 * @param region only elements whose bounding box intersects this rectangle (page points)
 * @param visibleOnly skip hidden layers
 */
std::vector<ElementLocation> elementsOnPage(const PageRef& page, size_t pageIndex, std::optional<size_t> layer,
                                            std::optional<xoj::util::Rectangle<double>> region,
                                            bool visibleOnly = false);

}  // namespace xoj::api
