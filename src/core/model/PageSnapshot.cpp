#include "PageSnapshot.h"

#include "model/Element.h"  // for Element
#include "model/Layer.h"    // for Layer
#include "model/Stroke.h"   // for Stroke
#include "model/XojPage.h"  // for XojPage

namespace xoj::model {

PageRef snapshotPage(const XojPage& page, const Range* only) {
    auto snap = std::make_shared<XojPage>(page.getWidth(), page.getHeight(), /*suppressLayerCreation=*/true);
    // Copied field by field: the setters have side effects (setBackgroundPdfPageNr makes any page a PDF page)
    snap->bgType = page.bgType;
    snap->pdfBackgroundPage = page.pdfBackgroundPage;
    snap->backgroundColor = page.backgroundColor;
    snap->backgroundImage = page.backgroundImage;
    snap->backgroundName = page.backgroundName;
    for (const Layer* l: page.getLayersView()) {
        auto* copy = new Layer();
        if (l->hasName()) {
            copy->setName(l->getName());
        }
        copy->setVisible(l->isVisible());
        for (const Element* e: l->getElementsView()) {
            if (!only || e->intersectsArea(only->minX, only->minY, only->getWidth(), only->getHeight())) {
                if (e->getType() == ELEMENT_STROKE && static_cast<const Stroke*>(e)->getErasable() != nullptr) {
                    delete copy;
                    return nullptr;
                }
                copy->addElement(e->clone());
            }
        }
        snap->addLayer(copy);
    }
    return snap;
}

}  // namespace xoj::model
