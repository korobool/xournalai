#include "McpHttpServer.h"

#include <algorithm>  // for remove
#include <random>     // for random_device

#include <libsoup/soup.h>

namespace xoj::mcp {

constexpr const char* SESSION_HEADER = "Mcp-Session-Id";
constexpr const char* DEFAULT_SESSION = "default";
constexpr size_t MAX_SESSIONS = 64;
constexpr gsize MAX_REQUEST_BYTES = 64 * 1024 * 1024;  ///< generous: imports may carry base64 images
constexpr guint KEEPALIVE_SECONDS = 20;

struct McpHttpServer::SessionState {
    Session session;
    std::vector<SoupServerMessage*> streams;  ///< open SSE streams (referenced)
    gint64 lastSeen = 0;
};

namespace {

std::string randomHex(size_t bytes) {
    std::random_device rd;
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < bytes; i++) {
        const auto b = static_cast<unsigned>(rd()) & 0xffU;
        out += hex[b >> 4];
        out += hex[b & 0xfU];
    }
    return out;
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) {
        diff = static_cast<unsigned char>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

/// True if a "host[:port]" or "scheme://host[:port]" value names this machine's loopback interface
bool isLocal(std::string value) {
    if (auto p = value.find("://"); p != std::string::npos) {
        value = value.substr(p + 3);
    }
    if (!value.empty() && value[0] == '[') {  // [::1]:port
        return value.rfind("[::1]", 0) == 0;
    }
    value = value.substr(0, value.find(':'));
    value = value.substr(0, value.find('/'));
    return value == "127.0.0.1" || value == "localhost";
}

/// Pending reply to a POST, kept alive until the protocol answers or the client disconnects. The server may be
/// stopped (settings applied) while a tool is still working: `server` is a weak pointer, null once it is gone.
struct PendingReply {
    SoupServer* server = nullptr;
    SoupServerMessage* msg = nullptr;
    bool paused = false;
    bool replied = false;
    bool gone = false;
    gulong finishedHandler = 0;

    explicit PendingReply(SoupServer* s): server(s) {
        g_object_add_weak_pointer(G_OBJECT(server), reinterpret_cast<gpointer*>(&server));
    }
    ~PendingReply() {
        if (server) {
            g_object_remove_weak_pointer(G_OBJECT(server), reinterpret_cast<gpointer*>(&server));
        }
    }
    PendingReply(const PendingReply&) = delete;
    PendingReply& operator=(const PendingReply&) = delete;
};

}  // namespace

McpHttpServer::McpHttpServer(McpProtocol& protocol): protocol(protocol) {}

McpHttpServer::~McpHttpServer() { stop(); }

bool McpHttpServer::start(const Options& opts, std::string& error) {
    stop();
    options = opts;
    server = soup_server_new("server-header", "xournalai-mcp ", nullptr);
    soup_server_add_handler(server, nullptr, &McpHttpServer::onRequest, this, nullptr);
    GError* err = nullptr;
    if (!soup_server_listen_local(server, options.port, SOUP_SERVER_LISTEN_IPV4_ONLY, &err)) {
        error = err ? err->message : "unknown error";
        if (err) {
            g_error_free(err);
        }
        g_object_unref(server);
        server = nullptr;
        return false;
    }
    keepaliveSource = g_timeout_add_seconds(KEEPALIVE_SECONDS, &McpHttpServer::onKeepalive, this);
    return true;
}

void McpHttpServer::stop() {
    if (keepaliveSource) {
        g_source_remove(keepaliveSource);
        keepaliveSource = 0;
    }
    for (auto& [id, s]: sessions) {
        for (auto* stream: s->streams) {
            soup_message_body_complete(soup_server_message_get_response_body(stream));
            if (server) {
                soup_server_unpause_message(server, stream);
            }
            g_object_unref(stream);
        }
    }
    sessions.clear();
    if (server) {
        soup_server_disconnect(server);
        g_object_unref(server);
        server = nullptr;
    }
}

void McpHttpServer::respondJson(SoupServerMessage* msg, unsigned status, const json& body) {
    const std::string text = body.is_null() ? std::string() : body.dump();
    soup_server_message_set_status(msg, status, nullptr);
    if (!text.empty()) {
        soup_server_message_set_response(msg, "application/json", SOUP_MEMORY_COPY, text.data(), text.size());
    }
}

void McpHttpServer::onRequest(SoupServer*, SoupServerMessage* msg, const char* path, GHashTable*, gpointer data) {
    auto* self = static_cast<McpHttpServer*>(data);
    const std::string p = path ? path : "";
    const std::string method = soup_server_message_get_method(msg);

    if (p == "/" && method == "GET") {
        static const std::string info = "xournalai MCP server. Connect an MCP client (Streamable HTTP) to /mcp.\n";
        soup_server_message_set_status(msg, 200, nullptr);
        soup_server_message_set_response(msg, "text/plain", SOUP_MEMORY_STATIC, info.data(), info.size());
        return;
    }
    if (p != "/mcp") {
        soup_server_message_set_status(msg, 404, nullptr);
        return;
    }
    if (!self->checkAccess(msg)) {
        return;
    }
    if (method == "POST") {
        self->handlePost(msg);
    } else if (method == "GET") {
        self->handleGet(msg);
    } else if (method == "DELETE") {
        self->handleDelete(msg);
    } else {
        soup_server_message_set_status(msg, 405, nullptr);
    }
}

bool McpHttpServer::checkAccess(SoupServerMessage* msg) {
    auto* headers = soup_server_message_get_request_headers(msg);
    const char* origin = soup_message_headers_get_one(headers, "Origin");
    const char* host = soup_message_headers_get_one(headers, "Host");
    if ((origin && !isLocal(origin)) || (host && !isLocal(host))) {
        respondJson(msg, 403, rpc::errorResponse(nullptr, rpc::INVALID_REQUEST, "Forbidden origin"));
        return false;
    }
    if (!options.token.empty()) {
        const char* auth = soup_message_headers_get_one(headers, "Authorization");
        if (!auth || !constantTimeEquals(auth, "Bearer " + options.token)) {
            soup_message_headers_replace(soup_server_message_get_response_headers(msg), "WWW-Authenticate",
                                         "Bearer realm=\"xournalai\"");
            respondJson(msg, 401,
                        rpc::errorResponse(nullptr, rpc::INVALID_REQUEST,
                                           "Missing or wrong bearer token. See ~/.config/xournalpp/mcp.json"));
            return false;
        }
    }
    return true;
}

McpHttpServer::SessionState* McpHttpServer::lookupSession(SoupServerMessage* msg, bool createDefault) {
    const char* id = soup_message_headers_get_one(soup_server_message_get_request_headers(msg), SESSION_HEADER);
    std::string key = id ? id : DEFAULT_SESSION;
    auto it = sessions.find(key);
    if (it == sessions.end()) {
        if (!createDefault) {
            return nullptr;
        }
        if (id && (key.empty() || key.size() > 128)) {
            return nullptr;
        }
        // A session id we don't know: the app was restarted (or the server re-applied its settings) while the agent
        // stayed connected. The caller holds the token, so adopt the id instead of answering 404: many clients do
        // not re-initialize and would otherwise stay disconnected until the user restarts them.
        auto state = std::make_unique<SessionState>();
        state->session.id = key;
        if (id) {
            state->session.initialized = true;
            state->session.protocolVersion = McpProtocol::supportedProtocolVersions().front();
            g_message("MCP: resumed session %s after a restart", key.c_str());
        }
        it = sessions.emplace(key, std::move(state)).first;
    }
    it->second->lastSeen = g_get_monotonic_time();
    return it->second.get();
}

void McpHttpServer::handlePost(SoupServerMessage* msg) {
    auto* body = soup_server_message_get_request_body(msg);
    GBytes* bytes = soup_message_body_flatten(body);
    gsize size = 0;
    const auto* data = static_cast<const char*>(g_bytes_get_data(bytes, &size));
    if (size > MAX_REQUEST_BYTES) {
        g_bytes_unref(bytes);
        respondJson(msg, 413,
                    rpc::errorResponse(nullptr, rpc::INVALID_REQUEST,
                                       "Request too large (at most 64 MB); pass big files by path instead"));
        return;
    }
    json message = json::parse(data, data + size, nullptr, false);
    g_bytes_unref(bytes);
    if (message.is_discarded()) {
        respondJson(msg, 400, rpc::errorResponse(nullptr, rpc::PARSE_ERROR, "Invalid JSON"));
        return;
    }
    if (message.is_array()) {
        respondJson(msg, 400, rpc::errorResponse(nullptr, rpc::INVALID_REQUEST, "JSON-RPC batches are not supported"));
        return;
    }

    SessionState* state = nullptr;
    const bool isInitialize = message.is_object() && message.value("method", "") == "initialize";
    if (isInitialize) {
        if (sessions.size() >= MAX_SESSIONS) {  // evict the least recently used session
            auto oldest = std::min_element(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) {
                return a.second->lastSeen < b.second->lastSeen;
            });
            if (oldest->second->streams.empty()) {
                sessions.erase(oldest);
            }
        }
        auto fresh = std::make_unique<SessionState>();
        fresh->session.id = randomHex(16);
        fresh->lastSeen = g_get_monotonic_time();
        state = fresh.get();
        sessions.emplace(fresh->session.id, std::move(fresh));
        soup_message_headers_replace(soup_server_message_get_response_headers(msg), SESSION_HEADER,
                                     state->session.id.c_str());
    } else {
        state = lookupSession(msg, true);
        if (!state) {
            respondJson(msg, 404, rpc::errorResponse(nullptr, rpc::INVALID_REQUEST, "Unknown session, re-initialize"));
            return;
        }
    }

