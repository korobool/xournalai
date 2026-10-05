#include "PenButtonObserver.h"

#include <utility>  // for move

namespace xoj::input {

namespace {
PenButtonObserver& instance() {
    static PenButtonObserver o;
    return o;
}
}  // namespace

std::function<void()>& sink() {
    static std::function<void()> s;
    return s;
}

void setPenButtonObserver(PenButtonObserver observer) { instance() = std::move(observer); }

void setPenBarrelReleaseSink(std::function<void()> s) { sink() = std::move(s); }

void penBarrelGone() {
    if (sink()) {
        sink()();
    }
}

const PenButtonObserver& penButtonObserver() { return instance(); }

}  // namespace xoj::input
