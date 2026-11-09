#include "LiveChunker.h"

#include <algorithm>  // for clamp, min
#include <cmath>      // for sqrt, lround
#include <cstddef>    // for ptrdiff_t
#include <utility>    // for move

namespace xoj::assistant {

namespace {
constexpr int MS_PER_FRAME = 1000 * LiveChunker::FRAME / LiveChunker::RATE;  // 50
size_t frames(int ms) { return static_cast<size_t>(std::max(1, ms / MS_PER_FRAME)); }
}  // namespace

LiveChunker::LiveChunker(Options options, OnChunk onChunk): opt(options), onChunk(std::move(onChunk)) {}

int64_t LiveChunker::heardMs() const { return totalSamples.load() * 1000 / RATE; }

void LiveChunker::feed(const float* in, size_t count, int channels, int sampleRate) {
    if (channels <= 0 || sampleRate <= 0) {
        return;
    }
    std::lock_guard lock(mutex);
    const double step = static_cast<double>(sampleRate) / RATE;  // input samples per output sample
    std::vector<float> out;
    out.reserve(static_cast<size_t>(static_cast<double>(count) / step) + 2);
    for (size_t i = 0; i < count; i++) {
        float m = 0;
        for (int c = 0; c < channels; c++) {
            m += in[i * static_cast<size_t>(channels) + static_cast<size_t>(c)];
        }
        m /= static_cast<float>(channels);
        if (!havePrev) {
            prev = m;
            havePrev = true;
            out.push_back(m);
            srcPos = step;  // (the next output sample, from this one)
            continue;
        }
        // output samples between prev (at 0) and m (at 1)
        while (srcPos <= 1.0) {
            out.push_back(prev + static_cast<float>(srcPos) * (m - prev));
            srcPos += step;
        }
        srcPos -= 1.0;
        prev = m;
    }
    pending.insert(pending.end(), out.begin(), out.end());
    size_t used = 0;
    while (pending.size() - used >= static_cast<size_t>(FRAME)) {
        frame(pending.data() + used);
        used += FRAME;
    }
    pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(used));
}

void LiveChunker::feedMono16k(const float* samples, size_t count) { feed(samples, count, 1, RATE); }

void LiveChunker::frame(const float* s) {
    double sum = 0;
    for (int i = 0; i < FRAME; i++) {
        sum += static_cast<double>(s[i]) * s[i];
        pcm.push_back(static_cast<int16_t>(std::lround(std::clamp(s[i], -1.0F, 1.0F) * 32767.0F)));
    }
    totalSamples += FRAME;
    const float rms = static_cast<float>(std::sqrt(sum / FRAME));
    const bool isVoice = voice.push(rms);
    frameRms.push_back(rms);
    frameVoice.push_back(isVoice);
    if (isVoice) {
        voiceFrames++;
        silentRun = 0;
    } else {
        silentRun++;
    }

    if (voiceFrames == 0) {
        dropLeadingSilence();
        cutWanted = false;  // (nothing said yet: nothing to cut)
        return;
    }
    const size_t n = frameRms.size();
    if (cutWanted.exchange(false)) {
        emit(quietestFrame(std::min(n, frames(500))) + 1, "cut");
    } else if (silentRun >= frames(opt.pauseMs)) {
        emit(n - silentRun + frames(300), "pause");  // (a little of the pause stays with the voice)
    } else if (n >= frames(opt.softMs) && silentRun >= frames(opt.microPauseMs)) {
        emit(n - silentRun / 2, "long");  // (in the middle of the short pause)
    } else if (n >= frames(opt.hardMs)) {
        emit(quietestFrame(std::min(n, frames(2000))) + 1, "limit");
    }
}

size_t LiveChunker::quietestFrame(size_t lookback) const {
    // the quietest 100 ms (2 frames) among the last `lookback` frames
    const size_t n = frameRms.size();
    size_t best = n - 1;
    float bestLevel = 1e9F;
    for (size_t i = n - lookback; i + 1 < n; i++) {
        const float level = frameRms[i] + frameRms[i + 1];
        if (level < bestLevel) {
            bestLevel = level;
            best = i;
        }
    }
    return best;
}

void LiveChunker::dropLeadingSilence() {
    // keep 300 ms of the silence before the voice (a soft start helps recognition)
    const size_t keep = frames(300);
    if (frameRms.size() > 2 * keep) {
        const size_t drop = frameRms.size() - keep;
        pcm.erase(pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(drop * FRAME));
        frameRms.erase(frameRms.begin(), frameRms.begin() + static_cast<std::ptrdiff_t>(drop));
        frameVoice.erase(frameVoice.begin(), frameVoice.begin() + static_cast<std::ptrdiff_t>(drop));
        startSample += static_cast<int64_t>(drop) * FRAME;
        silentRun = frameRms.size();
    }
}

void LiveChunker::emit(size_t upTo, const char* reason) {
    upTo = std::clamp<size_t>(upTo, 1, frameRms.size());
    size_t voiced = 0;
    for (size_t i = 0; i < upTo; i++) {
        voiced += frameVoice[i] ? 1 : 0;
    }
    LiveChunk c;
    c.startMs = startSample * 1000 / RATE;
    c.endMs = (startSample + static_cast<int64_t>(upTo) * FRAME) * 1000 / RATE;
    c.reason = reason;
    c.pcm.assign(pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(upTo * FRAME));
    // the rest begins the next piece
    pcm.erase(pcm.begin(), pcm.begin() + static_cast<std::ptrdiff_t>(upTo * FRAME));
    frameRms.erase(frameRms.begin(), frameRms.begin() + static_cast<std::ptrdiff_t>(upTo));
    frameVoice.erase(frameVoice.begin(), frameVoice.begin() + static_cast<std::ptrdiff_t>(upTo));
    startSample += static_cast<int64_t>(upTo) * FRAME;
    voiceFrames = static_cast<size_t>(std::count(frameVoice.begin(), frameVoice.end(), true));
    silentRun = 0;
    for (auto it = frameVoice.rbegin(); it != frameVoice.rend() && !*it; ++it) {
        silentRun++;
    }
    if (voiced * MS_PER_FRAME < static_cast<size_t>(opt.minVoiceMs)) {
        return;  // (a cough, a click: nothing to recognize)
    }
    c.index = nextIndex++;
    if (onChunk) {
        onChunk(std::move(c));
    }
}

void LiveChunker::finish() {
    std::lock_guard lock(mutex);
    if (!pending.empty()) {  // (the last partial frame, padded with silence)
        pending.resize(FRAME, 0.0F);
        const std::vector<float> last = std::move(pending);
        pending.clear();
        frame(last.data());
    }
    if (voiceFrames > 0 && !frameRms.empty()) {
        emit(frameRms.size(), "end");
    }
    pcm.clear();
    frameRms.clear();
    frameVoice.clear();
    voiceFrames = 0;
    silentRun = 0;
}

}  // namespace xoj::assistant
