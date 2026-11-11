#include <cmath>
#include <random>

#include <gtest/gtest.h>

#include "assistant/SpeakerTracker.h"

using xoj::assistant::SpeakerTracker;
using xoj::assistant::Voice;

namespace {
/// A voice: a fixed direction; each stretch of it a little noisy (cosine ~0.7–0.9 to the voice)
Voice stretch(int voice, int take, double noise = 0.35) {
    std::mt19937 rnd(static_cast<unsigned>(voice * 1000 + take));
    std::mt19937 base(static_cast<unsigned>(voice));
    std::normal_distribution<float> n(0, 1);
    Voice v(192);
    double norm = 0;
    for (auto& x: v) {
        x = n(base);
    }
    for (auto& x: v) {
        x += static_cast<float>(noise) * n(rnd) * 1.0f;
        norm += static_cast<double>(x) * x;
    }
    for (auto& x: v) {
        x = static_cast<float>(x / std::sqrt(norm));
    }
    return v;
}
}  // namespace

// «👥 Speakers» (1.23.0): the voices are named in the order they first speak, and each keeps its name
TEST(SpeakerTracker, twoVoicesKeepTheirLetters) {
    SpeakerTracker t;
    EXPECT_EQ(t.assign(stretch(1, 1), 3.0), "A");
    EXPECT_EQ(t.assign(stretch(2, 1), 4.0), "B");
    EXPECT_EQ(t.assign(stretch(1, 2), 2.0), "A");
    EXPECT_EQ(t.assign(stretch(2, 2), 6.0), "B");
    EXPECT_EQ(t.assign(stretch(1, 3), 1.6), "A");
    EXPECT_EQ(t.labels(), (std::vector<std::string>{"A", "B"}));
}

TEST(SpeakerTracker, aShortUnknownStretchIsAQuestionMarkNotANewVoice) {
    SpeakerTracker t;
    EXPECT_EQ(t.assign(stretch(1, 1), 3.0), "A");
    EXPECT_EQ(t.assign(stretch(2, 1), 0.8), "?");  // (too short to be a new voice)
    EXPECT_EQ(t.labels().size(), 1u);
    EXPECT_EQ(t.assign(stretch(1, 2), 0.7), "A");  // (short, but clearly A)
}

TEST(SpeakerTracker, noFingerprintIsAQuestionMark) {
    SpeakerTracker t;
    EXPECT_EQ(t.assign({}, 5.0), "?");
    EXPECT_TRUE(t.labels().empty());
}

TEST(SpeakerTracker, anotherModelsFingerprintsAreNeverCompared) {
    SpeakerTracker t;
    EXPECT_EQ(t.assign(stretch(1, 1), 3.0), "A");
    Voice other(256, 0.0625f);  // (another length: another model)
    EXPECT_EQ(t.assign(other, 3.0), "B");
}

TEST(SpeakerTracker, labelsAfterZ) {
    EXPECT_EQ(SpeakerTracker::labelOf(0), "A");
    EXPECT_EQ(SpeakerTracker::labelOf(25), "Z");
    EXPECT_EQ(SpeakerTracker::labelOf(26), "V27");
}

TEST(SpeakerTracker, similarity) {
    const Voice a = stretch(1, 1);
    EXPECT_NEAR(SpeakerTracker::similarity(a, a), 1.0, 1e-6);
    EXPECT_LT(SpeakerTracker::similarity(stretch(1, 1), stretch(2, 1)), 0.3);
    EXPECT_GT(SpeakerTracker::similarity(stretch(1, 1), stretch(1, 2)), 0.7);
    EXPECT_EQ(SpeakerTracker::similarity({}, a), 0.0);
}

// «🎤 Only my voice» (1.24.0): the owner's own voice, when known, is «Me»; the others keep their letters
TEST(SpeakerTracker, theOwnersVoiceIsMe) {
    SpeakerTracker t;
    EXPECT_FALSE(t.ownerKnown());
    t.setOwner(stretch(1, 99));
    EXPECT_TRUE(t.ownerKnown());
    EXPECT_EQ(t.assign(stretch(2, 1), 3.0), "A");
    EXPECT_EQ(t.assign(stretch(1, 1), 3.0), "Me");
    EXPECT_EQ(t.assign(stretch(1, 2), 0.6), "Me");  // (short, but clearly him)
    EXPECT_EQ(t.assign(stretch(2, 2), 2.0), "A");
    EXPECT_EQ(t.labels(), (std::vector<std::string>{"Me", "A"}));
}
