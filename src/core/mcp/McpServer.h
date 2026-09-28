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

#include "Registry.h"

class Control;

namespace xoj::mcp {

class McpProtocol;
class McpHttpServer;

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

    /// Text sent to agents in the initialize response (how to use this server)
    static std::string instructions();

private:
    void registerTools();

    Control* control;
    Registry registry;
    std::unique_ptr<McpProtocol> protocol;
    std::unique_ptr<McpHttpServer> http;
};

}  // namespace xoj::mcp
