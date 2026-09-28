#include "McpStdioBridge.h"

#include <cstdio>  // for fwrite, fflush
#include <string>  // for string

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
};

struct Pending {
    Bridge* bridge;
    std::string body;
    json id;  ///< null for notifications
    int attempt = 0;
    SoupMessage* msg = nullptr;
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

    if (status == 200 && !body.empty()) {
        writeLine(body);
        startSse(b);
    } else if (status != 202 && status != 200) {
        json parsed = json::parse(body, nullptr, false);
        std::string message = parsed.is_object() && parsed.contains("error") ?
                                      parsed["error"].value("message", body) :
                                      "HTTP " + std::to_string(status) + " " + body;
        if (status == 404) {
            b->sessionId.clear();  // the application restarted; the client has to initialize again
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
            auto* p = new Pending{b, text, parsed.is_object() ? parsed.value("id", json()) : json()};
            b->inflight++;
            send(p);
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
