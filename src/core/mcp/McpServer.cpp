#include "McpServer.h"

#include <glib.h>  // for g_message

namespace xoj::mcp {

McpServer::McpServer(Control* control): control(control) {}

McpServer::~McpServer() { stop(); }

void McpServer::start() {
    if (running) {
        return;
    }
    running = true;
    g_message("MCP server initialized");
}

void McpServer::stop() {
    if (!running) {
        return;
    }
    running = false;
    g_message("MCP server stopped");
}

bool McpServer::isRunning() const { return running; }

}  // namespace xoj::mcp
