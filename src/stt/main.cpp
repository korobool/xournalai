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

#include <algorithm>           // for sort, max, min, count_if
#include <atomic>              // for atomic
#include <cctype>              // for isalnum, tolower
#include <chrono>              // for steady_clock
#include <cmath>               // for sqrt
#include <condition_variable>  // for condition_variable
#include <cstdint>             // for int16_t
#include <cstdio>              // for printf
#include <cstdlib>             // for getenv
#include <cstring>             // for memcpy
#include <deque>               // for deque
#include <fstream>             // for ifstream
#include <iostream>            // for cin
#include <iterator>            // for istreambuf_iterator
#include <mutex>               // for mutex
#include <string>              // for string
#include <thread>              // for hardware_concurrency
#include <vector>              // for vector

#include <nlohmann/json.hpp>
#include <portaudio.h>
#include <sndfile.h>
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

/// ~/.cache/xournalai (or $XDG_CACHE_HOME/xournalai)
std::string cacheDir() {
    const char* home = getenv("HOME");
    return getenv("XDG_CACHE_HOME") ? std::string(getenv("XDG_CACHE_HOME")) + "/xournalai" :
           home                     ? std::string(home) + "/.cache/xournalai" :
                                      std::string();
}

/// Diagnostics: speech.log in the cache folder (one line per clip, shared with the app)
void logLine(const std::string& text) {
    const std::string dir = cacheDir();
    if (dir.empty()) {
        return;
    }
    if (FILE* f = fopen((dir + "/speech.log").c_str(), "a")) {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char stamp[32];
        strftime(stamp, sizeof stamp, "%H:%M:%S", localtime(&now));
        fprintf(f, "%s stt  %s\n", stamp, text.c_str());
        fclose(f);
    }
}

/// Keeps a clip judged silent (for diagnosis): 16-bit mono 16 kHz WAV
void saveWav(const std::string& path, const std::vector<float>& pcm) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return;
    }
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    const auto bytes = static_cast<uint32_t>(pcm.size() * 2);
    fwrite("RIFF", 1, 4, f), u32(36 + bytes), fwrite("WAVEfmt ", 1, 8, f), u32(16), u16(1), u16(1), u32(RATE),
            u32(RATE * 2), u16(2), u16(16), fwrite("data", 1, 4, f), u32(bytes);
    for (float s: pcm) {
        const auto v = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767);
        fwrite(&v, 2, 1, f);
    }
    fclose(f);
}

struct SpeechStats {
    float peak = 0, noise = 0, threshold = 0;
    long loudFrames = 0;
};
SpeechStats lastStats;

bool isNonSpeech(const std::string& text);

/// Mono float samples at 16 kHz from any file libsndfile reads (the recorder's .ogg, .wav, .flac, …); empty on failure
std::vector<float> readAudio(const std::string& path, std::string& why) {
    SF_INFO info{};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (!f) {
        why = std::string("cannot read the audio file: ") + sf_strerror(nullptr);
        return {};
    }
    std::vector<float> frames(static_cast<size_t>(info.frames) * static_cast<size_t>(info.channels));
    const sf_count_t got = sf_readf_float(f, frames.data(), info.frames);
    sf_close(f);
    std::vector<float> mono(static_cast<size_t>(got));
    for (sf_count_t i = 0; i < got; i++) {
        float sum = 0;
        for (int c = 0; c < info.channels; c++) {
            sum += frames[static_cast<size_t>(i * info.channels + c)];
        }
        mono[static_cast<size_t>(i)] = sum / static_cast<float>(info.channels);
    }
    if (info.samplerate == RATE) {
        return mono;
    }
    // To 16 kHz: each output sample averages the input it covers (a simple anti-aliasing filter)
    const double step = static_cast<double>(info.samplerate) / RATE;
    std::vector<float> out(static_cast<size_t>(static_cast<double>(mono.size()) / step));
    for (size_t i = 0; i < out.size(); i++) {
        const auto a = static_cast<size_t>(static_cast<double>(i) * step);
        const auto b = std::min(mono.size(), std::max(a + 1, static_cast<size_t>(static_cast<double>(i + 1) * step)));
        float sum = 0;
        for (size_t k = a; k < b; k++) {
            sum += mono[k];
        }
        out[i] = sum / static_cast<float>(b - a);
    }
    return out;
}

