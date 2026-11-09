/*
 * xournalai (based on Xournal++)
 *
 * A tap on the sound being recorded: the samples the recorder writes to its file (after the gain) are also given to
 * whoever listens (live notes cut them into pieces for speech recognition). The recording itself is unchanged.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>     // for size_t
#include <functional>  // for function

namespace xoj::audio {

/// Interleaved float frames (`count` frames of `channels` samples) at `sampleRate`, on the recorder's thread
using SampleTap = std::function<void(const float* frames, size_t count, int channels, int sampleRate)>;

void setSampleTap(SampleTap tap);  ///< null: nobody listens
/// Called by the recorder for every block it writes
void tapSamples(const float* frames, size_t count, int channels, int sampleRate);

}  // namespace xoj::audio
