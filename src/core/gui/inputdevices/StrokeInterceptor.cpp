#include "StrokeInterceptor.h"

#include <utility>  // for move

namespace xoj::input {

namespace {
StrokeInterceptor& instance() {
    static StrokeInterceptor i;
    return i;
}
}  // namespace

void setStrokeInterceptor(StrokeInterceptor interceptor) { instance() = std::move(interceptor); }

const StrokeInterceptor& strokeInterceptor() { return instance(); }

}  // namespace xoj::input
