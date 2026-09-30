#include "PenButtonObserver.h"

#include <utility>  // for move

namespace xoj::input {

namespace {
PenButtonObserver& instance() {
    static PenButtonObserver o;
    return o;
}
}  // namespace

void setPenButtonObserver(PenButtonObserver observer) { instance() = std::move(observer); }

const PenButtonObserver& penButtonObserver() { return instance(); }

}  // namespace xoj::input
