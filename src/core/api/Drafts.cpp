#include "Drafts.h"

#include <algorithm>     // for find
#include <mutex>         // for unique_lock
#include <shared_mutex>  // for shared_lock
#include <stdexcept>     // for invalid_argument

#include "control/Control.h"                // for Control
#include "control/layer/LayerController.h"  // for LayerController
#include "model/Document.h"                 // for Document
#include "model/Layer.h"                    // for Layer
#include "model/XojPage.h"                  // for XojPage

namespace xoj::api {

Drafts& Drafts::get() {
    static Drafts d;
    return d;
}

const Draft& Drafts::begin(Control* control, size_t pageIndex) {
    Document* doc = control->getDocument();
    PageRef page;
    {
        std::shared_lock lock(*doc);
        if (pageIndex >= doc->getPageCount()) {
            throw std::invalid_argument("Page " + std::to_string(pageIndex + 1) + " does not exist");
        }
        page = doc->getPage(pageIndex);
    }
    Draft d;
    d.id = "d" + std::to_string(++counter);
    d.page = page;
    d.layerName = "AI draft " + d.id;
    d.layer = new Layer();
    d.layer->setName(d.layerName);
    d.layer->setVisible(false);  // private until committed
    const auto selected = page->getSelectedLayerId();
    control->getLayerController()->insertLayer(page, d.layer, static_cast<Layer::Index>(page->getLayerCount()));
    page->setSelectedLayerId(selected);
    return drafts.emplace(d.id, d).first->second;
}

Draft& Drafts::find(Control* control, const std::string& id) {
    auto it = drafts.find(id);
    if (it == drafts.end()) {
        throw std::invalid_argument("No open draft '" + id + "' (drafts: begin with draft(op=\"begin\"))");
    }
    // The document may have been replaced or the user may have deleted the layer
    Document* doc = control->getDocument();
    std::shared_lock lock(*doc);
    const bool pageAlive = doc->indexOf(it->second.page) != npos;
    const auto& layers = it->second.page->getLayers();
    if (!pageAlive || std::find(layers.begin(), layers.end(), it->second.layer) == layers.end()) {
        drafts.erase(it);
        throw std::invalid_argument("Draft '" + id + "' no longer exists (its page or layer was removed)");
    }
    return it->second;
}

bool Drafts::isDraftLayer(const Layer* layer) const {
    for (const auto& [id, d]: drafts) {
        if (d.layer == layer) {
            return true;
        }
    }
    return false;
}

void Drafts::close(Control* control, const std::string& id) {
    Draft& d = find(control, id);
    Layer* layer = d.layer;
    PageRef page = d.page;
    const auto selected = page->getSelectedLayerId();
    control->getLayerController()->removeLayer(page, layer);
    page->setSelectedLayerId(std::min<size_t>(selected, page->getLayerCount()));
    drafts.erase(id);
    {
        std::unique_lock lock(*control->getDocument());
        delete layer;  // deletes the remaining elements too
    }
}

}  // namespace xoj::api
