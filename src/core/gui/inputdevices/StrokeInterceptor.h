/*
 * xournalai (based on Xournal++)
 *
 * While the assistant's Ask lasso is armed, the next pen or mouse stroke draws a lasso for it instead of using the
 * current tool. The interceptor gets the stroke in page coordinates and consumes it; touch (scrolling, zooming) is
 * never intercepted.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function

#include "model/PageRef.h"  // for PageRef

namespace xoj::input {

struct InterceptedStroke {
    enum Kind { Down, Move, Up, Cancel };
    Kind kind;
    PageRef page;  ///< the page under the pointer (null if none)
    double x = 0;  ///< page coordinates (points)
    double y = 0;
};

/// Returns true if it consumed the event (the current tool then does not see it)
using StrokeInterceptor = std::function<bool(const InterceptedStroke&)>;

void setStrokeInterceptor(StrokeInterceptor interceptor);  ///< null: no interception
const StrokeInterceptor& strokeInterceptor();

}  // namespace xoj::input