    auto pending = std::make_shared<PendingReply>(server);
    pending->msg = msg;
    g_object_ref(msg);
    // "finished" fires when the exchange ends, including when the client disconnects early
    auto* finishedFlag = new std::shared_ptr<PendingReply>(pending);
    pending->finishedHandler = g_signal_connect_data(
            msg, "finished", G_CALLBACK(+[](SoupServerMessage*, gpointer d) {
                (*static_cast<std::shared_ptr<PendingReply>*>(d))->gone = true;
            }),
            finishedFlag, +[](gpointer d, GClosure*) { delete static_cast<std::shared_ptr<PendingReply>*>(d); },
            static_cast<GConnectFlags>(0));

    const bool willReply = protocol.handle(message, state->session, [pending](json response) {
        if (pending->replied) {
            return;
        }
        pending->replied = true;
        if (!pending->gone && pending->server) {  // not if the client left or the server was restarted
            respondJson(pending->msg, 200, response);
            if (pending->paused) {
                soup_server_unpause_message(pending->server, pending->msg);
            }
        }
        g_object_unref(pending->msg);
    });

    if (!willReply) {
        soup_server_message_set_status(msg, 202, nullptr);  // notification or client response accepted
        g_object_unref(msg);
        pending->replied = true;
    } else if (!pending->replied) {
        pending->paused = true;  // the tool answers later
        soup_server_pause_message(server, msg);
    }
}

