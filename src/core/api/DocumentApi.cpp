#include "DocumentApi.h"

#include <stdexcept>  // for invalid_argument

#include "model/Document.h"  // for Document
#include "model/Layer.h"     // for Layer
#include "model/XojPage.h"   // for XojPage

#include "ElementIds.h"
#include "Geometry.h"

namespace xoj::api {

std::optional<ElementLocation> locate(Document* doc, const Element* e) {
    if (!e) {
        return std::nullopt;
    }
    for (size_t p = 0; p < doc->getPageCount(); p++) {
        PageRef page = doc->getPage(p);
        size_t layerIndex = 0;
        for (Layer* l: page->getLayers()) {
            ++layerIndex;
            const Element::Index i = l->indexOf(e);
            if (i != Element::InvalidIndex) {
                return ElementLocation{p, layerIndex, i, const_cast<Element*>(e)};
            }
        }
    }
    return std::nullopt;
}

ElementLocation locateId(Document* doc, const std::string& id) {
    const Element* e = ElementIds::get().lookup(id);
    if (!e) {
        if (!ElementIds::parse(id)) {
            throw std::invalid_argument("'" + id + "' is not an element id (expected something like \"e42\")");
        }
        throw std::invalid_argument("Unknown element id '" + id +
                                    "'. Ids come from page_elements and creation tools; the element may have been "
                                    "erased and discarded.");
    }
    auto loc = locate(doc, e);
    if (!loc) {
        throw std::invalid_argument("Element '" + id +
                                    "' is not in the document right now (it was deleted; undo could restore it)");
    }
    return *loc;
}

std::vector<ElementLocation> elementsOnPage(const PageRef& page, size_t pageIndex, std::optional<size_t> layer,
                                            std::optional<xoj::util::Rectangle<double>> region, bool visibleOnly) {
    std::vector<ElementLocation> out;
    size_t layerIndex = 0;
    for (Layer* l: page->getLayers()) {
        ++layerIndex;
        if ((layer && *layer != layerIndex) || (visibleOnly && !l->isVisible())) {
            continue;
        }
        Element::Index i = 0;
        for (const auto& e: l->getElements()) {
            if (!region || intersects(e->getBoundingBox(), *region)) {
                out.push_back({pageIndex, layerIndex, i, e.get()});
            }
            i++;
        }
    }
    return out;
}

}  // namespace xoj::api
