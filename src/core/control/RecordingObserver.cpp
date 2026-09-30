#include "RecordingObserver.h"

#include <utility>  // for move

namespace xoj::audio {

namespace {
RecordingObserver& instance() {
    static RecordingObserver o;
    return o;
}
}  // namespace

void setRecordingObserver(RecordingObserver observer) { instance() = std::move(observer); }

const RecordingObserver& recordingObserver() { return instance(); }

}  // namespace xoj::audio
