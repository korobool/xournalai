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

}  // namespace xoj::audio
