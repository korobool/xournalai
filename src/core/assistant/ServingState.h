/*
 * xournalai (based on Xournal++)
 *
 * What the serving session is doing, as reported by its Claude Code hooks (`xournalpp --ai-hook <event>`):
 * not running, idle, busy (with the current tool), or waiting for the user (a permission prompt).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <string>      // for string

#include <glib.h>  // for gint64

#include "mcp/Json.h"

namespace xoj::assistant {

class ServingState final {
public:
    enum class State { NotRunning, Idle, Busy, Waiting };

    /// Applies one hook report: {"event": "Stop", "payload": {...Claude Code's hook input...}}
    void apply(const mcp::json& report);
    /// The serving process started or exited (from the terminal)
    void processStarted();
    void processExited();

    State state() const { return current; }
    const std::string& tool() const { return currentTool; }
    const std::string& sessionId() const { return session; }
    gint64 changedUs() const { return changed; }
    /// The current process has reported through hooks (Claude Code does; Codex doesn't)
    bool hooksSeen() const { return sawHook; }
    /// Background subagents currently working (started via the Agent tool, until their SubagentStop)
    int subagents() const { return subagentCount; }
    /// The current (or last) turn handed work to a subagent (its zones stay until that work ends)
    bool delegated() const { return delegatedThisTurn; }
    static const char* name(State s);
    mcp::json toJson() const;

    /// Called after every change
    void setListener(std::function<void()> l) { listener = std::move(l); }

private:
    void set(State s, std::string tool = {});

    State current = State::NotRunning;
    std::string currentTool;
    std::string session;
    gint64 changed = 0;
    bool sawHook = false;
    int subagentCount = 0;
    bool delegatedThisTurn = false;
    std::function<void()> listener;
};

/// `xournalpp --ai-hook <event>`: reads Claude Code's hook input from stdin and reports it to the running app.
/// Always exits 0 quickly: a hook must never block or fail the session.
int runHook(const char* event);

}  // namespace xoj::assistant
