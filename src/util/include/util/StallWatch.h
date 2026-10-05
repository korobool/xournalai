/*
 * xournalai (based on Xournal++)
 *
 * UI stall watchdog. A heartbeat on the main loop and a watchdog thread notice every time the UI thread does not
 * respond for longer than a threshold (the user's pen and the canvas freeze then), and record what the UI thread
 * was doing, from the activities it announces (Activity: "tool render_page", "waiting for the document lock", ...).
 *
 * Always on (cheap); a summary is in app_status. A detailed log is written to ~/.cache/xournalai/stall-trace.log
 * while ~/.cache/xournalai/trace-stalls exists (or XOURNALAI_TRACE_STALLS names a file).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for int64_t
#include <functional>  // for function
#include <string>   // for string
#include <vector>   // for vector

namespace xoj::util::stall {

constexpr int64_t THRESHOLD_US = 50 * 1000;  ///< a stall: the UI thread is unresponsive for longer than this

/// Starts the heartbeat and the watchdog (from the main thread, with the main loop about to run). Idempotent.
/// Called once per hang, from the watchdog thread (not the UI thread, which is stuck), when the UI thread has been
/// stuck for `afterUs`: e.g. a rescue save. Must not touch GTK. Set before start(); null: none.
void setHangHandler(int64_t afterUs, std::function<void()> handler);

void start();
/// Stops them (at exit)
void stop();

/// Announces what the current thread is doing while it exists (nests). For the UI thread it tells what a stall is;
/// for other threads, what ran meanwhile (e.g. who held the document lock the UI thread waited for)
class Activity final {
public:
    explicit Activity(std::string what);
    ~Activity();
    Activity(const Activity&) = delete;
    Activity& operator=(const Activity&) = delete;
};

struct Stall {
    int64_t startUs;     ///< monotonic time
    int64_t durationUs;  ///< how long the UI thread did not respond
    std::string what;    ///< the activities seen during the stall ("unknown" if none was announced)
};

/// The most recent stalls (up to 64), oldest first
std::vector<Stall> recent();

struct Summary {
    int64_t count = 0;    ///< stalls since start
    int64_t maxUs = 0;    ///< the longest
    int64_t totalUs = 0;  ///< all stalls together
};
Summary summary();

}  // namespace xoj::util::stall
