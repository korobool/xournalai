#include "McpProtocol.h"

#include <algorithm>  // for find
#include <chrono>     // for steady_clock
#include <memory>     // for make_shared
#include <stdexcept>  // for invalid_argument

#include <glib.h>  // for g_main_depth

#include "util/StallWatch.h"  // for stall::Activity

namespace xoj::mcp {

namespace rpc {
json errorResponse(const json& id, int code, const std::string& message) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
}

json resultResponse(const json& id, json result) {
    return {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}};
}

json notification(const std::string& method, json params) {
    return {{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}};
}
}  // namespace rpc

const std::vector<std::string>& McpProtocol::supportedProtocolVersions() {
    static const std::vector<std::string> versions = {"2025-06-18", "2025-03-26", "2024-11-05"};
    return versions;
}

McpProtocol::McpProtocol(ServerInfo info, const Registry& registry): info(std::move(info)), registry(registry) {}

bool McpProtocol::handle(const json& message, Session& session, Reply reply) {
    if (!message.is_object()) {
        reply(rpc::errorResponse(nullptr, rpc::INVALID_REQUEST, "Message must be a JSON object"));
        return true;
    }
    const bool hasId = message.contains("id") && (message["id"].is_string() || message["id"].is_number());
    auto methodIt = message.find("method");
    if (methodIt == message.end()) {
        return false;  // a response from the client (we never send requests yet) - ignore
    }
    if (!methodIt->is_string() || message.value("jsonrpc", "") != "2.0") {
        if (hasId) {
            reply(rpc::errorResponse(message["id"], rpc::INVALID_REQUEST, "Invalid JSON-RPC 2.0 request"));
            return true;
        }
        return false;
    }
    const std::string method = methodIt->get<std::string>();
    const json params = message.value("params", json::object());

    if (!hasId) {  // notification
        if (method == "notifications/initialized") {
            session.initialized = true;
        }
        return false;
    }
    const json id = message["id"];

    try {
        if (method == "initialize") {
            reply(rpc::resultResponse(id, initialize(params, session)));
        } else if (method == "ping") {
            reply(rpc::resultResponse(id, json::object()));
        } else if (method == "tools/list") {
            json tools = json::array();
            for (const auto& t: registry.tools()) {
                tools.push_back(t.toListJson());
            }
            reply(rpc::resultResponse(id, {{"tools", std::move(tools)}}));
        } else if (method == "tools/call") {
            callTool(id, params, reply);
        } else if (method == "prompts/list") {
            json prompts = json::array();
            for (const auto& p: registry.prompts()) {
                prompts.push_back(p.toListJson());
            }
            reply(rpc::resultResponse(id, {{"prompts", std::move(prompts)}}));
        } else if (method == "prompts/get") {
            reply(rpc::resultResponse(id, getPrompt(params)));
        } else if (method == "resources/list" || method == "resources/templates/list") {
            const bool templates = method == "resources/templates/list";
            json list = json::array();
            for (const auto& r: registry.resources()) {
                if (r.isTemplate != templates) {
                    continue;
                }
                json entry = {{"name", r.name}, {"description", r.description}, {"mimeType", r.mimeType}};
                entry[templates ? "uriTemplate" : "uri"] = r.uri;
                list.push_back(std::move(entry));
            }
            reply(rpc::resultResponse(id, {{templates ? "resourceTemplates" : "resources", std::move(list)}}));
        } else if (method == "resources/read") {
            const std::string uri = params.value("uri", "");
            if (auto contents = readResource(uri)) {
                reply(rpc::resultResponse(id, {{"contents", std::move(*contents)}}));
            } else {
                reply(rpc::errorResponse(id, rpc::RESOURCE_NOT_FOUND, "Resource not found: " + uri));
            }
        } else if (method == "resources/subscribe" || method == "resources/unsubscribe") {
            const std::string uri = params.value("uri", "");
            if (method == "resources/subscribe") {
                session.subscriptions.insert(uri);
            } else {
                session.subscriptions.erase(uri);
            }
            reply(rpc::resultResponse(id, json::object()));
        } else if (method == "logging/setLevel") {
            reply(rpc::resultResponse(id, json::object()));
        } else {
            reply(rpc::errorResponse(id, rpc::METHOD_NOT_FOUND, "Method not found: " + method));
        }
    } catch (const ToolError& e) {
        reply(rpc::errorResponse(id, rpc::INVALID_PARAMS, e.what()));
    } catch (const std::exception& e) {
        reply(rpc::errorResponse(id, rpc::INTERNAL_ERROR, e.what()));
    }
    return true;
}