/// A whole recording (long, with timestamps): {"event":"transcript","text","segments":[{start,end,text}],…}
json transcribeFile(whisper_context* ctx, const std::vector<float>& pcm, int threads) {
    const auto t0 = std::chrono::steady_clock::now();
    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.language = "en";
    p.n_threads = threads;
    p.no_timestamps = false;
    p.print_progress = false;
    p.print_realtime = false;
    p.print_special = false;
    p.print_timestamps = false;
    p.suppress_blank = true;
    if (whisper_full(ctx, p, pcm.data(), static_cast<int>(pcm.size())) != 0) {
        return {{"event", "error"}, {"message", "transcription failed"}};
    }
    json segments = json::array();
    std::string text;
    for (int i = 0; i < whisper_full_n_segments(ctx); i++) {
        std::string s = whisper_full_get_segment_text(ctx, i);
        const auto b = s.find_first_not_of(" \t\n");
        s = b == std::string::npos ? "" : s.substr(b);
        if (s.empty() || isNonSpeech(s)) {
            continue;
        }
        // (whisper's timestamps are in 10 ms units)
        segments.push_back({{"start", whisper_full_get_segment_t0(ctx, i) / 100.0},
                            {"end", whisper_full_get_segment_t1(ctx, i) / 100.0},
                            {"text", s}});
        text += (text.empty() ? "" : " ") + s;
    }
    const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    logLine("file: audio " + std::to_string(pcm.size() * 1000 / RATE) + "ms, " + std::to_string(segments.size()) +
            " segments in " + std::to_string(ms) + "ms");
    return {{"event", "transcript"},
            {"text", text},
            {"segments", segments},
            {"audio_ms", static_cast<long>(pcm.size() * 1000 / RATE)},
            {"ms", ms}};
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
    // The background: the quietest frames. The threshold is relative to it, but capped: when the user speaks from
    // the first to the last moment, even the quietest frames are voice (an uncapped threshold then rejected speech)
    const float noise = sorted[sorted.size() / 10];
    const float threshold = std::clamp(noise * 3.0f, 0.005f, 0.02f);
    const auto loud = std::count_if(rms.begin(), rms.end(), [&](float r) { return r > threshold; });
    lastStats = {sorted.back(), noise, threshold, static_cast<long>(loud)};
    // ~0.2 s of voice; or clearly something loud: then whisper decides (its "no words" answers are filtered)
    return loud >= 10 || sorted.back() > 0.05f;
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
        if (fakeLive.exchange(false)) {
            // the fake microphone "heard" what played until now, like a real one
            std::lock_guard g(m);
            const auto ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - fakeStart)
                            .count();
            pcm.resize(std::min(pcm.size(), static_cast<size_t>(ms) * RATE / 1000));
            return std::move(pcm);
        }
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
    lastStats = {};
    auto stats = [&]() {
        char b[160];
        snprintf(b, sizeof b, "audio %ldms peak %.4f noise %.4f threshold %.4f voiced frames %ld", audioMs,
                 lastStats.peak, lastStats.noise, lastStats.threshold, lastStats.loudFrames);
        return std::string(b);
    };
    if (!hasSpeech(pcm)) {
        logLine(stats() + " -> SILENT (no voice found; clip kept as last-silent.wav)");
        if (!cacheDir().empty()) {
            saveWav(cacheDir() + "/last-silent.wav", pcm);
        }
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
    logLine(stats() + " -> " + (silent ? "SILENT (whisper heard no words: \"" + text + "\")" : "\"" + text + "\"") +
            " in " + std::to_string(ms) + "ms");
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
    // Transcriptions run here, in order, so that a new "start" opens the microphone at once even while the previous
    // request is still being transcribed (else the start of the next phrase was never recorded)
    std::mutex jobsMutex;
    std::condition_variable jobsCond;
    std::deque<std::vector<float>> jobs;      // clips; an empty one stands for the next file of fileJobs
    std::deque<std::vector<float>> fileJobs;  // whole recordings
    bool closing = false;
    std::thread worker([&] {
        for (;;) {
            std::vector<float> pcm;
            bool file = false;
            {
                std::unique_lock g(jobsMutex);
                jobsCond.wait(g, [&] { return closing || !jobs.empty(); });
                if (jobs.empty()) {
                    return;  // closing, nothing left
                }
                pcm = std::move(jobs.front());
                jobs.pop_front();
                if (pcm.empty() && !fileJobs.empty()) {
                    pcm = std::move(fileJobs.front());
                    fileJobs.pop_front();
                    file = true;
                }
            }
            emit(file ? transcribeFile(ctx, pcm, threads) : transcribe(ctx, pcm, threads));
        }
    });
    auto enqueue = [&](std::vector<float> pcm) {
        {
            std::lock_guard g(jobsMutex);
            jobs.push_back(std::move(pcm));
        }
        jobsCond.notify_one();
    };

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
            enqueue(recorder.stop());
            recording = false;
        } else if (c == "cancel") {
            recorder.stop();
            recording = false;
            emit({{"event", "cancelled"}});
        } else if (c == "transcribe") {
            std::string why;
            auto pcm = readWav(cmd.value("wav", ""), why);
            why.empty() ? enqueue(std::move(pcm)) : error(why);
        } else if (c == "transcribe_file") {
            // A whole recording (audio_transcribe): on the worker's queue too, after any short request
            std::string why;
            auto pcm = readAudio(cmd.value("path", ""), why);
            if (!why.empty()) {
                error(why);
                continue;
            }
            {
                std::lock_guard g(jobsMutex);
                fileJobs.push_back(std::move(pcm));
                jobs.emplace_back();  // a marker: the next job is a file
            }
            jobsCond.notify_one();
        } else if (c == "quit") {
            break;
        } else {
            error("unknown command: " + c);
        }
    }
    {
        std::lock_guard g(jobsMutex);
        closing = true;  // the queued transcriptions are still answered
    }
    jobsCond.notify_one();
    worker.join();
    running = false;
    levels.join();
    whisper_free(ctx);
    return 0;
}
