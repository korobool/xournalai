/*
 * xournalai (based on Xournal++)
 *
 * Live notes: cuts the sound being recorded into pieces for speech recognition, as it comes. A piece ends
 *  - at a pause (no voice for `pauseMs`, 1.5 s),
 *  - in a monologue, at the first short pause (`microPauseMs`) once it is `softMs` long (15 s),
 *  - at the quietest moment of its last 2 s when it reaches `hardMs` (25 s: Whisper hears 30 s at a time),
 *  - when asked (✂: at the quietest moment of the last 0.5 s), and at the end of the recording.
 * Voice is told from the microphone's own noise floor (VoiceActivity), so a quiet built-in microphone works too.
 * Silence before the voice is dropped (a piece of silence is never sent). Pieces are mono 16 kHz, 16-bit.
 *
 * GTK-free; fed on the recorder's thread, `cut` from any thread.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>      // for atomic
#include <cstdint>     // for int64_t, int16_t
#include <functional>  // for function
#include <mutex>       // for mutex
#include <string>      // for string
#include <vector>      // for vector

#include "assistant/VoiceActivity.h"  // for VoiceActivity

namespace xoj::assistant {

struct LiveChunk {
    int index = 0;           ///< 1, 2, … in the recording
    int64_t startMs = 0;     ///< where it starts in the recording
    int64_t endMs = 0;       ///< where it ends
    std::string reason;      ///< "pause", "long" (a short pause after softMs), "limit" (hardMs), "cut" (✂), "end"
    std::vector<int16_t> pcm;  ///< mono, RATE
};

class LiveChunker final {
public:
    static constexpr int RATE = 16000;
    static constexpr int FRAME = RATE / 20;  ///< 50 ms: the unit of the voice decision

    struct Options {
        int pauseMs = 1500;
        int softMs = 15000;
        int hardMs = 25000;
        int microPauseMs = 300;
        int minVoiceMs = 300;  ///< less voice than this in a piece: nothing to recognize (dropped)
    };
    using OnChunk = std::function<void(LiveChunk&&)>;  ///< called on the feeding thread

    LiveChunker(Options options, OnChunk onChunk);

    /// Interleaved float frames (any rate, any channels)
    void feed(const float* frames, size_t count, int channels, int sampleRate);
    /// Mono 16 kHz samples (tests, files)
    void feedMono16k(const float* samples, size_t count);
    /// ✂: ends the piece now (at the quietest moment of the last 0.5 s), if it holds voice
    void cut() { cutWanted = true; }
    /// The recording ended: the rest, if it holds voice
    void finish();

    int64_t heardMs() const;  ///< how much sound came so far

private:
    void frame(const float* s);  ///< one FRAME of mono samples
    void emit(size_t frames, const char* reason);
    size_t quietestFrame(size_t lookback) const;  ///< (index into frameRms, from the end back `lookback` frames)
    void dropLeadingSilence();

    Options opt;
    OnChunk onChunk;
    std::mutex mutex;
    std::atomic<bool> cutWanted{false};
    VoiceActivity voice;

    // resampling to RATE (linear)
    double srcPos = 0;      ///< where the next output sample is, in input samples after `prev`
    float prev = 0;         ///< the last input sample (mono)
    bool havePrev = false;
    std::vector<float> pending;  ///< mono RATE samples not yet a whole frame

    // the piece being built
    std::vector<int16_t> pcm;
    std::vector<float> frameRms;
    std::vector<bool> frameVoice;
    int64_t startSample = 0;  ///< of `pcm[0]`, in the recording
    std::atomic<int64_t> totalSamples{0};  ///< RATE samples heard (read on the UI thread: heardMs)
    size_t voiceFrames = 0;
    size_t silentRun = 0;  ///< frames without voice at the end
    int nextIndex = 1;
};

}  // namespace xoj::assistant
