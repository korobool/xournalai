/*
 * xournalai (based on Xournal++)
 *
 * Lets the assistant hear that an audio recording (the Xournal++ recorder) has ended, with its file: the serving
 * session is told, so it can transcribe it and use it.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for int64_t
#include <functional>  // for function
#include <string>      // for string

#include "filesystem.h"  // for fs::path

namespace xoj::audio {

struct RecordingFinished {
    fs::path file;       ///< the complete file (written and closed)
    std::string name;    ///< its name, as strokes written meanwhile refer to it
    int64_t durationMs;  ///< how long it recorded
};

using RecordingObserver = std::function<void(const RecordingFinished&)>;

void setRecordingObserver(RecordingObserver observer);  ///< null: nobody is told
const RecordingObserver& recordingObserver();

/// Where in the recording `nowUs` is, in ms from the file's first sample (`firstSampleUs`, 0 until captured; before
/// that, from `startedMs`, when recording was started). Never negative. All g_get_monotonic_time based.
constexpr int64_t recordingTimeMs(int64_t nowUs, int64_t firstSampleUs, int64_t startedMs) {
    const int64_t startMs = firstSampleUs > 0 ? firstSampleUs / 1000 : startedMs;
    const int64_t t = nowUs / 1000 - startMs;
    return t > 0 ? t : 0;
}

}  // namespace xoj::audio
