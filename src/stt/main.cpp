/*
 * xournalai (based on Xournal++)
 *
 * xournalai-stt: local speech to text for the assistant's Ask (hold the pen button and speak).
 *
 * Loads a whisper.cpp model once and waits for commands, one JSON object per line on stdin; answers the same way on
 * stdout:
 *   {"cmd":"start"}                  → {"event":"listening"}          (records from the default microphone)
 *   {"cmd":"stop"}                   → {"event":"text","text":"…","silent":false,"audio_ms":…,"ms":…}
 *   {"cmd":"cancel"}                 → {"event":"cancelled"}
 *   {"cmd":"transcribe","wav":"…"}   → {"event":"text",…}             (a 16-bit PCM WAV file)
 *   {"cmd":"quit"}
 * At start: {"event":"ready","model":"…","load_ms":…}; on problems: {"event":"error","message":"…"}.
 *
 * Silence (no speech in the recording) gives "silent":true and no text, so that holding the button without speaking
 * changes nothing. XOURNALAI_STT_FAKE_MIC=<wav> makes "start" record that file instead of the microphone (tests).
 *
 *   xournalai-stt --model ~/.local/share/xournalai/models/ggml-base.en.bin [--threads 4]
 *
 * @license GNU GPLv2 or later
 */

#include <algorithm>  // for sort, max, min, count_if
#include <atomic>     // for atomic
#include <cctype>     // for isalnum, tolower
#include <chrono>     // for steady_clock
#include <cmath>      // for sqrt
#include <cstdint>    // for int16_t
#include <cstdio>     // for printf
#include <cstdlib>    // for getenv
#include <cstring>    // for memcpy
#include <fstream>    // for ifstream
#include <iostream>   // for cin
#include <iterator>   // for istreambuf_iterator
#include <mutex>      // for mutex
#include <string>     // for string
#include <thread>     // for hardware_concurrency
#include <vector>     // for vector

#include <nlohmann/json.hpp>
#include <portaudio.h>
#include <whisper.h>

using json = nlohmann::json;

namespace {

constexpr int RATE = WHISPER_SAMPLE_RATE;  // 16 kHz

std::mutex outMutex;  // the main loop and the level thread both write

void emit(const json& j) {
    const std::string line = j.dump() + "\n";
    std::lock_guard g(outMutex);
    fwrite(line.data(), 1, line.size(), stdout);
    fflush(stdout);
}

void error(const std::string& message) { emit({{"event", "error"}, {"message", message}}); }

/// Mono float samples at 16 kHz from a 16-bit PCM WAV file (any rate / channel count); empty on failure
std::vector<float> readWav(const std::string& path, std::string& why) {
    std::ifstream in(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto u16 = [&](size_t o) {
        return static_cast<uint16_t>(static_cast<uint8_t>(bytes[o]) | (static_cast<uint8_t>(bytes[o + 1]) << 8));
    };
    auto u32 = [&](size_t o) { return static_cast<uint32_t>(u16(o) | (static_cast<uint32_t>(u16(o + 2)) << 16)); };
    if (bytes.size() < 12 || std::string(bytes.data(), 4) != "RIFF" || std::string(bytes.data() + 8, 4) != "WAVE") {
        why = "not a WAV file";
        return {};
    }
    int channels = 0, rate = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= bytes.size()) {
        const std::string id(bytes.data() + pos, 4);
        const size_t len = u32(pos + 4);
        const size_t body = pos + 8;
        if (id == "fmt " && body + 16 <= bytes.size()) {
            channels = u16(body + 2);
            rate = static_cast<int>(u32(body + 4));
            bits = u16(body + 14);
        } else if (id == "data") {
            if (bits != 16 || channels < 1 || rate < 8000) {
                why = "only 16-bit PCM WAV is supported";
                return {};
            }
            const size_t frames = std::min(len, bytes.size() - body) / (2 * static_cast<size_t>(channels));
            std::vector<float> mono(frames);
            for (size_t f = 0; f < frames; f++) {
                float sum = 0;
                for (int c = 0; c < channels; c++) {
                    sum += static_cast<int16_t>(u16(body + 2 * (f * static_cast<size_t>(channels) + c))) / 32768.0f;
                }
                mono[f] = sum / static_cast<float>(channels);
            }
            if (rate == RATE) {
                return mono;
            }
            // Linear resampling to 16 kHz
            const double step = static_cast<double>(rate) / RATE;
            std::vector<float> out(static_cast<size_t>(static_cast<double>(frames) / step));
            for (size_t i = 0; i < out.size(); i++) {
                const double src = static_cast<double>(i) * step;
                const auto k = static_cast<size_t>(src);
                const double t = src - static_cast<double>(k);
                out[i] = static_cast<float>(mono[k] * (1 - t) + (k + 1 < frames ? mono[k + 1] : mono[k]) * t);
            }
            return out;
        }
        pos = body + len + (len & 1);
    }
    why = "no audio data in the WAV file";
    return {};
}

/// Whether the recording contains speech: enough 20 ms frames clearly louder than the background
bool hasSpeech(const std::vector<float>& pcm) {
    constexpr size_t FRAME = RATE / 50;
    std::vector<float> rms;
    for (size_t i = 0; i + FRAME <= pcm.size(); i += FRAME) {
        double sum = 0;
        for (size_t k = i; k < i + FRAME; k++) {
            sum += static_cast<double>(pcm[k]) * pcm[k];
        }
        rms.push_back(static_cast<float>(std::sqrt(sum / FRAME)));
    }
    if (rms.size() < 10) {  // under 0.2 s
        return false;
    }
    std::vector<float> sorted = rms;
    std::sort(sorted.begin(), sorted.end());
    const float noise = sorted[sorted.size() / 5];
    const float threshold = std::max(0.012f, noise * 3.0f);
    const auto loud = std::count_if(rms.begin(), rms.end(), [&](float r) { return r > threshold; });
    return loud >= 12;  // at least ~0.25 s of voice
}

/// Whisper's words for silence or noise (it sometimes "hears" these in quiet recordings)
bool isNonSpeech(const std::string& text) {
    std::string t;
    for (char c: text) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return t.empty() || t == "blankaudio" || t == "silence" || t == "music" || t == "inaudible" || t == "noise";
}

class Recorder {
public:
    ~Recorder() {
        stop();
        if (initialized) {
            Pa_Terminate();
        }
    }

