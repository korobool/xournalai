/*
 * xournalai (based on Xournal++)
 *
 * The event pump: the app watches the canvas, the serving session works. Relevant events (the user's strokes when
 * Auto-improve is on, toolbar actions, markers) are collected, coalesced and delivered as ONE short wake-up line
 * typed into the idle session's terminal. The session handles them and goes idle again; nothing polls.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for uint64_t
#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include <glib.h>  // for gint64, guint

#include "api/EventHub.h"  // for DocEvent
#include "util/Rectangle.h"

#include "ServingState.h"

namespace xoj::assistant {

class EventPump final {
public:
    struct Env {
        std::function<bool()> paused;                  ///< the user paused the agent
        std::function<ServingState::State()> state;    ///< from the hooks (or process start/exit)
        std::function<bool()> hooksSeen;               ///< the session reports through hooks (Claude Code)
        std::function<bool()> processRunning;          ///< the serving terminal's process runs
        std::function<gint64()> lastTerminalInputUs;   ///< when the user last typed into the terminal
        std::function<bool(const std::string&)> type;  ///< types text into the serving terminal
        std::function<void()> changed;                 ///< the status text changed
    };
    struct Settings {
        bool autoImprove = false;
        int idleMs = 2500;
        int watchdogS = 20;
        std::vector<std::string> rules{"formulas", "text", "diagrams", "colours"};
    };

    EventPump(Env env, Settings settings);
    ~EventPump();
    EventPump(const EventPump&) = delete;
    EventPump& operator=(const EventPump&) = delete;

    /// Receives the document's change events (the user's edits)
    void onDocEvent(const api::DocEvent& e);
    /// An explicit request (toolbar button, marker): delivered as soon as the session is idle
    void addIntent(const std::string& description);

    void setAutoImprove(bool on);
    void setRules(std::vector<std::string> rules);
    const std::vector<std::string>& rules() const { return settings.rules; }
    bool autoImprove() const { return settings.autoImprove; }

    /// One line for the status strip ("" when there is nothing to say)
    const std::string& status() const { return statusText; }
    /// Number of events waiting to be delivered
    size_t pending() const;

    /// Runs the decision now (normally every 250 ms)
    void tick();

private:
    std::string message() const;
    void setStatus(std::string s);

    Env env;
    Settings settings;
    guint timer = 0;
    guint enterSource = 0;

    // user edits waiting (coalesced)
    size_t editCount = 0;
    std::vector<size_t> editPages;
    xoj::util::Rectangle<double> editArea{0, 0, 0, 0};
    uint64_t firstCursor = 0;
    gint64 lastEditUs = 0;
    std::vector<std::string> intents;

    // delivery
    std::string lastMessage;
    gint64 sentUs = 0;
    bool awaitingBusy = false;
    bool resent = false;
    std::string statusText;
};

}  // namespace xoj::assistant
