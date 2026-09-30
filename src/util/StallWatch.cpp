#include "util/StallWatch.h"

#include <algorithm>  // for find
#include <atomic>     // for atomic
#include <chrono>     // for milliseconds
#include <cstdio>     // for FILE, fprintf
#include <deque>      // for deque
#include <mutex>      // for mutex, lock_guard
#include <thread>     // for thread, this_thread

#include <glib.h>

namespace xoj::util::stall {

namespace {
constexpr int64_t BEAT_US = 20 * 1000;            ///< heartbeat interval
constexpr int64_t SUSPEND_US = 60 * 1000 * 1000;  ///< longer gaps are a suspended machine, not a stall
constexpr size_t KEEP = 64;

std::atomic<int64_t> lastBeat{0};
std::atomic<bool> running{false};
std::thread watchdog;
guint beatSource = 0;
GThread* mainThread = nullptr;

std::mutex m;                                                     // guards everything below
std::vector<std::string> activities;                              // of the UI thread
std::vector<std::pair<std::thread::id, std::string>> background;  // of other threads (e.g. holding the lock)
std::deque<Stall> ring;
Summary sum;
FILE* logFile = nullptr;

FILE* openLog() {
    const char* env = g_getenv("XOURNALAI_TRACE_STALLS");
    if (env && *env) {
        return fopen(env, "a");
    }
    gchar* flag = g_build_filename(g_get_user_cache_dir(), "xournalai", "trace-stalls", nullptr);
    gchar* path = g_build_filename(g_get_user_cache_dir(), "xournalai", "stall-trace.log", nullptr);
    FILE* f = g_file_test(flag, G_FILE_TEST_EXISTS) ? fopen(path, "a") : nullptr;
    g_free(flag);
    g_free(path);
    return f;
}

void watch() {
    int64_t stallSince = 0;  // the beat the stall follows (0: no stall)
    std::vector<std::string> seen;
    std::vector<std::string> seenBackground;
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const int64_t beat = lastBeat;
        const int64_t now = g_get_monotonic_time();
        if (stallSince == 0) {
            if (now - beat > THRESHOLD_US + BEAT_US) {
                stallSince = beat;
                seen.clear();
                seenBackground.clear();
            }
        } else if (beat != stallSince) {
            // The UI thread responded again
            const int64_t duration = beat - stallSince - BEAT_US;
            stallSince = 0;
            if (duration <= THRESHOLD_US || duration > SUSPEND_US) {
                continue;
            }
            std::string what;
            for (const auto& s: seen) {
                what += (what.empty() ? "" : " ; ") + s;
            }
            if (what.empty()) {
                what = "unknown";
            }
            if (!seenBackground.empty()) {
                what += " (meanwhile:";
                for (const auto& s: seenBackground) {
                    what += " " + s + ";";
                }
                what.back() = ')';
            }
            std::lock_guard g(m);
            ring.push_back({beat - duration, duration, what});
            if (ring.size() > KEEP) {
                ring.pop_front();
            }
            sum.count++;
            sum.totalUs += duration;
            sum.maxUs = std::max(sum.maxUs, duration);
            if (logFile) {
                fprintf(logFile, "%lld stall %lldms: %s\n", static_cast<long long>((beat - duration) / 1000),
                        static_cast<long long>(duration / 1000), what.c_str());
                fflush(logFile);
            }
            continue;
        }
        if (stallSince != 0) {
            // Stalled: what is the UI thread doing?
            std::lock_guard g(m);
            std::string now;
            for (const auto& a: activities) {
                now += (now.empty() ? "" : " / ") + a;
            }
            if (!now.empty() && std::find(seen.begin(), seen.end(), now) == seen.end()) {
                seen.push_back(now);
            }
            for (const auto& [id, b]: background) {
                if (std::find(seenBackground.begin(), seenBackground.end(), b) == seenBackground.end()) {
                    seenBackground.push_back(b);
                }
            }
        }
    }
}

gboolean onBeat(gpointer) {
    lastBeat = g_get_monotonic_time();
    return G_SOURCE_CONTINUE;
}
}  // namespace

void start() {
    if (running) {
        return;
    }
    mainThread = g_thread_self();
    lastBeat = g_get_monotonic_time();
    {
        std::lock_guard g(m);
        logFile = openLog();
    }
    beatSource = g_timeout_add_full(G_PRIORITY_HIGH, BEAT_US / 1000, onBeat, nullptr, nullptr);
    running = true;
    watchdog = std::thread(watch);
}

void stop() {
    if (!running) {
        return;
    }
    running = false;
    if (watchdog.joinable()) {
        watchdog.join();
    }
    if (beatSource) {
        g_source_remove(beatSource);
        beatSource = 0;
    }
    std::lock_guard g(m);
    if (logFile) {
        fclose(logFile);
        logFile = nullptr;
    }
}

Activity::Activity(std::string what) {
    std::lock_guard g(m);
    if (g_thread_self() == mainThread) {
        activities.push_back(std::move(what));
    } else {
        background.emplace_back(std::this_thread::get_id(), std::move(what));
    }
}

Activity::~Activity() {
    std::lock_guard g(m);
    if (g_thread_self() == mainThread) {
        if (!activities.empty()) {
            activities.pop_back();
        }
        return;
    }
    const auto self = std::this_thread::get_id();
    for (auto it = background.rbegin(); it != background.rend(); ++it) {
        if (it->first == self) {
            background.erase(std::next(it).base());
            break;
        }
    }
}

auto recent() -> std::vector<Stall> {
    std::lock_guard g(m);
    return {ring.begin(), ring.end()};
}

auto summary() -> Summary {
    std::lock_guard g(m);
    return sum;
}

}  // namespace xoj::util::stall
