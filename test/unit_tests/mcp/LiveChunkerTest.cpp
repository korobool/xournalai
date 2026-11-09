#include "config-features.h"

#ifdef ENABLE_MCP

#include <cmath>   // for sin
#include <vector>  // for vector

#include <gtest/gtest.h>

#include "assistant/LiveChunker.h"

using namespace xoj::assistant;

namespace {
constexpr int R = LiveChunker::RATE;

/// A quiet built-in microphone: noise ~0.001 RMS, "voice" a tone of 0.1
struct Sound {
    std::vector<float> s;
    unsigned seed = 1;
    Sound& noise(double sec) {
        for (int i = 0; i < static_cast<int>(sec * R); i++) {
            seed = seed * 1103515245 + 12345;
            s.push_back((static_cast<float>((seed >> 16) & 0x7fff) / 32767.0F - 0.5F) * 0.0035F);
        }
        return *this;
    }
    Sound& voice(double sec) {
        const size_t start = s.size();
        for (int i = 0; i < static_cast<int>(sec * R); i++) {
            s.push_back(0.1F * static_cast<float>(std::sin(2 * M_PI * 220 * static_cast<double>(start + i) / R)));
        }
        return *this;
    }
};

struct Pieces {
    std::vector<LiveChunk> chunks;
    LiveChunker chunker;
    explicit Pieces(LiveChunker::Options o = {}): chunker(o, [this](LiveChunk&& c) { chunks.push_back(std::move(c)); }) {}
    void feed(const Sound& snd, size_t block = 512) {
        for (size_t i = 0; i < snd.s.size(); i += block) {
            chunker.feedMono16k(snd.s.data() + i, std::min(block, snd.s.size() - i));
        }
    }
};
}  // namespace

TEST(LiveChunker, aPauseEndsAPiece) {
    Pieces r;
    r.feed(Sound().noise(1).voice(3).noise(2).voice(2).noise(2));
    r.chunker.finish();
    ASSERT_EQ(r.chunks.size(), 2U);
    EXPECT_EQ(r.chunks[0].reason, "pause");
    EXPECT_EQ(r.chunks[0].index, 1);
    EXPECT_NEAR(r.chunks[0].startMs, 700, 120);  // (the silence before is dropped, but for 300 ms)
    EXPECT_NEAR(r.chunks[0].endMs, 4300, 120);   // (300 ms of the pause stay with it)
    EXPECT_EQ(r.chunks[0].pcm.size(), static_cast<size_t>((r.chunks[0].endMs - r.chunks[0].startMs) * R / 1000));
    EXPECT_EQ(r.chunks[1].reason, "pause");
    EXPECT_NEAR(r.chunks[1].startMs, 5700, 120);
    EXPECT_NEAR(r.chunks[1].endMs, 8300, 120);
}

TEST(LiveChunker, silenceAndClicksAreNeverSent) {
    Pieces r;
    r.feed(Sound().noise(5).voice(0.1).noise(3));
    r.chunker.cut();
    r.feed(Sound().noise(1));
    r.chunker.finish();
    EXPECT_TRUE(r.chunks.empty());
}

TEST(LiveChunker, aMonologueIsCutAtAShortPauseAfterTheSoftLength) {
    Pieces r;
    Sound snd;
    snd.noise(0.5);
    for (int i = 0; i < 8; i++) {  // 4 s of voice, 0.4 s breath: no 1.5 s pause at all
        snd.voice(4).noise(0.4);
    }
    r.feed(snd);
    ASSERT_GE(r.chunks.size(), 1U);
    EXPECT_EQ(r.chunks[0].reason, "long");
    const int64_t len = r.chunks[0].endMs - r.chunks[0].startMs;
    EXPECT_GE(len, 15000);
    EXPECT_LE(len, 20000);
}

TEST(LiveChunker, endlessVoiceIsCutAtTheLimit) {
    Pieces r;
    r.feed(Sound().noise(0.5).voice(40));
    ASSERT_GE(r.chunks.size(), 1U);
    EXPECT_EQ(r.chunks[0].reason, "limit");
    EXPECT_LE(r.chunks[0].endMs - r.chunks[0].startMs, 25000);
    EXPECT_GE(r.chunks[0].endMs - r.chunks[0].startMs, 23000);
}

TEST(LiveChunker, theScissorsCutNow) {
    Pieces r;
    r.feed(Sound().noise(1).voice(3));
    r.chunker.cut();
    r.feed(Sound().voice(0.2));  // (the cut happens with the next sound)
    ASSERT_EQ(r.chunks.size(), 1U);
    EXPECT_EQ(r.chunks[0].reason, "cut");
    EXPECT_NEAR(r.chunks[0].endMs, 4000, 600);
    r.feed(Sound().voice(2));
    r.chunker.finish();
    ASSERT_EQ(r.chunks.size(), 2U);
    EXPECT_EQ(r.chunks[1].reason, "end");
    EXPECT_EQ(r.chunks[1].index, 2);
}

TEST(LiveChunker, anyRateAndChannelsKeepTheTime) {
    Pieces r;
    Sound snd;
    snd.noise(1).voice(3).noise(2);
    // the same sound at 48 kHz, stereo
    std::vector<float> st;
    for (size_t i = 0; i < snd.s.size(); i++) {
        for (int k = 0; k < 3; k++) {
            st.push_back(snd.s[i]);
            st.push_back(snd.s[i]);
        }
    }
    for (size_t i = 0; i < st.size() / 2; i += 1024) {
        r.chunker.feed(st.data() + 2 * i, std::min<size_t>(1024, st.size() / 2 - i), 2, 48000);
    }
    EXPECT_NEAR(r.chunker.heardMs(), 6000, 60);
    ASSERT_EQ(r.chunks.size(), 1U);
    EXPECT_NEAR(r.chunks[0].endMs, 4300, 120);
}

#endif