void McpHttpServer::handleGet(SoupServerMessage* msg) {
    const char* accept = soup_message_headers_get_one(soup_server_message_get_request_headers(msg), "Accept");
    if (!accept || !g_strrstr(accept, "text/event-stream")) {
        soup_server_message_set_status(msg, 405, nullptr);
        return;
    }
    SessionState* state = lookupSession(msg, true);
    if (!state) {
        soup_server_message_set_status(msg, 404, nullptr);
        return;
    }
    auto* headers = soup_server_message_get_response_headers(msg);
    soup_message_headers_set_encoding(headers, SOUP_ENCODING_CHUNKED);
    soup_message_headers_set_content_type(headers, "text/event-stream", nullptr);
    soup_message_headers_replace(headers, "Cache-Control", "no-cache");
    soup_server_message_set_status(msg, 200, nullptr);
    soup_message_body_set_accumulate(soup_server_message_get_response_body(msg), FALSE);

    g_object_ref(msg);
    state->streams.push_back(msg);
    const std::string sessionId = state->session.id;
    struct StreamRef {
        McpHttpServer* self;
        std::string session;
    };
    g_signal_connect_data(
            msg, "finished", G_CALLBACK(+[](SoupServerMessage* m, gpointer d) {
                auto* ref = static_cast<StreamRef*>(d);
                auto it = ref->self->sessions.find(ref->session);
                if (it != ref->self->sessions.end()) {
                    auto& streams = it->second->streams;
                    if (auto s = std::find(streams.begin(), streams.end(), m); s != streams.end()) {
                        streams.erase(s);
                        g_object_unref(m);
                    }
                }
            }),
            new StreamRef{this, sessionId}, +[](gpointer d, GClosure*) { delete static_cast<StreamRef*>(d); },
            static_cast<GConnectFlags>(0));
    sendSse(msg, ": connected\n\n");
}

void McpHttpServer::handleDelete(SoupServerMessage* msg) {
    const char* id = soup_message_headers_get_one(soup_server_message_get_request_headers(msg), SESSION_HEADER);
    if (!id || !sessions.count(id)) {
        soup_server_message_set_status(msg, 404, nullptr);
        return;
    }
    auto& state = sessions[id];
    for (auto* stream: state->streams) {
        soup_message_body_complete(soup_server_message_get_response_body(stream));
        soup_server_unpause_message(server, stream);
    }
    // streams are released by their "finished" handlers, which look the session up by id; keep references valid
    for (auto* stream: state->streams) {
        g_object_unref(stream);
    }
    state->streams.clear();
    sessions.erase(id);
    soup_server_message_set_status(msg, 200, nullptr);
}

void McpHttpServer::sendSse(SoupServerMessage* stream, const std::string& chunk) {
    soup_message_body_append(soup_server_message_get_response_body(stream), SOUP_MEMORY_COPY, chunk.data(),
                             chunk.size());
    soup_server_unpause_message(server, stream);
}

void McpHttpServer::broadcast(const json& notification, const std::function<bool(const Session&)>& filter) {
    const std::string chunk = "event: message\ndata: " + notification.dump() + "\n\n";
    for (auto& [id, s]: sessions) {
        if (filter && !filter(s->session)) {
            continue;
        }
        for (auto* stream: s->streams) {
            sendSse(stream, chunk);
        }
    }
}

gboolean McpHttpServer::onKeepalive(gpointer data) {
    auto* self = static_cast<McpHttpServer*>(data);
    for (auto& [id, s]: self->sessions) {
        for (auto* stream: s->streams) {
            self->sendSse(stream, ": keepalive\n\n");
        }
    }
    return G_SOURCE_CONTINUE;
}

}  // namespace xoj::mcp