    bool start(std::string& why) {
        std::lock_guard g(m);
        pcm.clear();
        if (const char* fake = getenv("XOURNALAI_STT_FAKE_MIC")) {
            pcm = readWav(fake, why);
            // "plays" in real time for the level meter
            fakeStart = std::chrono::steady_clock::now();
            fakeRead = 0;
            fakeLive = why.empty();
            return why.empty();
        }
        if (!initialized) {
            if (Pa_Initialize() != paNoError) {
                why = "no audio system";
                return false;
            }
            initialized = true;
        }
        // 16 kHz if the device can; else 48 kHz, decimated by 3
        for (int rate: {RATE, 48000}) {
            if (Pa_OpenDefaultStream(&stream, 1, 0, paFloat32, rate, paFramesPerBufferUnspecified, &Recorder::onAudio,
                                     this) == paNoError) {
                decimate = rate / RATE;
                if (Pa_StartStream(stream) == paNoError) {
                    live = true;
                    return true;
                }
                Pa_CloseStream(stream);
                stream = nullptr;
            }
        }
        why = "could not open the microphone";
        return false;
    }

    std::vector<float> stop() {
        live = false;
        fakeLive = false;
        if (stream) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            stream = nullptr;
        }
        std::lock_guard g(m);
        return std::move(pcm);
    }

private:
    static int onAudio(const void* input, void*, unsigned long frames, const PaStreamCallbackTimeInfo*,
                       PaStreamCallbackFlags, void* self) {
        auto* r = static_cast<Recorder*>(self);
        const auto* in = static_cast<const float*>(input);
        if (in) {
            double sum = 0;
            for (unsigned long i = 0; i < frames; i++) {
                sum += static_cast<double>(in[i]) * in[i];
            }
            const auto rms = static_cast<float>(std::sqrt(sum / std::max(1ul, frames)));
            float prev = r->peak.load();
            while (rms > prev && !r->peak.compare_exchange_weak(prev, rms)) {}
        }
        std::lock_guard g(r->m);
        for (unsigned long i = 0; in && i + static_cast<unsigned long>(r->decimate) <= frames;
             i += static_cast<unsigned long>(r->decimate)) {
            float sum = 0;
            for (int k = 0; k < r->decimate; k++) {
                sum += in[i + static_cast<unsigned long>(k)];
            }
            r->pcm.push_back(sum / static_cast<float>(r->decimate));
        }
        return paContinue;
    }

public:
    /// The loudest level since the last call (0 if not recording)
    float takePeak() {
        if (fakeLive) {
            std::lock_guard g(m);
            const auto ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fakeStart)
                            .count();
            const size_t upTo = std::min(pcm.size(), static_cast<size_t>(ms) * RATE / 1000);
            double sum = 0;
            for (size_t i = fakeRead; i < upTo; i++) {
                sum += static_cast<double>(pcm[i]) * pcm[i];
            }
            const float rms =
                    upTo > fakeRead ? static_cast<float>(std::sqrt(sum / static_cast<double>(upTo - fakeRead))) : 0;
            fakeRead = upTo;
            return rms;
        }
        return peak.exchange(0.0f);
    }
    bool isRecording() const { return live || fakeLive; }

