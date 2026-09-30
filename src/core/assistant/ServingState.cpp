#include "ServingState.h"

#include <algorithm>  // for max
#include <iostream>   // for cin
#include <iterator>   // for istreambuf_iterator

#include <libsoup/soup.h>
#include <unistd.h>  // for isatty

#include "mcp/McpConfig.h"

namespace xoj::assistant {

const char* ServingState::name(State s) {
    switch (s) {
        case State::NotRunning:
            return "not_running";
        case State::Idle:
            return "idle";
        case State::Busy:
            return "busy";
        case State::Waiting:
            return "waiting";
    }
    return "unknown";
}

void ServingState::set(State s, std::string tool) {
    current = s;
    currentTool = std::move(tool);
    changed = g_get_monotonic_time();
    if (listener) {
        listener();
    }
}

void ServingState::processStarted() {
    sawHook = false;
    set(State::Idle);
}

void ServingState::processExited() {
    sawHook = false;
    set(State::NotRunning);
}

void ServingState::apply(const mcp::json& report) {
    const std::string event = report.value("event", "");
    sawHook = true;
    const mcp::json payload = report.value("payload", mcp::json::object());
    if (payload.is_object() && payload.contains("session_id") && payload["session_id"].is_string()) {
        session = payload["session_id"].get<std::string>();
    }
    if (event == "SubagentStop") {
        subagentCount = std::max(0, subagentCount - 1);  // the coordinator's own state is unchanged
        set(current, currentTool);
    } else if (event == "SessionStart" || event == "SessionEnd") {
        subagentCount = 0;
        set(event == "SessionStart" ? State::Idle : State::NotRunning);
    } else if (event == "Stop") {
        set(State::Idle);
    } else if (event == "UserPromptSubmit") {
        delegatedThisTurn = false;
        set(State::Busy);
    } else if (event == "PreToolUse") {
        std::string tool = payload.is_object() ? payload.value("tool_name", "") : "";
        if (tool == "Agent" || tool == "Task") {
            subagentCount++;  // a subagent starts (it reports SubagentStop when it's done)
            delegatedThisTurn = true;
        }
        if (tool.rfind("mcp__xournalai__", 0) == 0) {
            tool = tool.substr(16);
        }
        set(State::Busy, tool);
    } else if (event == "PostToolUse") {
        set(State::Busy);
    } else if (event == "Notification") {
        // Claude Code notifies on permission prompts and when it has been waiting for input a while
        const std::string msg = payload.is_object() ? payload.value("message", "") : "";
        set(msg.find("permission") != std::string::npos ? State::Waiting : State::Idle);
    }
}

mcp::json ServingState::toJson() const {
    mcp::json j = {{"state", name(current)}};
    if (!currentTool.empty()) {
        j["tool"] = currentTool;
    }
    if (!session.empty()) {
        j["session_id"] = session;
    }
    if (subagentCount > 0) {
        j["subagents"] = subagentCount;
    }
    return j;
}

int runHook(const char* event) {
    // The hook input (JSON) arrives on stdin; never wait on an interactive terminal
    std::string input;
    if (!isatty(0)) {
        input.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
    }
    mcp::json payload = mcp::json::parse(input, nullptr, false);
    if (payload.is_discarded()) {
        payload = mcp::json::object();
    }
    const mcp::McpConfig cfg = mcp::McpConfig::load();
    uint16_t port = cfg.port;
    if (const char* p = g_getenv("XOURNALAI_PORT")) {
        port = static_cast<uint16_t>(g_ascii_strtoull(p, nullptr, 10));
    }
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + "/ai-hook";
    const std::string body = mcp::json({{"event", event ? event : ""}, {"payload", payload}}).dump();

    SoupSession* http = soup_session_new_with_options("timeout", 2u, nullptr);
    SoupMessage* msg = soup_message_new("POST", url.c_str());
    if (msg) {
        soup_message_headers_replace(soup_message_get_request_headers(msg), "Authorization",
                                     ("Bearer " + cfg.token).c_str());
        GBytes* bytes = g_bytes_new(body.data(), body.size());
        soup_message_set_request_body_from_bytes(msg, "application/json", bytes);
        g_bytes_unref(bytes);
        GBytes* reply = soup_session_send_and_read(http, msg, nullptr, nullptr);
        if (reply) {
            g_bytes_unref(reply);
        }
        g_object_unref(msg);
    }
    g_object_unref(http);
    return 0;
}

}  // namespace xoj::assistant
