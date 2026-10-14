/*
 * xournalai (based on Xournal++)
 *
 * Whether the microphone hears someone talking, from its level stream: a level counts as voice when it stands out
 * from the microphone's own noise floor (the quietest levels of the last seconds), so a quiet built-in microphone
 * (speech at ~0.01 RMS) is heard as well as a loud headset.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <deque>  // for deque

namespace xoj::assistant {

class VoiceActivity final {
public:
    /// Adds a level (RMS, 0 … 1); returns whether it is voice
    bool push(float rms);
    /// The level above which a level is voice now
    float threshold() const;
    void reset() { levels.clear(); }

    static constexpr float MIN_THRESHOLD = 0.005F;  ///< (the speech helper's own silence check uses the same bounds)
    static constexpr float MAX_THRESHOLD = 0.02F;

private:
    std::deque<float> levels;  ///< the last ~10 s (the helper sends ~20 levels a second)
};

}  // namespace xoj::assistant
