/*
 * xournalai (based on Xournal++)
 *
 * Tool, prompt and resource definitions of the MCP server
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <optional>    // for optional
#include <stdexcept>   // for runtime_error
#include <string>      // for string
#include <vector>      // for vector

#include "Json.h"

namespace xoj::mcp {

/**
 * Permission tier a tool (or a particular use of a tool) belongs to. Users grant tiers in the MCP configuration.
 *  - Read: inspect and render content          - Draw: create and edit content (always undoable)
 *  - Ui: menus, dialogs, tools, view            - Files: open, save, export, import
 *  - Destructive: discard unsaved changes, overwrite existing files, close without saving
 */
enum class Tier { Read, Draw, Ui, Files, Destructive };
const char* tierName(Tier t);
std::optional<Tier> tierFromName(const std::string& name);

/**
 * @brief An error meant for the agent (bad arguments, missing document, ...).
 * Thrown from tool handlers; reported as a tool result with isError=true, so the model can correct itself.
 */
class ToolError: public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Result of a tool call: MCP content blocks plus optional structured content
class ToolResult {
public:
    static ToolResult text(std::string text);
    /// Structured result. It is sent as structuredContent and, for clients that ignore that, as JSON text.
    static ToolResult structured(json data, std::string summary = {});
    static ToolResult error(std::string message);

    ToolResult& addText(std::string text);
    ToolResult& addImage(std::string base64Data, std::string mimeType = "image/png");
    ToolResult& addResourceLink(std::string uri, std::string name, std::string mimeType, std::string description = {});

    bool isError() const { return error_; }
    const json& content() const { return content_; }
    const std::optional<json>& structuredContent() const { return structured_; }
    json toJson() const;

private:
    json content_ = json::array();
    std::optional<json> structured_;
    bool error_ = false;
};

using Responder = std::function<void(ToolResult)>;

struct ToolSpec {
    std::string name;
    std::string title;
    std::string description;
    json inputSchema;
    Tier tier = Tier::Read;
    bool readOnly = false;
    bool destructive = false;
    bool idempotent = false;

    /// Synchronous handler (most tools)
    std::function<ToolResult(const json& args)> handler;
    /// Asynchronous handler; must call the responder exactly once (possibly later, e.g. wait_for_user)
    std::function<void(const json& args, Responder respond)> asyncHandler;

    json toListJson() const;
};

struct PromptArgument {
    std::string name;
    std::string description;
    bool required = false;
};

struct PromptSpec {
    std::string name;
    std::string title;
    std::string description;
    std::vector<PromptArgument> arguments;
    /// Returns the text of the user message for the given arguments
    std::function<std::string(const json& args)> render;

    json toListJson() const;
};

struct ResourceSpec {
    std::string uri;  ///< Fixed URI, or a URI template such as xournal://page/{page}
    std::string name;
    std::string description;
    std::string mimeType;
    bool isTemplate = false;
    /// Returns the MCP "contents" array for the given URI, or std::nullopt if the URI does not match
    std::function<std::optional<json>(const std::string& uri)> read;
};

class Registry {
public:
    /// Adds a tool; throws std::logic_error if the name is taken or the schema is not portable
    void addTool(ToolSpec spec);
    void addPrompt(PromptSpec spec);
    void addResource(ResourceSpec spec);

    const ToolSpec* findTool(const std::string& name) const;
    const PromptSpec* findPrompt(const std::string& name) const;
    const std::vector<ToolSpec>& tools() const { return tools_; }
    const std::vector<PromptSpec>& prompts() const { return prompts_; }
    const std::vector<ResourceSpec>& resources() const { return resources_; }

private:
    std::vector<ToolSpec> tools_;
    std::vector<PromptSpec> prompts_;
    std::vector<ResourceSpec> resources_;
};

}  // namespace xoj::mcp
