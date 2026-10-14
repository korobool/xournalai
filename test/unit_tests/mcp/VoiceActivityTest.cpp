#include "config-features.h"

#ifdef ENABLE_MCP

#include <gtest/gtest.h>

#include "assistant/VoiceActivity.h"

using namespace xoj::assistant;

TEST(VoiceActivity, quietBuiltInMicrophoneSpeechIsVoice) {
    // the owner's laptop: noise ~0.001, speech ~0.008 … 0.03 (above the old fixed 0.03 almost never)
    VoiceActivity va;
    for (int i = 0; i < 60; i++) {
        va.push(0.001F);
    }
    EXPECT_TRUE(va.push(0.008F));
    EXPECT_TRUE(va.push(0.012F));
    EXPECT_FALSE(va.push(0.0015F));
}

TEST(VoiceActivity, aNoisyRoomNeedsLouderSpeech) {
    VoiceActivity va;
    for (int i = 0; i < 60; i++) {
        va.push(0.004F);
    }
    EXPECT_FALSE(va.push(0.009F));  // 3x the floor: 0.012
    EXPECT_TRUE(va.push(0.02F));
    for (int i = 0; i < 200; i++) {
        va.push(0.05F);  // very loud floor: capped, loud speech still counts
    }
    EXPECT_FLOAT_EQ(va.threshold(), VoiceActivity::MAX_THRESHOLD);
}

TEST(VoiceActivity, beforeTheFloorIsKnownTheLowestThreshold) {
    VoiceActivity va;
    EXPECT_TRUE(va.push(0.006F));
    EXPECT_FALSE(va.push(0.004F));
}

#endif
