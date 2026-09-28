#include "McpServer.h"

#include <glib.h>  // for g_message

#include "api/Backup.h"
#include "api/EventHub.h"
#include "tools/Tools.h"

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
    // Changes made while a (non read-only) tool runs belong to the agent. Read-only tools such as wait_for_user
    // may run for minutes while the user draws, so they don't count.
    auto mutating = [this](const std::string& name) {
        const ToolSpec* t = registry.findTool(name);
        return t && !t->readOnly;
    };
    protocol->setCallStarted([this, mutating](const std::string& name) {
        if (events) {
            events->flush();  // earlier changes (by the user or a previous call) keep their attribution
            if (mutating(name)) {
                events->beginAgentWork();
            }
        }
    });
    protocol->setCallObserver([this, mutating](const std::string& name, bool, double) {
        if (events && mutating(name)) {
            events->endAgentWork();
        }
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

McpServer::~McpServer() {
    *alive = false;
    stop();
}

std::string McpServer::backup(const std::string& reason) const {
    if (!config.backups) {
        return {};
    }
    auto file = api::backupDocument(control, config.backupDir, reason);
    return file ? toUtf8(*file) : std::string();
}

std::string McpServer::instructions() {
    return "xournalai is a running Xournal++ note-taking app (handwriting, drawings, PDF annotation). "
           "Coordinates are page points (1/72 inch), origin at the top-left of each page; pages are numbered "
           "from 1. Call the 'guide' tool for conventions and step-by-step recipes.";
}

void McpServer::registerTools() { tools::registerAll(*this); }

void McpServer::start() {
    if (http && http->isListening()) {
        return;
    }
    config = McpConfig::load();
    if (!events) {
        events = std::make_unique<api::EventHub>(control);
    }
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
    tools::wireResourceNotifications(*this);
}

void McpServer::stop() {
    events.reset();
    if (http) {
        http->stop();
        http.reset();
        g_message("MCP server stopped");
    }
}

bool McpServer::isRunning() const { return http && http->isListening(); }

}  // namespace xoj::mcp
