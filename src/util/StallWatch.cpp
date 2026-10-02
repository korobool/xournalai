#include "util/StallWatch.h"

#include <algorithm>  // for find
#include <atomic>     // for atomic
#include <chrono>     // for milliseconds
#include <cstdio>     // for FILE, fprintf
#include <deque>      // for deque
#include <mutex>      // for mutex, lock_guard
#include <thread>     // for thread, this_thread

#include <glib.h>

#if defined(__linux__) && defined(__GLIBC__)
#include <csignal>       // for sigaction, SIGUSR2
#include <ctime>         // for time, localtime_r, strftime
#include <dirent.h>      // for opendir (the process's threads)
#include <execinfo.h>    // for backtrace, backtrace_symbols_fd
#include <sys/syscall.h> // for SYS_tgkill, SYS_gettid
#include <unistd.h>      // for getpid, write, syscall
#define XOJ_STALL_STACKS 1
#endif

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

constexpr int64_t HANG_US = 2 * 1000 * 1000;           ///< a stall this long is reported while it lasts
constexpr int64_t HANG_REPEAT_US = 10 * 1000 * 1000;  ///< and again this often

std::string describe(const std::vector<std::string>& seen, const std::vector<std::string>& seenBackground) {
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
    return what;
}

#ifdef XOJ_STALL_STACKS
/// A hang (the UI thread stuck for HANG_US) gets the stack of every thread written to the log: each thread, signalled
/// in turn, writes its own (backtrace is async-signal-safe once loaded). Resolve "+0x…" offsets with
/// `addr2line -f -C -e <binary> 0x…`.
constexpr int STACK_SIGNAL = SIGUSR2;
std::atomic<int> stackFd{-1};
std::atomic<bool> stackDone{false};

void onStackSignal(int) {
    const int fd = stackFd.load();
    if (fd >= 0) {
        char head[64];
        const int n = snprintf(head, sizeof head, "  -- thread %ld%s\n", static_cast<long>(syscall(SYS_gettid)),
                               static_cast<long>(syscall(SYS_gettid)) == static_cast<long>(getpid()) ? " (UI)" : "");
        if (n > 0) {
            [[maybe_unused]] auto w = write(fd, head, static_cast<size_t>(n));
        }
        void* frames[48];
        const int count = backtrace(frames, 48);
        backtrace_symbols_fd(frames, count, fd);
    }
    stackDone = true;
}

void installStackSignal() {
    void* warm[2];
    backtrace(warm, 2);  // loads libgcc now, not inside the signal handler
    struct sigaction sa {};
    sa.sa_handler = onStackSignal;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(STACK_SIGNAL, &sa, nullptr);
}

/// Every thread of the process writes its stack to `f`, the UI thread first
void dumpStacks(FILE* f) {
    fflush(f);
    stackFd = fileno(f);
    std::vector<long> tids{static_cast<long>(getpid())};
    if (DIR* d = opendir("/proc/self/task")) {
        while (dirent* e = readdir(d)) {
            const long tid = atol(e->d_name);
            if (tid > 0 && tid != tids.front() && tid != static_cast<long>(syscall(SYS_gettid))) {
                tids.push_back(tid);
            }
        }
        closedir(d);
    }
    for (long tid: tids) {
        stackDone = false;
        if (syscall(SYS_tgkill, getpid(), tid, STACK_SIGNAL) != 0) {
            continue;
        }
        for (int i = 0; i < 50 && !stackDone; i++) {  // (a thread blocked with signals masked never answers)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    stackFd = -1;
}
#endif

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
    int64_t hangReported = 0;  // when this stall was last reported while lasting (0: not yet)
    std::vector<std::string> seen;
    std::vector<std::string> seenBackground;
    while (running) {
        // Not stalled: sleep until a stall could have started (few wake-ups: this runs all the time, on laptops);
        // stalled: look often, to see what the UI thread is doing
        int64_t sleepUs = 10 * 1000;
        if (stallSince == 0) {
            sleepUs = std::max<int64_t>(sleepUs, lastBeat + THRESHOLD_US + BEAT_US - g_get_monotonic_time() + 1000);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(sleepUs));
        const int64_t beat = lastBeat;
        const int64_t now = g_get_monotonic_time();
        if (stallSince == 0) {
            if (now - beat > THRESHOLD_US + BEAT_US) {
                stallSince = beat;
                hangReported = 0;
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
            const std::string what = describe(seen, seenBackground);
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
            // A hang: report it while it lasts (a frozen app may be killed before it ends), with the stacks once
            const int64_t stuck = g_get_monotonic_time() - stallSince;
            if (logFile && stuck > HANG_US && stuck < SUSPEND_US &&
                (hangReported == 0 || g_get_monotonic_time() - hangReported > HANG_REPEAT_US)) {
                fprintf(logFile, "%lld HANG %llds so far: %s\n", static_cast<long long>(stallSince / 1000),
                        static_cast<long long>(stuck / G_USEC_PER_SEC), describe(seen, seenBackground).c_str());
#ifdef XOJ_STALL_STACKS
                if (hangReported == 0) {
                    dumpStacks(logFile);
                }
#endif
                fflush(logFile);
                hangReported = g_get_monotonic_time();
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
#ifdef XOJ_STALL_STACKS
        if (logFile) {  // (times in the log are ms since boot; this line ties them to the clock)
            char when[32];
            const time_t t = time(nullptr);
            tm local{};
            localtime_r(&t, &local);
            strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", &local);
            fprintf(logFile, "%lld start pid %d at %s\n", static_cast<long long>(g_get_monotonic_time() / 1000),
                    static_cast<int>(getpid()), when);
            fflush(logFile);
            installStackSignal();
        }
#endif
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
