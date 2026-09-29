#include "McpStdioBridge.h"

#include <algorithm>  // for find
#include <cstdio>     // for fwrite, fflush
#include <string>     // for string

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libsoup/soup.h>
#include <unistd.h>  // for setsid

#include "Json.h"
#include "McpConfig.h"
#include "McpProtocol.h"

namespace xoj::mcp {

namespace {

constexpr int MAX_CONNECT_ATTEMPTS = 80;  // x 250 ms = 20 s for the application to start
constexpr guint RETRY_MS = 250;
constexpr guint POLL_SECONDS = 3;  // while the application is not running: look for it this often

struct Bridge {
    McpConfig cfg;
    std::string url;
    std::string executable;
    SoupSession* http = nullptr;
    GMainLoop* loop = nullptr;
    GDataInputStream* in = nullptr;
    std::string sessionId;
    bool sseRunning = false;
    bool launched = false;
    bool stdinClosed = false;
    int inflight = 0;
    /// The application is not running: the bridge answers for it (no tools) and polls until it appears, then tells
    /// the client that the tools changed. Agents are configured once and work whenever the user starts the app.
    bool offline = false;
    guint pollSource = 0;
    json clientInit;  ///< the client's initialize params, replayed to the application when it appears
};

struct Pending {
    Bridge* bridge;
    std::string body;
    json id;  ///< null for notifications
    int attempt = 0;
    SoupMessage* msg = nullptr;
    std::string method;
};

void log(const std::string& text) { g_printerr("xournalai-mcp-stdio: %s\n", text.c_str()); }

void writeLine(const std::string& text) {
    std::string line = text;
    for (auto& c: line) {  // stdio framing: a message must not contain raw newlines
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    line += '\n';
    fwrite(line.data(), 1, line.size(), stdout);
    fflush(stdout);
}

void writeError(const json& id, const std::string& message) {
    if (!id.is_null()) {
        writeLine(rpc::errorResponse(id, rpc::INTERNAL_ERROR, message).dump());
    }
}

void maybeQuit(Bridge* b) {
    if (!b->stdinClosed || b->inflight > 0) {
        return;
    }
    if (!b->sessionId.empty()) {  // end the session politely
        SoupMessage* del = soup_message_new("DELETE", b->url.c_str());
        auto* h = soup_message_get_request_headers(del);
        soup_message_headers_replace(h, "Authorization", ("Bearer " + b->cfg.token).c_str());
        soup_message_headers_replace(h, "Mcp-Session-Id", b->sessionId.c_str());
        GBytes* r = soup_session_send_and_read(b->http, del, nullptr, nullptr);
        if (r) {
            g_bytes_unref(r);
        }
        g_object_unref(del);
    }
    g_main_loop_quit(b->loop);
}

void launchApplication(Bridge* b) {
    b->launched = true;
    std::string port = "--mcp-port=" + std::to_string(b->cfg.port);
    const char* argv[] = {b->executable.c_str(), "--mcp", port.c_str(), nullptr};
    GError* err = nullptr;
    const auto flags = static_cast<GSpawnFlags>(G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL);
    if (!g_spawn_async(
                nullptr, const_cast<char**>(argv), nullptr, flags, [](gpointer) { setsid(); }, nullptr, nullptr,
                &err)) {
        log(std::string("could not start ") + b->executable + ": " + (err ? err->message : "?"));
        if (err) {
            g_error_free(err);
        }
    } else {
        log("started " + b->executable + ", waiting for its MCP server on " + b->url);
    }
}

void startSse(Bridge* b);
void send(Pending* p);
void goOffline(Bridge* b);

/// Our answer to `initialize`: tools/prompts/resources may change (they appear when the application starts)
json advertiseChanges(json result) {
    if (result.is_object() && result.contains("capabilities")) {
        auto& caps = result["capabilities"];
        for (const char* k: {"tools", "prompts", "resources"}) {
            if (caps.contains(k) && caps[k].is_object()) {
                caps[k]["listChanged"] = true;
            }
        }
    }
    return result;
}

json offlineInitializeResult(const json& params) {
    const auto& versions = McpProtocol::supportedProtocolVersions();
    const std::string requested = params.value("protocolVersion", "");
    const bool known = std::find(versions.begin(), versions.end(), requested) != versions.end();
    return {{"protocolVersion", known ? requested : std::string(McpProtocol::LATEST_PROTOCOL_VERSION)},
            {"capabilities",
             {{"tools", {{"listChanged", true}}},
              {"prompts", {{"listChanged", true}}},
              {"resources", {{"subscribe", true}, {"listChanged", true}}},
              {"logging", json::object()}}},
            {"serverInfo", {{"name", "xournalai"}, {"title", "Xournal++ (xournalai)"}, {"version", "offline"}}},
            {"instructions", "xournalai (a note-taking app) is not running right now. Its tools appear as soon as "
                             "the user starts it; calling a tool also starts it."}};
}

/// Answers a request while the application is not running. Returns false if it has to go to the application.
bool answerOffline(Bridge* b, const std::string& method, const json& id) {
    if (method == "tools/call") {
        return false;  // an explicit request: start the application and forward
    }
    if (id.is_null()) {
        return true;  // notifications need no application
    }
    json result;
    if (method == "tools/list") {
        result = {{"tools", json::array()}};
    } else if (method == "prompts/list") {
        result = {{"prompts", json::array()}};
    } else if (method == "resources/list") {
        result = {{"resources", json::array()}};
    } else if (method == "resources/templates/list") {
        result = {{"resourceTemplates", json::array()}};
    } else if (method == "ping" || method == "logging/setLevel") {
        result = json::object();
    } else if (method == "initialize") {
        result = offlineInitializeResult(b->clientInit);
    } else {
        writeError(id, "xournalai is not running; ask the user to start it");
        return true;
    }
    writeLine(rpc::resultResponse(id, result).dump());
    return true;
}

void notifyListsChanged() {
    for (const char* m: {"notifications/tools/list_changed", "notifications/prompts/list_changed",
                         "notifications/resources/list_changed"}) {
        writeLine(json({{"jsonrpc", "2.0"}, {"method", m}}).dump());
    }
}

/// Offline: try to reach the application; once it answers, initialize a session for the client and tell it
gboolean poll(gpointer data) {
    auto* b = static_cast<Bridge*>(data);
    if (!b->offline || b->stdinClosed) {
        b->pollSource = 0;
        return G_SOURCE_REMOVE;
    }
    json init = {{"jsonrpc", "2.0"},
                 {"id", "xournalai-bridge-init"},
                 {"method", "initialize"},
                 {"params", b->clientInit.is_object() ?
                                    b->clientInit :
                                    json{{"protocolVersion", McpProtocol::LATEST_PROTOCOL_VERSION},
                                         {"capabilities", json::object()},
                                         {"clientInfo", {{"name", "xournalai-bridge"}, {"version", "1"}}}}}};
    SoupMessage* msg = soup_message_new("POST", b->url.c_str());
    auto* h = soup_message_get_request_headers(msg);
    soup_message_headers_replace(h, "Authorization", ("Bearer " + b->cfg.token).c_str());
    soup_message_headers_replace(h, "Accept", "application/json, text/event-stream");
    const std::string text = init.dump();
    GBytes* bytes = g_bytes_new(text.data(), text.size());
    soup_message_set_request_body_from_bytes(msg, "application/json", bytes);
    g_bytes_unref(bytes);
    GBytes* reply = soup_session_send_and_read(b->http, msg, nullptr, nullptr);  // local and quick
    const guint status = soup_message_get_status(msg);
    const char* sid = soup_message_headers_get_one(soup_message_get_response_headers(msg), "Mcp-Session-Id");
    if (reply && status == 200 && sid) {
        b->sessionId = sid;
        b->offline = false;
        b->launched = false;
        b->pollSource = 0;
        g_object_unref(msg);
        g_bytes_unref(reply);
        // complete the handshake, then let the client fetch the real tools
        auto* note = new Pending{b,       json({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}).dump(),
                                 json(),  0,
                                 nullptr, "notifications/initialized"};
        b->inflight++;
        send(note);
        log("xournalai is running: tools are available");
        notifyListsChanged();
        startSse(b);
        return G_SOURCE_REMOVE;
    }
    if (reply) {
        g_bytes_unref(reply);
    }
    g_object_unref(msg);
    return G_SOURCE_CONTINUE;
}

void goOffline(Bridge* b) {
    const bool wasOnline = !b->offline;
    b->offline = true;
    b->sessionId.clear();
    if (!b->pollSource) {
        b->pollSource = g_timeout_add_seconds(POLL_SECONDS, poll, b);
    }
    if (wasOnline) {
        notifyListsChanged();  // the tools are gone until the application is back
    }
}

void onSseLine(GObject* source, GAsyncResult* res, gpointer data) {
    auto* b = static_cast<Bridge*>(data);
    auto* stream = G_DATA_INPUT_STREAM(source);
    gsize len = 0;
    char* line = g_data_input_stream_read_line_finish_utf8(stream, res, &len, nullptr);
    if (!line) {  // stream closed: reconnect later
        g_object_unref(stream);
        b->sseRunning = false;
        g_timeout_add_seconds(
                2,
                [](gpointer d) {
                    startSse(static_cast<Bridge*>(d));
                    return G_SOURCE_REMOVE;
                },
                b);
        return;
    }
    if (g_str_has_prefix(line, "data:")) {
        const char* payload = line + 5;
        while (*payload == ' ') {
            payload++;
        }
        writeLine(payload);
    }
    g_free(line);
    g_data_input_stream_read_line_async(stream, G_PRIORITY_DEFAULT, nullptr, onSseLine, b);
}

void startSse(Bridge* b) {
    if (b->sseRunning || b->sessionId.empty() || b->stdinClosed) {
        return;
    }
    b->sseRunning = true;
    SoupMessage* msg = soup_message_new("GET", b->url.c_str());
    auto* h = soup_message_get_request_headers(msg);
    soup_message_headers_replace(h, "Authorization", ("Bearer " + b->cfg.token).c_str());
    soup_message_headers_replace(h, "Mcp-Session-Id", b->sessionId.c_str());
    soup_message_headers_replace(h, "Accept", "text/event-stream");
    soup_session_send_async(
            b->http, msg, G_PRIORITY_DEFAULT, nullptr,
            [](GObject* src, GAsyncResult* res, gpointer d) {
                auto* bridge = static_cast<Bridge*>(d);
                GInputStream* body = soup_session_send_finish(SOUP_SESSION(src), res, nullptr);
                if (!body) {
                    bridge->sseRunning = false;
                    return;
                }
                GDataInputStream* data = g_data_input_stream_new(body);
                g_object_unref(body);
                g_data_input_stream_read_line_async(data, G_PRIORITY_DEFAULT, nullptr, onSseLine, bridge);
            },
            b);
    g_object_unref(msg);
}

void onResponse(GObject* source, GAsyncResult* res, gpointer data) {
    auto* p = static_cast<Pending*>(data);
    Bridge* b = p->bridge;
    GError* err = nullptr;
    GBytes* bytes = soup_session_send_and_read_finish(SOUP_SESSION(source), res, &err);
    const guint status = soup_message_get_status(p->msg);

    if (!bytes) {
        const bool refused = err && g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED);
        const std::string reason = err ? err->message : "unknown error";
        if (err) {
            g_error_free(err);
        }
        g_object_unref(p->msg);
        p->msg = nullptr;
        if (refused && p->method != "tools/call") {
            // Not running: answer for it and wait for the user to start it (no window pops up by itself)
            if (!b->offline) {
                log("xournalai is not running; its tools appear when it starts");
            }
            goOffline(b);
            answerOffline(b, p->method, p->id);
            b->inflight--;
            delete p;
            maybeQuit(b);
            return;
        }
        if (refused && p->attempt < MAX_CONNECT_ATTEMPTS) {
            if (!b->launched) {
                launchApplication(b);
            }
            p->attempt++;
            g_timeout_add(
                    RETRY_MS,
                    [](gpointer d) {
                        send(static_cast<Pending*>(d));
                        return G_SOURCE_REMOVE;
                    },
                    p);
            return;
        }
        writeError(p->id, "xournalai is not reachable at " + b->url + ": " + reason);
        b->inflight--;
        delete p;
        maybeQuit(b);
        return;
    }

    gsize size = 0;
    const auto* text = static_cast<const char*>(g_bytes_get_data(bytes, &size));
    std::string body(text ? text : "", size);
    g_bytes_unref(bytes);

    if (const char* sid = soup_message_headers_get_one(soup_message_get_response_headers(p->msg), "Mcp-Session-Id")) {
        b->sessionId = sid;
    }
    g_object_unref(p->msg);

    b->launched = false;  // reachable again: if the user closes the app later, the next call may start it again
    if (b->offline && status < 500) {  // reached it while offline (a tool call started it)
        b->offline = false;
        notifyListsChanged();
    }
    if (status == 200 && !body.empty()) {
        if (p->method == "initialize") {
            json parsed = json::parse(body, nullptr, false);
            if (parsed.is_object() && parsed.contains("result")) {
                parsed["result"] = advertiseChanges(parsed["result"]);
                body = parsed.dump();
            }
        }
        writeLine(body);
        startSse(b);
    } else if (status != 202 && status != 200) {
        json parsed = json::parse(body, nullptr, false);
        std::string message = parsed.is_object() && parsed.contains("error") ?
                                      parsed["error"].value("message", body) :
                                      "HTTP " + std::to_string(status) + " " + body;
        if (status == 404) {
            b->sessionId.clear();  // unknown session (older servers): the next request starts a new one
        }
        writeError(p->id, message);
    }
    b->inflight--;
    delete p;
    maybeQuit(b);
}

void send(Pending* p) {
    Bridge* b = p->bridge;
    p->msg = soup_message_new("POST", b->url.c_str());
    auto* h = soup_message_get_request_headers(p->msg);
    soup_message_headers_replace(h, "Authorization", ("Bearer " + b->cfg.token).c_str());
    soup_message_headers_replace(h, "Accept", "application/json, text/event-stream");
    if (!b->sessionId.empty()) {
        soup_message_headers_replace(h, "Mcp-Session-Id", b->sessionId.c_str());
    }
    GBytes* bytes = g_bytes_new(p->body.data(), p->body.size());
    soup_message_set_request_body_from_bytes(p->msg, "application/json", bytes);
    g_bytes_unref(bytes);
    soup_session_send_and_read_async(b->http, p->msg, G_PRIORITY_DEFAULT, nullptr, onResponse, p);
}

void onStdinLine(GObject* source, GAsyncResult* res, gpointer data) {
    auto* b = static_cast<Bridge*>(data);
    gsize len = 0;
    char* line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(source), res, &len, nullptr);
    if (!line) {
        b->stdinClosed = true;
        maybeQuit(b);
        return;
    }
    std::string text(line, len);
    g_free(line);
    if (text.find_first_not_of(" \t\r") != std::string::npos) {
        json parsed = json::parse(text, nullptr, false);
        if (parsed.is_discarded()) {
            writeLine(rpc::errorResponse(nullptr, rpc::PARSE_ERROR, "Invalid JSON").dump());
        } else {
            const std::string method = parsed.is_object() ? parsed.value("method", "") : "";
            const json id = parsed.is_object() ? parsed.value("id", json()) : json();
            if (method == "initialize") {
                b->clientInit = parsed.value("params", json::object());
            }
            if (b->offline && answerOffline(b, method, id)) {
                // answered for the absent application
            } else {
                auto* p = new Pending{b, text, id, 0, nullptr, method};
                b->inflight++;
                send(p);
            }
        }
    }
    g_data_input_stream_read_line_async(b->in, G_PRIORITY_DEFAULT, nullptr, onStdinLine, b);
}

}  // namespace

int runStdioBridge(const char* executable) {
    Bridge b;
    b.cfg = McpConfig::load();
    b.url = b.cfg.url();
    if (executable && *executable) {
        b.executable = executable;
    } else if (char* self = g_file_read_link("/proc/self/exe", nullptr)) {
        b.executable = self;
        g_free(self);
    } else {
        b.executable = "xournalpp";
    }
    b.http = soup_session_new_with_options("timeout", 0u, "idle-timeout", 0u, nullptr);
    b.loop = g_main_loop_new(nullptr, false);
    GInputStream* stdinStream = g_unix_input_stream_new(0, false);
    b.in = g_data_input_stream_new(stdinStream);
    g_object_unref(stdinStream);

    g_data_input_stream_read_line_async(b.in, G_PRIORITY_DEFAULT, nullptr, onStdinLine, &b);
    g_main_loop_run(b.loop);

    g_object_unref(b.in);
    g_object_unref(b.http);
    g_main_loop_unref(b.loop);
    return 0;
}

}  // namespace xoj::mcp
