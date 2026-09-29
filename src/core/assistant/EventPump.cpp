#include "EventPump.h"

#include <algorithm>  // for find, min, max
#include <cmath>      // for lround

namespace xoj::assistant {

namespace {
constexpr guint TICK_MS = 250;
constexpr gint64 TYPING_QUIET_US = 8 * G_USEC_PER_SEC;  ///< don't type into the terminal while the user does
constexpr gint64 NO_HOOKS_GAP_US = 5 * G_USEC_PER_SEC;  ///< without hooks: at most one wake-up per this gap

xoj::util::Rectangle<double> unite(const xoj::util::Rectangle<double>& a, const xoj::util::Rectangle<double>& b) {
    if (a.width <= 0 && a.height <= 0) {
        return b;
    }
    const double x1 = std::min(a.x, b.x), y1 = std::min(a.y, b.y);
    const double x2 = std::max(a.x + a.width, b.x + b.width), y2 = std::max(a.y + a.height, b.y + b.height);
    return {x1, y1, x2 - x1, y2 - y1};
}
}  // namespace

EventPump::EventPump(Env e, Settings s): env(std::move(e)), settings(s) {
    timer = g_timeout_add(
            TICK_MS,
            [](gpointer self) -> gboolean {
                static_cast<EventPump*>(self)->tick();
                return G_SOURCE_CONTINUE;
            },
            this);
}

EventPump::~EventPump() {
    if (timer) {
        g_source_remove(timer);
    }
    if (enterSource) {
        g_source_remove(enterSource);
    }
}

void EventPump::onDocEvent(const api::DocEvent& e) {
    if (e.origin != "user" || e.type.rfind("element_", 0) != 0) {
        return;  // the agent's own work never wakes it; page events are not edits
    }
    if (!settings.autoImprove) {
        return;  // commands only: markers and buttons arrive as intents
    }
    if (editCount == 0) {
        firstCursor = e.seq - 1;
    }
    editCount += e.ids.empty() ? 1 : e.ids.size();
    if (std::find(editPages.begin(), editPages.end(), e.page) == editPages.end()) {
        editPages.push_back(e.page);
    }
    editArea = unite(editArea, e.area);
    lastEditUs = g_get_monotonic_time();
}

void EventPump::addIntent(const std::string& description, int zone) {
    intents.push_back(description);
    if (zone) {
        intentZones.push_back(zone);
    }
    tick();
}

void EventPump::setAutoImprove(bool on) {
    settings.autoImprove = on;
    if (!on) {
        editCount = 0;  // pending edits are dropped; commands only from now on
        editPages.clear();
        editArea = {0, 0, 0, 0};
    }
    if (env.changed) {
        env.changed();
    }
}

void EventPump::setRules(std::vector<std::string> r) {
    settings.rules = std::move(r);
    if (env.changed) {
        env.changed();
    }
}

size_t EventPump::pending() const { return (editCount > 0 ? 1 : 0) + intents.size(); }

std::string EventPump::message() const {
    std::string msg = "[xournalai] ";
    std::vector<std::string> parts;
    for (const auto& i: intents) {
        parts.push_back(i);
    }
    if (editCount > 0) {
        std::string pages;
        for (size_t p: editPages) {
            pages += (pages.empty() ? "" : ",") + std::to_string(p + 1);
        }
        auto r = [](double v) { return std::to_string(std::lround(v)); };
        parts.push_back("the user wrote/drew " + std::to_string(editCount) + " element(s) on page " + pages +
                        " around [" + r(editArea.x) + "," + r(editArea.y) + "," + r(editArea.width) + "," +
                        r(editArea.height) + "] (changes_get since=" + std::to_string(firstCursor) + ")");
    }
    for (size_t i = 0; i < parts.size(); i++) {
        msg += (i ? "; " : "") + parts[i];
    }
    msg += ". Auto-improve is " + std::string(settings.autoImprove ? "ON" : "OFF");
    if (settings.autoImprove) {
        std::string rules;
        for (const auto& r: settings.rules) {
            rules += (rules.empty() ? "" : ", ") + r;
        }
        msg += " (rules: " + (rules.empty() ? std::string("none") : rules) + ")";
    }
    msg += ".";
    return msg;
}

void EventPump::setStatus(std::string s) {
    if (s != statusText) {
        statusText = std::move(s);
        if (env.changed) {
            env.changed();
        }
    }
}

void EventPump::tick() {
    const gint64 now = g_get_monotonic_time();
    const bool running = env.processRunning && env.processRunning();
    const bool hooks = env.hooksSeen && env.hooksSeen();
    const ServingState::State st = env.state ? env.state() : ServingState::State::NotRunning;

    // A wake-up was sent: did the session pick it up?
    if (awaitingBusy) {
        if (st == ServingState::State::Busy || st == ServingState::State::Waiting) {
            awaitingBusy = false;
            resent = false;
            setStatus("");
        } else if (now - sentUs > static_cast<gint64>(settings.watchdogS) * G_USEC_PER_SEC) {
            if (!resent && running && env.type && env.type("\r")) {  // maybe the Enter got lost
                resent = true;
                sentUs = now;
                setStatus("the session didn't react; reminded it");
            } else {
                awaitingBusy = false;
                setStatus("the session isn't responding: restart it in the AI terminal");
            }
        }
        return;
    }
    if (pending() == 0) {
        return;
    }
    if (env.paused && env.paused()) {
        setStatus("paused: " + std::to_string(pending()) + " event(s) waiting");
        return;
    }
    if (!running) {
        setStatus("no serving session: " + std::to_string(pending()) + " event(s) waiting");
        return;
    }
    if (editCount > 0 && intents.empty() && now - lastEditUs < static_cast<gint64>(settings.idleMs) * 1000) {
        return;  // the user is still drawing: wait until they pause
    }
    if (hooks && st != ServingState::State::Idle) {
        return;  // busy or waiting for the user: deliver when it's idle again
    }
    if (!hooks && sentUs && now - sentUs < NO_HOOKS_GAP_US) {
        return;
    }
    if (env.lastTerminalInputUs && now - env.lastTerminalInputUs() < TYPING_QUIET_US) {
        setStatus("waiting: you're typing in the AI terminal");
        return;
    }
    const std::string msg = message();
    if (!env.type || !env.type(msg)) {
        return;
    }
    // Enter separately, so the text isn't taken for a paste
    if (enterSource) {
        g_source_remove(enterSource);
    }
    enterSource = g_timeout_add(
            150,
            [](gpointer self) -> gboolean {
                auto* p = static_cast<EventPump*>(self);
                p->enterSource = 0;
                if (p->env.type) {
                    p->env.type("\r");
                }
                return G_SOURCE_REMOVE;
            },
            this);
    if (env.delivered) {
        env.delivered(intentZones, editCount > 0, editPages.empty() ? 0 : editPages.front(), editArea);
    }
    intentZones.clear();
    lastMessage = msg;
    sentUs = now;
    awaitingBusy = hooks;
    resent = false;
    editCount = 0;
    editPages.clear();
    editArea = {0, 0, 0, 0};
    intents.clear();
    setStatus("");
}

}  // namespace xoj::assistant
