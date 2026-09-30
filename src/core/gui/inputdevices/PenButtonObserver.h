/*
 * xournalai (based on Xournal++)
 *
 * Lets the assistant watch the pen's first barrel button without taking it over: while it is held, the assistant
 * listens (Ask), and the strokes drawn meanwhile (with the button's usual tool, e.g. the lasso selection) tell it
 * where. The input system reports; it never waits for, or changes, what the observer does.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function

#include "model/PageRef.h"  // for PageRef

namespace xoj::input {

struct PenButtonEvent {
    enum Kind { Down, Point, Up };
    Kind kind;
    PageRef page;  ///< the page under the pen (null if none)
    double x = 0;  ///< page coordinates (points)
    double y = 0;
    bool tipDown = false;  ///< the pen touches the surface (Point: a lasso point)
};

using PenButtonObserver = std::function<void(const PenButtonEvent&)>;

/// One observer (the assistant); null to stop observing
void setPenButtonObserver(PenButtonObserver observer);
const PenButtonObserver& penButtonObserver();

}  // namespace xoj::input
