#include "McpServer.h"

#include <cstdlib>  // for getenv

#include <glib.h>  // for g_message

#include "McpHttpServer.h"
#include "McpProtocol.h"
#include "config.h"  // for XOURNALAI_VERSION

namespace xoj::mcp {

McpServer::McpServer(Control* control): control(control) {
    registerTools();
    ServerInfo info;
    info.version = XOURNALAI_VERSION;
    info.instructions = instructions();
    protocol = std::make_unique<McpProtocol>(info, registry);
}

McpServer::~McpServer() { stop(); }

std::string McpServer::instructions() {
    return "xournalai is a running Xournal++ note-taking app (handwriting, drawings, PDF annotation). "
           "Coordinates are page points (1/72 inch), origin at the top-left of each page; pages are numbered "
           "from 1. Call the 'guide' tool for conventions and step-by-step recipes.";
}

void McpServer::registerTools() {}

void McpServer::start() {
    if (http && http->isListening()) {
        return;
    }
    McpHttpServer::Options options;
    if (const char* token = std::getenv("XOURNALAI_MCP_TOKEN")) {
        options.token = token;
    }
    http = std::make_unique<McpHttpServer>(*protocol);
    std::string error;
    if (!http->start(options, error)) {
        g_warning("MCP server could not listen on 127.0.0.1:%u: %s", options.port, error.c_str());
        http.reset();
        return;
    }
    g_message("MCP server listening on http://127.0.0.1:%u/mcp", options.port);
}

void McpServer::stop() {
    if (http) {
        http->stop();
        http.reset();
        g_message("MCP server stopped");
    }
}

bool McpServer::isRunning() const { return http && http->isListening(); }

}  // namespace xoj::mcp
