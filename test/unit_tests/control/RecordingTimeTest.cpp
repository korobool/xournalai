#include <gtest/gtest.h>

#include "control/RecordingObserver.h"

using xoj::audio::recordingTimeMs;

// A stroke's moment in a recording counts from the file's first sample, not from the click on Record: opening the
// microphone takes a while, and counting from the click would put every stroke late in the audio.
TEST(RecordingTime, countsFromTheFirstCapturedSample) {
    const int64_t clickMs = 10'000;       // Record pressed
    const int64_t firstUs = 10'350'000;   // the microphone delivered its first sample 350 ms later
    const int64_t strokeUs = 12'350'000;  // written two seconds into the audio
    EXPECT_EQ(recordingTimeMs(strokeUs, firstUs, clickMs), 2000);
}

TEST(RecordingTime, beforeTheFirstSampleItCountsFromTheClick) {
    EXPECT_EQ(recordingTimeMs(10'120'000, 0, 10'000), 120);
}

TEST(RecordingTime, neverNegative) {
    EXPECT_EQ(recordingTimeMs(10'000'000, 10'350'000, 10'000), 0);
    EXPECT_EQ(recordingTimeMs(9'000'000, 0, 10'000), 0);
}
