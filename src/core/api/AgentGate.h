/*
 * xournalai (based on Xournal++)
 *
 * Global "the user paused the agent" switch, checked by long-running agent operations
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>
#include <string>

#include <glib.h>  // for g_get_real_time, GDateTime

namespace xoj::api {

class AgentGate {
public:
    static bool paused() { return flag().load(); }
    static void setPaused(bool p) {
        if (p && !flag().load()) {
            sinceUs().store(g_get_real_time());
        }
        flag().store(p);
    }
    /// Local time the pause started, "HH:MM:SS" (empty when not paused)
    static std::string pausedSince() {
        if (!paused()) {
            return {};
        }
        GDateTime* t = g_date_time_new_from_unix_local(sinceUs().load() / G_USEC_PER_SEC);
        gchar* s = g_date_time_format(t, "%H:%M:%S");
        std::string out = s;
        g_free(s);
        g_date_time_unref(t);
        return out;
    }

    /// Window actions only the user may trigger (agents must not pause/resume themselves or change their rights)
    static bool userOnlyAction(const std::string& name) {
        const std::string bare = name.rfind("win.", 0) == 0 ? name.substr(4) : name;
        return bare == "mcp-paused" || bare == "mcp-settings";
    }
    /// Name of the settings dialog, which agent UI automation must not touch
    static constexpr const char* SETTINGS_DIALOG = "mcpSettingsDialog";

private:
    static std::atomic<gint64>& sinceUs() {
        static std::atomic<gint64> t{0};
        return t;
    }
    static std::atomic<bool>& flag() {
        static std::atomic<bool> f{false};
        return f;
    }
};

}  // namespace xoj::api
