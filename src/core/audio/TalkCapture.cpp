#include "TalkCapture.h"

#include <algorithm>  // for clamp

#include <glib.h>  // for g_message

#include "control/settings/Settings.h"  // for Settings

namespace xoj::audio {

namespace {
constexpr int RATE = 16000;
}

TalkCapture::TalkCapture(Settings* settings): settings(settings) {}

TalkCapture::~TalkCapture() { stop(); }

bool TalkCapture::start(bool withDevice, std::string& error) {
    stop();
    {
        std::lock_guard lock(mutex);
        mono.clear();
        havePrev = false;
        srcPos = 0;
    }
#ifdef ENABLE_AUDIO
    if (withDevice) {
        try {
            portaudio::System& sys = portaudio::System::instance();
            portaudio::Device* device = nullptr;
            const PaDeviceIndex idx = settings ? settings->getAudioInputDevice() : -1;
            try {
                device = idx == -1 ? &sys.defaultInputDevice() : &sys.deviceByIndex(idx);
            } catch (const portaudio::PaException&) {
                device = &sys.defaultInputDevice();
            }
            channels = std::clamp(device->maxInputChannels(), 1, 2);
            rate = settings ? static_cast<int>(settings->getAudioSampleRate()) : 44100;
            portaudio::DirectionSpecificStreamParameters in(*device, channels, portaudio::FLOAT32, true,
                                                            device->defaultLowInputLatency(), nullptr);
            portaudio::StreamParameters params(in, portaudio::DirectionSpecificStreamParameters::null(), rate, 512,
                                               paNoFlag);
            stream = std::make_unique<portaudio::MemFunCallbackStream<TalkCapture>>(params, *this,
                                                                                    &TalkCapture::callback);
            stream->start();
        } catch (const std::exception& e) {
            stream.reset();
            error = std::string("the microphone could not be opened: ") + e.what();
            return false;
        }
    }
#else
    if (withDevice) {
        error = "built without audio";
        return false;
    }
#endif
    running = true;
    return true;
}

std::vector<int16_t> TalkCapture::stop() {
#ifdef ENABLE_AUDIO
    if (stream) {
        try {
            stream->stop();
            stream->close();
        } catch (const std::exception&) {
        }
        stream.reset();
    }
#endif
    std::vector<int16_t> out;
    if (!running) {
        return out;
    }
    running = false;
    std::lock_guard lock(mutex);
    out.reserve(mono.size());
    for (float s: mono) {
        out.push_back(static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f));
    }
    mono.clear();
    return out;
}

void TalkCapture::feed(const float* in, size_t count, int ch, int sampleRate) {
    if (ch <= 0 || sampleRate <= 0) {
        return;
    }
    std::lock_guard lock(mutex);
    if (mono.size() > static_cast<size_t>(RATE) * 120) {
        return;  // (two minutes are enough for a command)
    }
    // downmixed and resampled to 16 kHz (linear), as the live notes' chunker does
    const double step = static_cast<double>(sampleRate) / RATE;
    for (size_t i = 0; i < count; i++) {
        float m = 0;
        for (int c = 0; c < ch; c++) {
            m += in[i * static_cast<size_t>(ch) + static_cast<size_t>(c)];
        }
        m /= static_cast<float>(ch);
        if (!havePrev) {
            prev = m;
            havePrev = true;
            mono.push_back(m);
            srcPos = step;
            continue;
        }
        while (srcPos <= 1.0) {
            mono.push_back(prev + static_cast<float>(srcPos) * (m - prev));
            srcPos += step;
        }
        srcPos -= 1.0;
        prev = m;
    }
}

#ifdef ENABLE_AUDIO
int TalkCapture::callback(const void* in, void*, unsigned long frames, const PaStreamCallbackTimeInfo*,
                          PaStreamCallbackFlags) {
    if (in) {
        feed(static_cast<const float*>(in), frames, channels, rate);
    }
    return paContinue;
}
#endif

}  // namespace xoj::audio