private:
    std::atomic<float> peak{0.0f};
    std::atomic<bool> live{false};      ///< the microphone is recording (read by the level thread)
    std::atomic<bool> fakeLive{false};  ///< XOURNALAI_STT_FAKE_MIC is "playing"
    std::chrono::steady_clock::time_point fakeStart;
    size_t fakeRead = 0;
    std::mutex m;
    std::vector<float> pcm;
    PaStream* stream = nullptr;
    int decimate = 1;
    bool initialized = false;
};

json transcribe(whisper_context* ctx, const std::vector<float>& pcm, int threads) {
    const auto audioMs = static_cast<long>(pcm.size() * 1000 / RATE);
    if (!hasSpeech(pcm)) {
        return {{"event", "text"}, {"text", ""}, {"silent", true}, {"audio_ms", audioMs}, {"ms", 0}};
    }
    const auto t0 = std::chrono::steady_clock::now();
    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.language = "en";
    p.n_threads = threads;
    p.no_timestamps = true;
    p.no_context = true;
    p.suppress_blank = true;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_special = false;
    p.print_timestamps = false;
    // Short requests: size the encoder to the audio (whisper otherwise always encodes 30 s); 1500 = 30 s
    p.audio_ctx = std::min(1500, static_cast<int>(pcm.size() * 1500 / (30 * RATE)) + 128);
    if (whisper_full(ctx, p, pcm.data(), static_cast<int>(pcm.size())) != 0) {
        return {{"event", "error"}, {"message", "transcription failed"}};
    }
    std::string text;
    for (int i = 0; i < whisper_full_n_segments(ctx); i++) {
        text += whisper_full_get_segment_text(ctx, i);
    }
    // Trim
    const auto b = text.find_first_not_of(" \t\n");
    const auto e = text.find_last_not_of(" \t\n");
    text = b == std::string::npos ? "" : text.substr(b, e - b + 1);
    const bool silent = isNonSpeech(text);
    const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    return {{"event", "text"}, {"text", silent ? "" : text}, {"silent", silent}, {"audio_ms", audioMs}, {"ms", ms}};
}

}  // namespace

int main(int argc, char** argv) {
    std::string model;
    int threads = std::min(4, std::max(1, static_cast<int>(std::thread::hardware_concurrency())));
    for (int i = 1; i + 1 < argc; i++) {
        if (std::string(argv[i]) == "--model") {
            model = argv[++i];
        } else if (std::string(argv[i]) == "--threads") {
            threads = std::max(1, std::atoi(argv[++i]));
        }
    }
    if (model.empty()) {
        error("usage: xournalai-stt --model <ggml model file> [--threads N]");
        return 2;
    }
    whisper_log_set([](enum ggml_log_level, const char*, void*) {}, nullptr);  // quiet: stdout is the protocol

    const auto t0 = std::chrono::steady_clock::now();
    whisper_context_params cp = whisper_context_default_params();
    cp.flash_attn = true;
    whisper_context* ctx = whisper_init_from_file_with_params(model.c_str(), cp);
    if (!ctx) {
        error("could not load the speech model " + model);
        return 1;
    }
    emit({{"event", "ready"},
          {"model", model},
          {"threads", threads},
          {"load_ms",
           std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()}});

    Recorder recorder;
    bool recording = false;
    // While recording from the microphone: its level ~20 times a second (the app shows it moving with the voice)
    std::atomic<bool> running{true};
    std::thread levels([&] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (recorder.isRecording()) {
                emit({{"event", "level"}, {"rms", std::round(recorder.takePeak() * 1000) / 1000}});
            }
        }
    });
    std::string line;
    while (std::getline(std::cin, line)) {
        json cmd;
        try {
            cmd = json::parse(line);
        } catch (const std::exception&) {
            error("not JSON: " + line);
            continue;
        }
        const std::string c = cmd.value("cmd", "");
        if (c == "start") {
            if (recording) {
                recorder.stop();
            }
            std::string why;
            recording = recorder.start(why);
            recording ? emit({{"event", "listening"}}) : error(why);
        } else if (c == "stop") {
            const auto pcm = recorder.stop();
            recording = false;
            emit(transcribe(ctx, pcm, threads));
        } else if (c == "cancel") {
            recorder.stop();
            recording = false;
            emit({{"event", "cancelled"}});
        } else if (c == "transcribe") {
            std::string why;
            const auto pcm = readWav(cmd.value("wav", ""), why);
            why.empty() ? emit(transcribe(ctx, pcm, threads)) : error(why);
        } else if (c == "quit") {
            break;
        } else {
            error("unknown command: " + c);
        }
    }
    running = false;
    levels.join();
    whisper_free(ctx);
    return 0;
}
