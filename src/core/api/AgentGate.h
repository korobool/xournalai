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

namespace xoj::api {

class AgentGate {
public:
    static bool paused() { return flag().load(); }
    static void setPaused(bool p) { flag().store(p); }

    /// Window actions only the user may trigger (agents must not pause/resume themselves or change their rights)
    static bool userOnlyAction(const std::string& name) {
        const std::string bare = name.rfind("win.", 0) == 0 ? name.substr(4) : name;
        return bare == "mcp-paused" || bare == "mcp-settings";
    }
    /// Name of the settings dialog, which agent UI automation must not touch
    static constexpr const char* SETTINGS_DIALOG = "mcpSettingsDialog";

private:
    static std::atomic<bool>& flag() {
        static std::atomic<bool> f{false};
        return f;
    }
};

}  // namespace xoj::api
