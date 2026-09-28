/*
 * Xournal++ (xournalai)
 *
 * Inserting agent-created content into the document: target layer, attribution, undo and animation
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>     // for size_t
#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include "model/Element.h"  // for ElementPtr

#include "Pressure.h"  // for PressureSettings

class Control;
class Layer;

namespace xoj::api {

struct DrawTarget {
    size_t page = 0;           ///< 0-based
    std::string layer = "AI";  ///< "AI" (created on top if missing), "current", a layer name or "#<n>" (1-based)
    bool createLayer = true;   ///< create a named layer that doesn't exist yet
};

struct DrawResult {
    std::string operation;  ///< "op<n>", attributed to every inserted element
    size_t page = 0;        ///< 0-based
    size_t layer = 0;       ///< 1-based
    std::string layerName;
    bool layerCreated = false;
    std::vector<const Element*> elements;
};

struct AnimationOptions {
    bool enabled = false;
    double speed = 1.0;       ///< 1 = roughly hand speed (~400 points per second), higher is faster
    double maxSeconds = 6.0;  ///< the whole animation never takes longer than this
};

/// Inserts elements for agents. All functions must run on the main thread.
class DrawApi {
public:
    explicit DrawApi(Control* control): control(control) {}

    /**
     * @brief Inserts elements into the target layer as ONE undo step (including a newly created layer).
     * The user's selected layer and tool are not changed. Throws std::invalid_argument for bad targets.
     * With animation, strokes grow point by point and `done` is called when finished (immediately otherwise).
     */
    void insert(const DrawTarget& target, std::vector<ElementPtr> elements, const AnimationOptions& animation,
                std::function<void(const DrawResult&)> done);

    /// The user's pressure settings (minimum pressure, multiplier, pressure sensitivity)
    PressureSettings pressureSettings() const;

    /// Next operation id ("op1", "op2", ...)
    static std::string nextOperation();

private:
    Control* control;
};

}  // namespace xoj::api
