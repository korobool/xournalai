#include "VoiceActivity.h"

#include <algorithm>  // for clamp, nth_element
#include <vector>     // for vector

namespace xoj::assistant {

namespace {
constexpr size_t WINDOW = 200;
constexpr size_t ENOUGH = 20;  ///< before this many levels, the floor isn't known: the lowest threshold
}  // namespace

float VoiceActivity::threshold() const {
    if (levels.size() < ENOUGH) {
        return MIN_THRESHOLD;
    }
    std::vector<float> v(levels.begin(), levels.end());
    auto tenth = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 10);
    std::nth_element(v.begin(), tenth, v.end());
    return std::clamp(3 * *tenth, MIN_THRESHOLD, MAX_THRESHOLD);
}

bool VoiceActivity::push(float rms) {
    levels.push_back(rms);
    if (levels.size() > WINDOW) {
        levels.pop_front();
    }
    return rms > threshold();
}

}  // namespace xoj::assistant
