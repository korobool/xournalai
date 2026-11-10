/*
 * xournalai (based on Xournal++)
 *
 * «Держи и говори» (hold and talk, the owner's wish 2026-11-09): while the button is held, the microphone is
 * recorded on its own (whatever the Record button does), as 16 kHz mono for the speech recognizer. The selected input
 * device of the audio settings, else the default one. feed() takes sound from elsewhere too (tests).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for int16_t
#include <memory>   // for unique_ptr
#include <mutex>    // for mutex
#include <string>   // for string
#include <vector>   // for vector

#include "config-features.h"  // for ENABLE_AUDIO

#ifdef ENABLE_AUDIO
#include <portaudiocpp/PortAudioCpp.hxx>  // for MemFunCallbackStream, System
#endif

class Settings;

namespace xoj::audio {

class TalkCapture final {
public:
    explicit TalkCapture(Settings* settings);
    ~TalkCapture();
    TalkCapture(const TalkCapture&) = delete;
    TalkCapture& operator=(const TalkCapture&) = delete;

    /// Opens the microphone (`withDevice`; false: only what feed() gets); false with `error` if it can't
    bool start(bool withDevice, std::string& error);
    /// Ends it: the samples, 16 kHz mono
    std::vector<int16_t> stop();
    bool active() const { return running; }
    /// Sound in, any rate and channels (interleaved floats)
    void feed(const float* frames, size_t count, int channels, int sampleRate);

private:
    Settings* settings;
    bool running = false;
    std::mutex mutex;
    std::vector<float> mono;  ///< 16 kHz
    float prev = 0;
    bool havePrev = false;
    double srcPos = 0;
#ifdef ENABLE_AUDIO
    int callback(const void* in, void* out, unsigned long frames, const PaStreamCallbackTimeInfo* time,
                 PaStreamCallbackFlags flags);
    std::unique_ptr<portaudio::MemFunCallbackStream<TalkCapture>> stream;
    int channels = 1;
    int rate = 16000;
#endif
};

}  // namespace xoj::audio
