#include "SpeakerTracker.h"

#include <cmath>  // for sqrt

namespace xoj::assistant {

double SpeakerTracker::similarity(const Voice& a, const Voice& b) {
    if (a.empty() || a.size() != b.size()) {
        return 0;
    }
    double dot = 0;
    double na = 0;
    double nb = 0;
    for (size_t i = 0; i < a.size(); i++) {
        dot += static_cast<double>(a[i]) * b[i];
        na += static_cast<double>(a[i]) * a[i];
        nb += static_cast<double>(b[i]) * b[i];
    }
    return na > 0 && nb > 0 ? dot / std::sqrt(na * nb) : 0;
}

std::string SpeakerTracker::labelOf(size_t index) {
    return index < 26 ? std::string(1, static_cast<char>('A' + index)) : "V" + std::to_string(index + 1);
}

Voice SpeakerTracker::Speaker::average() const {
    double n = 0;
    for (double x: sum) {
        n += x * x;
    }
    n = std::sqrt(n);
    Voice v;
    v.reserve(sum.size());
    for (double x: sum) {
        v.push_back(static_cast<float>(n > 0 ? x / n : 0));
    }
    return v;
}

std::string SpeakerTracker::assign(const Voice& voice, double seconds) {
    if (voice.empty()) {
        return "?";
    }
    Speaker* best = nullptr;
    double bestSim = -2;
    for (auto& s: speakers) {
        if (s.sum.size() != voice.size()) {
            continue;  // (another model's: never compared)
        }
        const double sim = similarity(s.average(), voice);
        if (sim > bestSim) {
            bestSim = sim;
            best = &s;
        }
    }
    auto learn = [&](Speaker& s) {
        if (seconds >= settings.learnMinS) {
            for (size_t i = 0; i < voice.size(); i++) {
                s.sum[i] += voice[i] * seconds;
            }
        }
    };
    if (best && bestSim >= settings.same) {
        learn(*best);
        return best->label;
    }
    if (seconds >= settings.newMinS) {
        Speaker s{labelOf(speakers.size()), std::vector<double>(voice.size(), 0.0)};
        for (size_t i = 0; i < voice.size(); i++) {
            s.sum[i] = voice[i] * seconds;
        }
        speakers.push_back(std::move(s));
        return speakers.back().label;
    }
    return best && bestSim >= settings.nearS ? best->label : "?";
}

std::vector<std::string> SpeakerTracker::labels() const {
    std::vector<std::string> out;
    out.reserve(speakers.size());
    for (const auto& s: speakers) {
        out.push_back(s.label);
    }
    return out;
}

}  // namespace xoj::assistant
