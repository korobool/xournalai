#include "DrawApi.h"

#include <algorithm>  // for min, max
#include <atomic>     // for atomic
#include <cmath>      // for ceil
#include <memory>     // for shared_ptr
#include <mutex>      // for unique_lock
#include <stdexcept>  // for invalid_argument

#include <glib.h>  // for g_timeout_add

#include "control/Control.h"                // for Control
#include "control/layer/LayerController.h"  // for LayerController
#include "control/settings/Settings.h"      // for Settings
#include "model/Document.h"                 // for Document
#include "model/Layer.h"                    // for Layer
#include "model/Stroke.h"                   // for Stroke
#include "model/XojPage.h"                  // for XojPage
#include "undo/GroupUndoAction.h"           // for GroupUndoAction
#include "undo/InsertLayerUndoAction.h"     // for InsertLayerUndoAction
#include "undo/InsertUndoAction.h"          // for InsertsUndoAction
#include "undo/UndoRedoHandler.h"           // for UndoRedoHandler

#include "ElementIds.h"

namespace xoj::api {

namespace {

std::string layerName(const Layer* l, size_t index) {
    return l->hasName() ? l->getName() : "Layer " + std::to_string(index);
}

xoj::util::Rectangle<double> unionOf(const std::vector<const Element*>& elements) {
    xoj::util::Rectangle<double> r = elements.front()->getBoundingBox();
    for (const Element* e: elements) {
        const auto& b = e->getBoundingBox();
        const double x1 = std::min(r.x, b.x), y1 = std::min(r.y, b.y);
        const double x2 = std::max(r.x + r.width, b.x + b.width), y2 = std::max(r.y + r.height, b.y + b.height);
        r = {x1, y1, x2 - x1, y2 - y1};
    }
    return r;
}

/// State of a running stroke-growing animation
struct Animation {
    Control* control;
    PageRef page;
    std::vector<std::pair<Stroke*, std::vector<Point>>> strokes;  // stroke, its full point list
    size_t current = 0;
    size_t shown = 2;
    size_t pointsPerTick = 8;
    std::function<void(bool completed)> finish;
};

gboolean animationTick(gpointer data) {
    auto* a = static_cast<Animation*>(data);
    Document* doc = a->control->getDocument();
    while (a->current < a->strokes.size()) {
        auto& [stroke, full] = a->strokes[a->current];
        if (!ElementIds::get().existingId(stroke)) {  // destroyed meanwhile (document replaced): stop safely
            a->finish(false);
            delete a;
            return G_SOURCE_REMOVE;
        }
        a->shown = std::min(full.size(), a->shown + a->pointsPerTick);
        {
            std::unique_lock lock(*doc);
            stroke->setPointVector(
                    std::vector<Point>(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(a->shown)));
        }
        auto bb = stroke->getBoundingBox();
        a->page->fireRectChanged(bb);
        if (a->shown < full.size()) {
            return G_SOURCE_CONTINUE;
        }
        a->current++;
        a->shown = 2;
        if (a->current < a->strokes.size()) {
            return G_SOURCE_CONTINUE;  // next stroke starts on the next tick
        }
    }
    a->finish(true);
    delete a;
    return G_SOURCE_REMOVE;
}

}  // namespace

std::string DrawApi::nextOperation() {
    static std::atomic<unsigned> counter{0};
    return "op" + std::to_string(++counter);
}

PressureSettings DrawApi::pressureSettings() const {
    Settings* s = control->getSettings();
    return {s->getMinimumPressure(), s->getPressureMultiplier(), static_cast<bool>(s->isPressureSensitivity())};
}

void DrawApi::insert(const DrawTarget& target, std::vector<ElementPtr> elements, const AnimationOptions& animation,
                     std::function<void(const DrawResult&)> done) {
    if (elements.empty()) {
        throw std::invalid_argument("Nothing to draw");
    }
    Document* doc = control->getDocument();
    PageRef page;
    {
        std::shared_lock lock(*doc);
        if (target.page >= doc->getPageCount()) {
            throw std::invalid_argument("Page " + std::to_string(target.page + 1) + " does not exist");
        }
        page = doc->getPage(target.page);
    }
    control->clearSelectionEndText();

    DrawResult result;
    result.operation = nextOperation();
    result.page = target.page;
    auto group = std::make_unique<GroupUndoAction>();

    // Resolve (or create) the layer
    Layer* layer = nullptr;
    size_t layerIndex = 0;
    std::string wanted = target.layer.empty() ? "AI" : target.layer;
    {
        std::unique_lock lock(*doc);
        auto& layers = page->getLayers();
        if (wanted == "current") {
            layerIndex = std::max<size_t>(page->getSelectedLayerId(), 1);
            layer = page->getSelectedLayer();
        } else if (wanted[0] == '#') {
            size_t n = 0;
            try {
                n = std::stoul(wanted.substr(1));
            } catch (const std::exception&) {}
            if (n < 1 || n > layers.size()) {
                throw std::invalid_argument("Layer " + wanted + " does not exist (page has " +
                                            std::to_string(layers.size()) + " layers)");
            }
            layerIndex = n;
            layer = layers[n - 1];
        } else {
            for (size_t i = 0; i < layers.size(); i++) {
                if (layers[i]->hasName() && layers[i]->getName() == wanted) {
                    layer = layers[i];
                    layerIndex = i + 1;
                }
            }
            if (!layer) {
                if (!target.createLayer) {
                    throw std::invalid_argument("No layer named '" + wanted + "' on page " +
                                                std::to_string(target.page + 1));
                }
                result.layerCreated = true;
            }
        }
    }
    if (result.layerCreated) {
        const auto selected = page->getSelectedLayerId();
        layer = new Layer();
        layer->setName(wanted);
        const auto position = static_cast<Layer::Index>(page->getLayerCount());  // on top
        control->getLayerController()->insertLayer(page, layer, position);       // locks the document itself
        page->setSelectedLayerId(selected);                                      // keep the user's layer selected
        layerIndex = page->getLayerCount();
        group->addAction(std::make_unique<InsertLayerUndoAction>(control->getLayerController(), page, layer, position));
    }
    result.layer = layerIndex;
    result.layerName = layerName(layer, layerIndex);

    // Insert (strokes start with 2 points when animated)
    auto anim = std::make_unique<Animation>();
    anim->control = control;
    anim->page = page;
    {
        std::unique_lock lock(*doc);
        for (auto& e: elements) {
            const Element* raw = e.get();
            if (animation.enabled && e->getType() == ELEMENT_STROKE) {
                auto* s = static_cast<Stroke*>(e.get());
                if (s->getPointCount() > 2) {
                    auto full = s->getPointVector();
                    s->setPointVector(std::vector<Point>(full.begin(), full.begin() + 2));
                    anim->strokes.emplace_back(s, std::move(full));
                }
            }
            layer->addElement(std::move(e));
            result.elements.push_back(raw);
            ElementIds::get().setOrigin(raw, result.operation);
            ElementIds::get().idOf(raw);
        }
    }
    group->addAction(std::make_unique<InsertsUndoAction>(page, layer, result.elements));
    if (result.layerCreated) {
        control->getLayerController()->fireRebuildLayerMenu();
    }

    auto holder = std::make_shared<std::unique_ptr<GroupUndoAction>>(std::move(group));
    auto finish = [control = control, page, holder, result, done = std::move(done)](bool completed) {
        if (completed) {
            auto bb = unionOf(result.elements);
            page->fireRectChanged(bb);
            control->getUndoRedoHandler()->addUndoAction(std::move(*holder));
        }
        done(result);
    };

    if (anim->strokes.empty()) {
        finish(true);
        return;
    }
    // Pace: total points spread over the requested duration, at 60 ticks per second
    size_t totalPoints = 0;
    for (const auto& [s, full]: anim->strokes) {
        totalPoints += full.size();
    }
    // ~400 points per second at speed 1 (points are about 1 pt apart: hand speed)
    const double seconds = std::min(animation.maxSeconds,
                                    static_cast<double>(totalPoints) / (400.0 * std::max(animation.speed, 0.05)));
    const double ticks = std::max(1.0, seconds * 60.0);
    anim->pointsPerTick = std::max<size_t>(1, static_cast<size_t>(std::ceil(static_cast<double>(totalPoints) / ticks)));
    anim->finish = std::move(finish);
    g_timeout_add(16, animationTick, anim.release());
}

}  // namespace xoj::api
