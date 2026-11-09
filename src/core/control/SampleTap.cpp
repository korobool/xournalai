#include "SampleTap.h"

#include <mutex>    // for mutex, lock_guard
#include <utility>  // for move

namespace xoj::audio {

namespace {
std::mutex& tapMutex() {
    static std::mutex m;
    return m;
}
SampleTap& current() {
    static SampleTap t;
    return t;
}
}  // namespace

void setSampleTap(SampleTap tap) {
    std::lock_guard lock(tapMutex());
    current() = std::move(tap);
}

void tapSamples(const float* frames, size_t count, int channels, int sampleRate) {
    std::lock_guard lock(tapMutex());  // (set and cleared on the UI thread, called on the recorder's)
    if (current() && count > 0) {
        current()(frames, count, channels, sampleRate);
    }
}

}  // namespace xoj::audio
