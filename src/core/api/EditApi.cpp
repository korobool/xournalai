#include "EditApi.h"

#include <algorithm>  // for sort, min, max
#include <cmath>      // for abs
#include <map>        // for map
#include <mutex>      // for unique_lock
#include <stdexcept>  // for invalid_argument

#include "control/Control.h"                // for Control
#include "control/ScrollHandler.h"          // for ScrollHandler
#include "control/layer/LayerController.h"  // for LayerController
#include "control/tools/EditSelection.h"    // for SelectionFactory
#include "gui/MainWindow.h"                 // for MainWindow
#include "gui/XournalView.h"                // for XournalView
#include "model/Document.h"                 // for Document
#include "model/Layer.h"                    // for Layer
#include "model/Stroke.h"                   // for Stroke
#include "model/XojPage.h"                  // for XojPage
#include "undo/ArrangeUndoAction.h"         // for ArrangeUndoAction
#include "undo/ColorUndoAction.h"           // for ColorUndoAction
#include "undo/DeleteUndoAction.h"          // for DeleteUndoAction
#include "undo/FillUndoAction.h"            // for FillUndoAction
#include "undo/GroupUndoAction.h"           // for GroupUndoAction
#include "undo/LineStyleUndoAction.h"       // for LineStyleUndoAction
#include "undo/MoveUndoAction.h"            // for MoveUndoAction
#include "undo/RotateUndoAction.h"          // for RotateUndoAction
#include "undo/ScaleUndoAction.h"           // for ScaleUndoAction
#include "undo/SizeUndoAction.h"            // for SizeUndoAction
#include "undo/UndoRedoHandler.h"           // for UndoRedoHandler

#include "DocumentApi.h"  // for locateId
#include "DrawApi.h"      // for resolveLayer

