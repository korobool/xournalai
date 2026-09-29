/*
 * xournalai (based on Xournal++)
 *
 * Opt-in trace of touch input, zoom steps and canvas redraws, for diagnosing pinch-zoom behaviour on real hardware.
 * On when the environment variable XOURNALAI_TRACE_TOUCH names a file, or when ~/.cache/xournalai/trace-touch
 * exists (then it writes ~/.cache/xournalai/touch-trace.log). One line per event: microseconds since start, text.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdarg>  // for va_list
#include <cstdio>   // for FILE, fopen, vfprintf

#include <glib.h>

namespace xoj::input {

inline FILE* touchTraceFile() {
    static FILE* f = [] {
        const char* env = g_getenv("XOURNALAI_TRACE_TOUCH");
        gchar* flag = g_build_filename(g_get_user_cache_dir(), "xournalai", "trace-touch", nullptr);
        gchar* log = g_build_filename(g_get_user_cache_dir(), "xournalai", "touch-trace.log", nullptr);
        FILE* out = nullptr;
        if (env && *env) {
            out = fopen(env, "w");
        } else if (g_file_test(flag, G_FILE_TEST_EXISTS)) {
            out = fopen(log, "w");
        }
        g_free(flag);
        g_free(log);
        return out;
    }();
    return f;
}

inline bool touchTraceOn() { return touchTraceFile() != nullptr; }

[[gnu::format(printf, 1, 2)]] inline void touchTrace(const char* fmt, ...) {
    FILE* f = touchTraceFile();
    if (!f) {
        return;
    }
    static const gint64 start = g_get_monotonic_time();
    fprintf(f, "%9lld ", static_cast<long long>(g_get_monotonic_time() - start));
    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fflush(f);
}

}  // namespace xoj::input
