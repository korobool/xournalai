/*
 * Xournal++ (xournalai)
 *
 * Embedded MCP server: lets AI agents (any MCP client) use the running application as a tool.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

class Control;

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

private:
    Control* control;
    bool running = false;
};

}  // namespace xoj::mcp