namespace xoj::api {

namespace {

constexpr double PI = 3.14159265358979323846;

/// Moves elements from one layer to another on the same page (undoable, keeps the element objects and ids)
class LayerTransferUndoAction: public UndoAction {
public:
    LayerTransferUndoAction(const PageRef& page, Layer* from, Layer* to, std::vector<Element*> elements):
            UndoAction("LayerTransferUndoAction"), from(from), to(to), elements(std::move(elements)) {
        this->page = page;
    }
    bool undo(Control* control) override {
        transfer(control, to, from);
        return true;
    }
    bool redo(Control* control) override {
        transfer(control, from, to);
        return true;
    }
    std::string getText() override { return "Move to layer"; }

private:
    void transfer(Control* control, Layer* a, Layer* b) {
        {
            std::unique_lock lock(*control->getDocument());
            for (Element* e: elements) {
                auto pos = a->removeElement(e);
                if (pos.e) {
                    b->addElement(std::move(pos.e));
                }
            }
        }
        page->firePageChanged();
    }
    Layer* from;
    Layer* to;
    std::vector<Element*> elements;
};

void addUndo(Control* control, std::unique_ptr<UndoAction> a) {
    control->getUndoRedoHandler()->addUndoAction(std::move(a));
}

}  // namespace

ElementGroup EditApi::resolve(const std::vector<std::string>& ids) {
    if (ids.empty()) {
        throw std::invalid_argument("No element ids given");
    }
    control->clearSelectionEndText();
    ElementGroup g;
    Document* doc = control->getDocument();
    std::shared_lock lock(*doc);
    std::map<Layer*, size_t> layerSlot;
    bool first = true;
    for (const auto& id: ids) {
        const ElementLocation loc = locateId(doc, id);
        if (first) {
            g.pageIndex = loc.page;
            g.page = doc->getPage(loc.page);
        } else if (loc.page != g.pageIndex) {
            throw std::invalid_argument("All elements must be on the same page (" + id + " is on page " +
                                        std::to_string(loc.page + 1) + ", others on page " +
                                        std::to_string(g.pageIndex + 1) + ")");
        }
        if (std::find(g.all.begin(), g.all.end(), loc.element) != g.all.end()) {
            continue;  // duplicate id
        }
        Layer* layer = g.page->getLayers()[loc.layer - 1];
        auto [it, inserted] = layerSlot.try_emplace(layer, g.byLayer.size());
        if (inserted) {
            g.byLayer.push_back({layer, {}});
        }
        g.byLayer[it->second].second.push_back(loc.element);
        g.all.push_back(loc.element);
        const auto& bb = loc.element->getBoundingBox();
        if (first) {
            g.x1 = bb.x;
            g.y1 = bb.y;
            g.x2 = bb.x + bb.width;
            g.y2 = bb.y + bb.height;
        } else {
            g.x1 = std::min(g.x1, bb.x);
            g.y1 = std::min(g.y1, bb.y);
            g.x2 = std::max(g.x2, bb.x + bb.width);
            g.y2 = std::max(g.y2, bb.y + bb.height);
        }
        first = false;
    }
    return g;
}

void EditApi::move(const ElementGroup& g, double dx, double dy) {
    auto group = std::make_unique<GroupUndoAction>();
    {
        std::unique_lock lock(*control->getDocument());
        for (Element* e: g.all) {
            e->move(dx, dy);
        }
    }
    for (const auto& [layer, elements]: g.byLayer) {
        group->addAction(std::make_unique<MoveUndoAction>(layer, g.page, elements, dx, dy, layer, g.page));
    }
    g.page->firePageChanged();
    addUndo(control, std::move(group));
}

void EditApi::scale(const ElementGroup& g, double fx, double fy, double x0, double y0, bool keepLineWidth) {
    if (!(fx > 0) || !(fy > 0)) {
        throw std::invalid_argument("Scale factors must be positive");
    }
    std::vector<Element*> all = g.all;
    auto action = std::make_unique<ScaleUndoAction>(g.page, &all, x0, y0, fx, fy, 0.0, keepLineWidth);
    action->redo(control);  // applies the scaling
    addUndo(control, std::move(action));
}

void EditApi::rotate(const ElementGroup& g, double degrees, double x0, double y0) {
    std::vector<Element*> all = g.all;
    auto action = std::make_unique<RotateUndoAction>(g.page, &all, x0, y0, degrees * PI / 180.0);
    action->redo(control);  // applies the rotation
    addUndo(control, std::move(action));
}

size_t EditApi::restyle(const ElementGroup& g, const Restyle& st) {
    auto group = std::make_unique<GroupUndoAction>();
    size_t changed = 0;
    {
        std::unique_lock lock(*control->getDocument());
        for (const auto& [layer, elements]: g.byLayer) {
            auto color = std::make_unique<ColorUndoAction>(g.page, layer);
            auto size = std::make_unique<SizeUndoAction>(g.page, layer);
            auto fill = std::make_unique<FillUndoAction>(g.page, layer);
            auto line = std::make_unique<LineStyleUndoAction>(g.page, layer);
            bool anyColor = false, anySize = false, anyFill = false, anyLine = false;
            for (Element* e: elements) {
                bool touched = false;
                if (st.color && e->getType() != ELEMENT_IMAGE) {
                    color->addStroke(e, e->getColor(), *st.color);
                    e->setColor(*st.color);
                    anyColor = touched = true;
                }
                if (e->getType() != ELEMENT_STROKE) {
                    changed += touched ? 1 : 0;
                    continue;
                }
                auto* s = static_cast<Stroke*>(e);
                if (st.width && *st.width > 0) {
                    auto oldPressure = SizeUndoAction::getPressure(s);
                    const double oldWidth = s->getWidth();
                    const double factor = *st.width / oldWidth;
                    s->setWidth(*st.width);
                    if (s->hasPressure()) {
                        s->scalePressure(factor);
                    }
                    size->addStroke(s, oldWidth, *st.width, oldPressure, SizeUndoAction::getPressure(s),
                                    s->getPointCount());
                    anySize = touched = true;
                }
                if (st.fill) {
                    fill->addStroke(s, s->getFill(), *st.fill);
                    s->setFill(*st.fill);
                    anyFill = touched = true;
                }
                if (st.lineStyle) {
                    line->addStroke(s, s->getLineStyle(), *st.lineStyle);
                    s->setLineStyle(*st.lineStyle);
                    anyLine = touched = true;
                }
                changed += touched ? 1 : 0;
            }
            if (anyColor) {
                group->addAction(std::move(color));
            }
            if (anySize) {
                group->addAction(std::move(size));
            }
            if (anyFill) {
                group->addAction(std::move(fill));
            }
            if (anyLine) {
                group->addAction(std::move(line));
            }
        }
    }
    g.page->firePageChanged();
    if (changed) {
        addUndo(control, std::move(group));
    }
    return changed;
}

void EditApi::reorder(const ElementGroup& g, const std::string& where) {
    if (where != "front" && where != "back") {
        throw std::invalid_argument("reorder must be 'front' or 'back'");
    }
    auto group = std::make_unique<GroupUndoAction>();
    for (const auto& [layer, elements]: g.byLayer) {
        InsertionOrderRef oldOrder, newOrder;
        for (Element* e: elements) {
            oldOrder.emplace_back(e, layer->indexOf(e));
        }
        std::sort(oldOrder.begin(), oldOrder.end());
        const auto count = static_cast<Element::Index>(layer->getElements().size());
        const auto k = static_cast<Element::Index>(oldOrder.size());
        for (Element::Index i = 0; i < k; i++) {
            newOrder.emplace_back(oldOrder[static_cast<size_t>(i)].e, where == "front" ? count - k + i : i);
        }
        auto action = std::make_unique<ArrangeUndoAction>(
                g.page, layer, where == "front" ? "Bring to front" : "Send to back", oldOrder, newOrder);
        action->redo(control);  // applies the new order
        group->addAction(std::move(action));
    }
    addUndo(control, std::move(group));
}

std::string EditApi::toLayer(const ElementGroup& g, const std::string& layerName) {
    LayerChoice target = DrawApi(control).resolveLayer(g.page, layerName, true);
    auto group = std::make_unique<GroupUndoAction>();
    if (target.undo) {
        group->addAction(std::move(target.undo));
    }
    for (const auto& [layer, elements]: g.byLayer) {
        if (layer == target.layer) {
            continue;
        }
        auto action = std::make_unique<LayerTransferUndoAction>(g.page, layer, target.layer, elements);
        action->redo(control);
        group->addAction(std::move(action));
    }
    addUndo(control, std::move(group));
    return target.name;
}

size_t EditApi::remove(const ElementGroup& g) {
    auto action = std::make_unique<DeleteUndoAction>(g.page, false);
    size_t count = 0;
    {
        std::unique_lock lock(*control->getDocument());
        for (const auto& [layer, elements]: g.byLayer) {
            for (Element* e: elements) {
                auto pos = layer->removeElement(e);
                if (pos.e) {
                    action->addElement(layer, std::move(pos.e), pos.pos);
                    count++;
                }
            }
        }
    }
    g.page->firePageChanged();
    addUndo(control, std::move(action));
    return count;
}

void EditApi::select(const ElementGroup& g) {
    if (g.byLayer.size() != 1) {
        throw std::invalid_argument("A selection can only contain elements of one layer");
    }
    auto* xournal = control->getWindow()->getXournal();
    XojPageView* view = xournal->getViewFor(g.pageIndex);
    if (!view) {
        throw std::invalid_argument("The page is not available");
    }
    size_t layerIndex = 0;
    const auto& layers = g.page->getLayers();
    for (size_t i = 0; i < layers.size(); i++) {
        if (layers[i] == g.byLayer[0].first) {
            layerIndex = i + 1;
        }
    }
    if (control->getCurrentPageNo() != g.pageIndex) {
        control->getScrollHandler()->scrollToPage(g.pageIndex);
        control->firePageSelected(g.pageIndex);
    }
    control->getLayerController()->switchToLay(layerIndex);
    InsertionOrderRef refs;
    for (Element* e: g.byLayer[0].second) {
        refs.emplace_back(e, g.byLayer[0].first->indexOf(e));
    }
    std::sort(refs.begin(), refs.end());
    xournal->setSelection(SelectionFactory::createFromElementsOnActiveLayer(control, g.page, view, refs).release());
}

}  // namespace xoj::api
