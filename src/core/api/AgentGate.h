/*
 * Xournal++ (xournalai)
 *
 * Global "the user paused the agent" switch, checked by long-running agent operations
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <atomic>

namespace xoj::api {

class AgentGate {
public:
    static bool paused() { return flag().load(); }
    static void setPaused(bool p) { flag().store(p); }

private:
    static std::atomic<bool>& flag() {
        static std::atomic<bool> f{false};
        return f;
    }
};

}  // namespace xoj::api
