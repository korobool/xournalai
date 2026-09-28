/*
 * Xournal++ (xournalai)
 *
 * JSON-RPC 2.0 / Model Context Protocol message handling, independent of the transport
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <optional>    // for optional
#include <set>         // for set
#include <string>      // for string
#include <vector>      // for vector

#include "Json.h"
#include "Registry.h"

namespace xoj::mcp {

/// JSON-RPC error codes
namespace rpc {
constexpr int PARSE_ERROR = -32700;
constexpr int INVALID_REQUEST = -32600;
constexpr int METHOD_NOT_FOUND = -32601;
constexpr int INVALID_PARAMS = -32602;
constexpr int INTERNAL_ERROR = -32603;
constexpr int RESOURCE_NOT_FOUND = -32002;

json errorResponse(const json& id, int code, const std::string& message);
json resultResponse(const json& id, json result);
json notification(const std::string& method, json params);
}  // namespace rpc

/// State of one client connection
struct Session {
    std::string id;
    std::string protocolVersion;
    json clientInfo = json::object();
    json clientCapabilities = json::object();
    bool initialized = false;
    std::set<std::string> subscriptions;  ///< subscribed resource URIs
};

struct ServerInfo {
    std::string name = "xournalai";
    std::string title = "Xournal++ (xournalai)";
    std::string version;
    std::string instructions;
};

class McpProtocol {
public:
    using Reply = std::function<void(json response)>;
    /// Returns an error message if the tool may not be called (e.g. permission tier not granted)
    using PermissionCheck = std::function<std::optional<std::string>(const ToolSpec&)>;
    /// Observes finished tool calls (name, success, milliseconds); used for logging and the status indicator
    using CallObserver = std::function<void(const std::string& tool, bool ok, double ms)>;

    McpProtocol(ServerInfo info, const Registry& registry);

    void setPermissionCheck(PermissionCheck check) { permissionCheck = std::move(check); }
    void setCallObserver(CallObserver observer) { callObserver = std::move(observer); }
    /// Called right before a tool handler runs (paired with the call observer when it finishes)
    void setCallStarted(std::function<void(const std::string& tool)> started) { callStarted = std::move(started); }

    /**
     * @brief Handles one incoming JSON-RPC message.
     * @return true iff the message is a request, in which case `reply` is called exactly once (maybe later).
     *         Notifications and client responses return false and never reply.
     */
    bool handle(const json& message, Session& session, Reply reply);

    static const std::vector<std::string>& supportedProtocolVersions();
    static constexpr const char* LATEST_PROTOCOL_VERSION = "2025-06-18";

private:
    json initialize(const json& params, Session& session) const;
    void callTool(const json& id, const json& params, Reply reply);
    json getPrompt(const json& params) const;
    std::optional<json> readResource(const std::string& uri) const;

    ServerInfo info;
    const Registry& registry;
    PermissionCheck permissionCheck;
    CallObserver callObserver;
    std::function<void(const std::string& tool)> callStarted;
};

}  // namespace xoj::mcp