json McpProtocol::initialize(const json& params, Session& session) const {
    const auto& versions = supportedProtocolVersions();
    const std::string requested = params.value("protocolVersion", "");
    session.protocolVersion = std::find(versions.begin(), versions.end(), requested) != versions.end() ?
                                      requested :
                                      LATEST_PROTOCOL_VERSION;
    session.clientInfo = params.value("clientInfo", json::object());
    session.clientCapabilities = params.value("capabilities", json::object());

    json serverInfo = {{"name", info.name}, {"title", info.title}, {"version", info.version}};
    json result = {{"protocolVersion", session.protocolVersion},
                   {"capabilities",
                    {{"tools", {{"listChanged", false}}},
                     {"prompts", {{"listChanged", false}}},
                     {"resources", {{"subscribe", true}, {"listChanged", false}}},
                     {"logging", json::object()}}},
                   {"serverInfo", std::move(serverInfo)}};
    if (!info.instructions.empty()) {
        result["instructions"] = info.instructions;
    }
    return result;
}

void McpProtocol::callTool(const json& id, const json& params, Reply reply) {
    const std::string name = params.value("name", "");
    const ToolSpec* tool = registry.findTool(name);
    if (!tool) {
        reply(rpc::errorResponse(id, rpc::INVALID_PARAMS, "Unknown tool: " + name));
        return;
    }
    json args = params.value("arguments", json::object());
    if (args.is_null()) {
        args = json::object();
    }
    if (!args.is_object()) {
        reply(rpc::errorResponse(id, rpc::INVALID_PARAMS, "Tool arguments must be an object"));
        return;
    }
    if (permissionCheck) {
        if (auto denied = permissionCheck(*tool)) {
            reply(rpc::resultResponse(id, ToolResult::error(*denied).toJson()));
            return;
        }
    }
    // A modal dialog runs a nested main loop on the UI thread, and may hold the document lock meanwhile (Print
    // does, for as long as it is open): a tool run inside it that locks the document again waits for the UI
    // thread itself, forever (2026-10-05). No tools until it closes.
    if (g_main_depth() > 1) {
        reply(rpc::resultResponse(
                id, ToolResult::error("xournalai is showing a dialog (e.g. Print) right now; tools wait until it is "
                                      "closed. Try again in a moment, or ask the user to close it.")
                            .toJson()));
        return;
    }

    // Wrap the reply so it happens exactly once, and report timing to the observer
    const auto start = std::chrono::steady_clock::now();
    auto done = std::make_shared<bool>(false);
    auto observer = callObserver;
    Responder respond = [id, reply, done, name, start, observer](ToolResult result) {
        if (*done) {
            return;
        }
        *done = true;
        if (observer) {
            const double ms =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            observer(name, !result.isError(), ms);
        }
        if (const auto& text = result.prepared()) {
            reply(Response::text(R"({"jsonrpc":"2.0","id":)" + id.dump() + R"(,"result":)" + *text + "}"));
        } else {
            reply(rpc::resultResponse(id, result.toJson()));
        }
    };

    if (callStarted) {
        callStarted(name);
    }
    xoj::util::stall::Activity activity("tool " + name);
    // Error barrier: nothing a tool throws may escape into the GTK main loop
    try {
        if (tool->asyncHandler) {
            tool->asyncHandler(args, respond);
        } else {
            respond(tool->handler(args));
        }
    } catch (const ToolError& e) {
        respond(ToolResult::error(e.what()));
    } catch (const std::invalid_argument& e) {  // thrown by the typed services in src/core/api
        respond(ToolResult::error(e.what()));
    } catch (const json::exception& e) {
        respond(ToolResult::error(std::string("Invalid argument: ") + e.what()));
    } catch (const std::exception& e) {
        respond(ToolResult::error("Internal error in tool '" + name + "': " + e.what()));
    } catch (...) {
        respond(ToolResult::error("Internal error in tool '" + name + "'"));
    }
}

json McpProtocol::getPrompt(const json& params) const {
    const std::string name = params.value("name", "");
    const PromptSpec* prompt = registry.findPrompt(name);
    if (!prompt) {
        throw ToolError("Unknown prompt: " + name);
    }
    json args = params.value("arguments", json::object());
    for (const auto& a: prompt->arguments) {
        if (a.required && (!args.contains(a.name) || args[a.name].get<std::string>().empty())) {
            throw ToolError("Missing prompt argument: " + a.name);
        }
    }
    return {{"description", prompt->description},
            {"messages",
             json::array({{{"role", "user"}, {"content", {{"type", "text"}, {"text", prompt->render(args)}}}}})}};
}

std::optional<json> McpProtocol::readResource(const std::string& uri) const {
    for (const auto& r: registry.resources()) {
        if (!r.read) {
            continue;
        }
        if (!r.isTemplate && r.uri != uri) {
            continue;
        }
        if (auto contents = r.read(uri)) {
            return contents;
        }
    }
    return std::nullopt;
}

}  // namespace xoj::mcp
