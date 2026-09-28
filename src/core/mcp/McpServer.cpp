#include "McpServer.h"

#include <glib.h>  // for g_message

#include "McpHttpServer.h"
#include "McpProtocol.h"
#include "PathText.h"
#include "config.h"  // for XOURNALAI_VERSION

namespace xoj::mcp {

McpServer::McpServer(Control* control): control(control) {
    registerTools();
    ServerInfo info;
    info.version = XOURNALAI_VERSION;
    info.instructions = instructions();
    protocol = std::make_unique<McpProtocol>(info, registry);
    protocol->setPermissionCheck([this](const ToolSpec& tool) -> std::optional<std::string> {
        if (config.allows(tool.tier)) {
            return std::nullopt;
        }
        return permissionMessage(tool.tier, "tool '" + tool.name + "'");
    });
}

std::string McpServer::permissionMessage(Tier tier, const std::string& what) {
    return std::string("Permission denied: ") + what + " needs the '" + tierName(tier) +
           "' permission. The user can grant it in " + toUtf8(McpConfig::path()) + " (\"permissions\": {\"" +
           tierName(tier) + "\": true}) and restart xournalai.";
}

void McpServer::requireTier(Tier tier, const std::string& what) const {
    if (!config.allows(tier)) {
        throw ToolError(permissionMessage(tier, what));
    }
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
    config = McpConfig::load();
    if (!config.enabled) {
        g_message("MCP server disabled (see %s)", toUtf8(McpConfig::path()).c_str());
        return;
    }
    McpHttpServer::Options options;
    options.port = config.port;
    options.token = config.token;
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
