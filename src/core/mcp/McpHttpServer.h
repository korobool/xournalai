/*
 * xournalai (based on Xournal++)
 *
 * MCP "Streamable HTTP" transport on libsoup 3, running on the GLib main loop
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for uint16_t
#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for unique_ptr
#include <string>      // for string
#include <vector>      // for vector

#include <glib.h>  // for guint

#include "Json.h"
#include "McpProtocol.h"

typedef struct _SoupServer SoupServer;
typedef struct _SoupServerMessage SoupServerMessage;

namespace xoj::mcp {

/**
 * @brief HTTP endpoint for MCP clients.
 *
 *  - POST   /mcp  JSON-RPC request or notification. Replies with application/json; replies to slow tools are held
 *                 open (the message is paused) until the tool responds.
 *  - GET    /mcp  Server-sent event stream for server-to-client notifications (e.g. resource updates).
 *  - DELETE /mcp  Ends the session.
 *
 * Security: listens on 127.0.0.1 only, requires "Authorization: Bearer <token>" when a token is set, and rejects
 * requests whose Origin/Host is not local (DNS-rebinding protection).
 */
class McpHttpServer final {
public:
    struct Options {
        uint16_t port = 7474;
        std::string token;  ///< empty = no authentication (not recommended)
    };

    explicit McpHttpServer(McpProtocol& protocol);
    ~McpHttpServer();
    McpHttpServer(const McpHttpServer&) = delete;
    McpHttpServer& operator=(const McpHttpServer&) = delete;

    /// Starts listening; returns false and sets `error` on failure (e.g. port in use)
    bool start(const Options& options, std::string& error);
    void stop();
    bool isListening() const { return server != nullptr; }
    uint16_t port() const { return options.port; }
    size_t sessionCount() const { return sessions.size(); }

    /// Sends a notification on the SSE streams of all sessions accepted by `filter` (all if empty)
    void broadcast(const json& notification, const std::function<bool(const Session&)>& filter = {});

private:
    struct SessionState;

    static void onRequest(SoupServer* server, SoupServerMessage* msg, const char* path, GHashTable* query,
                          gpointer self);
    static gboolean onKeepalive(gpointer self);

    void handlePost(SoupServerMessage* msg);
    void handleGet(SoupServerMessage* msg);
    void handleDelete(SoupServerMessage* msg);
    bool checkAccess(SoupServerMessage* msg);
    SessionState* lookupSession(SoupServerMessage* msg, bool createDefault);
    void sendSse(SoupServerMessage* stream, const std::string& chunk);
    static void respondJson(SoupServerMessage* msg, unsigned status, const json& body);

    McpProtocol& protocol;
    Options options;
    SoupServer* server = nullptr;
    guint keepaliveSource = 0;
    std::map<std::string, std::unique_ptr<SessionState>> sessions;
};

}  // namespace xoj::mcp
