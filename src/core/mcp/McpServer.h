/*
 * Xournal++ (xournalai)
 *
 * Embedded MCP server: lets AI agents (any MCP client) use the running application as a tool.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr
#include <string>  // for string

#include "McpConfig.h"
#include "Registry.h"

class Control;

namespace xoj::mcp {

class McpProtocol;
class McpHttpServer;
}  // namespace xoj::mcp

namespace xoj::api {
class EventHub;
}

namespace xoj::mcp {

/**
 * @brief Owner of the embedded MCP server (transport, protocol, tool registry).
 *
 * Owned by Control; created and started when the main window exists and stopped before Control tears down.
 * Everything runs on the GTK main thread.
 */
class McpServer final {
public:
    explicit McpServer(Control* control);
    ~McpServer();
    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    /// Starts listening if the server is enabled in the configuration (no-op otherwise)
    void start();
    /// Stops listening and drops all sessions
    void stop();
    bool isRunning() const;

    Control* getControl() const { return control; }
    Registry& getRegistry() { return registry; }
    McpHttpServer* getHttpServer() const { return http.get(); }
    /// Document change log (created when the server starts)
    api::EventHub* getEvents() const { return events.get(); }
    const McpConfig& getConfig() const { return config; }

    /// False once the server is destroyed; long-running tool calls check it before touching the server
    std::shared_ptr<bool> aliveToken() const { return alive; }

    /// Saves a safety copy of the document if backups are enabled; returns its path (empty if none was made)
    std::string backup(const std::string& reason) const;

    /// Throws ToolError if `tier` is not granted (for tools whose risk depends on their arguments)
    void requireTier(Tier tier, const std::string& what) const;

    /// Text sent to agents in the initialize response (how to use this server)
    static std::string instructions();
    static std::string permissionMessage(Tier tier, const std::string& what);

private:
    void registerTools();

    Control* control;
    Registry registry;
    McpConfig config;
    std::unique_ptr<McpProtocol> protocol;
    std::unique_ptr<McpHttpServer> http;
    std::unique_ptr<api::EventHub> events;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

}  // namespace xoj::mcp
