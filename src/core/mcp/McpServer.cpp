#include "McpServer.h"

#include <glib.h>  // for g_message

#include "api/AgentGate.h"
#include "api/Backup.h"
#include "api/EventHub.h"
#include "assistant/Companion.h"
#include "tools/Tools.h"

#include "McpHttpServer.h"
#include "McpProtocol.h"
#include "McpUi.h"
#include "NotesStore.h"
#include "PathText.h"
#include "Transactions.h"
#include "config-features.h"  // for ENABLE_AI_TERMINAL
#include "config.h"           // for XOURNALAI_VERSION

namespace xoj::mcp {

McpServer::McpServer(Control* control): control(control), txns(std::make_unique<Transactions>(*this)) {
    registerTools();
    ServerInfo info;
    info.version = XOURNALAI_VERSION;
    info.instructions = instructions();
    protocol = std::make_unique<McpProtocol>(info, registry);
    protocol->setPermissionCheck([this](const ToolSpec& tool) -> std::optional<std::string> {
        if (api::AgentGate::paused() && tool.name != "app_status") {
            return "The user paused the AI agent in xournalai at " + api::AgentGate::pausedSince() +
                   " (the orange bar at the bottom of the window). Ask them to click \"Resume AI\" there (or press "
                   "Ctrl+Alt+Esc); app_status shows when you may continue.";
        }
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
        if (ui) {
            ui->toolStarted(name);
        }
    });
    protocol->setCallObserver([this, mutating](const std::string& name, bool, double) {
        if (events && mutating(name)) {
            events->endAgentWork();
        }
        if (ui) {
            ui->toolFinished();
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
    ui.reset();
    notes.reset();
    api::waitForBackups();  // don't quit while a safety copy is still being written
}

std::string McpServer::backup(const std::string& reason) const {
    if (!config.backups) {
        return {};
    }
    auto file = api::backupDocument(control, config.backupDir, reason);
    return file ? toUtf8(*file) : std::string();
}

std::string McpServer::instructions() {
    return "xournalai is the user's running note-taking app (handwriting, drawings, PDF annotation). Call it "
           "xournalai. "
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
    if (!notes) {
        notes = std::make_unique<NotesStore>(control);
    }
    if (!ui) {
        ui = std::make_unique<McpUi>(*this);
    }
    if (!config.enabled) {
        waiting.clear();
        g_message("MCP server disabled (see %s)", toUtf8(McpConfig::path()).c_str());
        return;
    }
    McpHttpServer::Options options;
    options.port = config.port;
    options.token = config.token;
    http = std::make_unique<McpHttpServer>(*protocol);
    std::string error;
    if (!http->start(options, error)) {
        // Usually another xournalai window holds the port: keep trying, take over when it is closed
        if (waiting.empty()) {
            g_warning("MCP server could not listen on 127.0.0.1:%u: %s (retrying every %u s)", options.port,
                      error.c_str(), RETRY_SECONDS);
        }
        waiting = "port " + std::to_string(options.port) + " is in use (another xournalai window?)";
        http.reset();
        if (!retrySource) {
            retrySource = g_timeout_add_seconds(
                    RETRY_SECONDS,
                    [](gpointer self) -> gboolean {
                        auto* s = static_cast<McpServer*>(self);
                        s->retrySource = 0;
                        s->start();
                        return G_SOURCE_REMOVE;
                    },
                    this);
        }
        ui->update();
        return;
    }
    waiting.clear();
    g_message("MCP server listening on http://127.0.0.1:%u/mcp", options.port);
    http->setHookHandler([this](const json& report) { servingState.apply(report); });
    // The serving session's folder follows this app (port, binary)
    {
        assistant::CompanionSetup setup;
        setup.port = options.port;
        if (char* self = g_file_read_link("/proc/self/exe", nullptr)) {
            setup.executable = self;
            g_free(self);
        } else {
            setup.executable = "xournalpp";
        }
        assistant::Companion::ensure(setup);
    }
#ifdef ENABLE_AI_TERMINAL
    if (config.assistant.autostart && ui) {
        ui->startServing();
    }
#endif
    tools::wireResourceNotifications(*this);
    ui->update();
}

void McpServer::stop() {
    if (retrySource) {
        g_source_remove(retrySource);
        retrySource = 0;
    }
    waiting.clear();
    events.reset();
    if (http) {
        http->stop();
        http.reset();
        g_message("MCP server stopped");
    }
}

void McpServer::restart() {
    // Keeps the change log, so agents' cursors stay valid
    if (http) {
        http->stop();
        http.reset();
    }
    start();
    if (ui) {
        ui->update();
    }
}

bool McpServer::isRunning() const { return http && http->isListening(); }

}  // namespace xoj::mcp
