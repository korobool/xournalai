/*
 * xournalai (based on Xournal++)
 *
 * Who is speaking (1.23.0, experimental; the owner, 11 Nov 2026: «начинаем работать над экспериментальной галочкой
 * разделения речи собеседников»). The speech recognizer only measures: a voice fingerprint for each stretch of speech
 * (docs/mcp/TRANSCRIBERS.md). This decides, as the recording goes on, which of the voices heard so far it is: close
 * enough to one of them (the cosine to its running average) is that voice, and that average learns from it; a
 * long enough stretch far from all of them is a new voice, named A, B, C … in the order they first speak; a short
 * stretch that matches none well is «?». Earlier answers are never changed (they are on the paper already).
 *
 * The thresholds come from a public two-speaker recording measured with NVIDIA TitaNet-L (the same speaker 0.51–0.88,
 * different ones at most 0.43); mcp.json can change them (assistant.live_notes.speakers).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "Transcriber.h"  // for Voice

namespace xoj::assistant {

class SpeakerTracker final {
public:
    struct Settings {
        double same = 0.47;     ///< this close to a voice's average (cosine): that voice
        double newMinS = 1.5;   ///< a stretch this long that matches no voice is a new one
        double nearS = 0.30;    ///< a shorter one is the closest voice when at least this close, else «?»
        double learnMinS = 1.0;  ///< only stretches this long change a voice's average
    };
    explicit SpeakerTracker(Settings settings): settings(settings) {}
    SpeakerTracker(): SpeakerTracker(Settings{}) {}

    /// Who spoke `seconds` of speech with this fingerprint: "A", "B" …, or "?" (none, too short, or unsure)
    std::string assign(const Voice& voice, double seconds);
    /// The voices heard so far, in the order they first spoke
    std::vector<std::string> labels() const;
    /// The cosine of two fingerprints (0 when either is empty or their lengths differ)
    static double similarity(const Voice& a, const Voice& b);
    /// "A" … "Z", then "V27", "V28" …
    static std::string labelOf(size_t index);

private:
    struct Speaker {
        std::string label;
        std::vector<double> sum;  ///< the fingerprints, weighted by their length
        Voice average() const;    ///< (unit length)
    };
    Settings settings;
    std::vector<Speaker> speakers;
};

}  // namespace xoj::assistant
