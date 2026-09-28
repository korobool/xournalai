#include "Registry.h"

#include <stdexcept>  // for logic_error

#include "Schema.h"

namespace xoj::mcp {

const char* tierName(Tier t) {
    switch (t) {
        case Tier::Read:
            return "read";
        case Tier::Draw:
            return "draw";
        case Tier::Ui:
            return "ui";
        case Tier::Files:
            return "files";
    }
    return "read";
}

std::optional<Tier> tierFromName(const std::string& name) {
    for (Tier t: {Tier::Read, Tier::Draw, Tier::Ui, Tier::Files}) {
        if (name == tierName(t)) {
            return t;
        }
    }
    return std::nullopt;
}

ToolResult ToolResult::text(std::string text) {
    ToolResult r;
    r.addText(std::move(text));
    return r;
}

ToolResult ToolResult::structured(json data, std::string summary) {
    ToolResult r;
    std::string text = summary.empty() ? data.dump(1) : summary + "\n" + data.dump(1);
    r.addText(std::move(text));
    r.structured_ = std::move(data);
    return r;
}

ToolResult ToolResult::error(std::string message) {
    ToolResult r;
    r.addText(std::move(message));
    r.error_ = true;
    return r;
}

ToolResult& ToolResult::addText(std::string text) {
    content_.push_back({{"type", "text"}, {"text", std::move(text)}});
    return *this;
}

ToolResult& ToolResult::addImage(std::string base64Data, std::string mimeType) {
    content_.push_back({{"type", "image"}, {"data", std::move(base64Data)}, {"mimeType", std::move(mimeType)}});
    return *this;
}

ToolResult& ToolResult::addResourceLink(std::string uri, std::string name, std::string mimeType,
                                        std::string description) {
    json link = {{"type", "resource_link"},
                 {"uri", std::move(uri)},
                 {"name", std::move(name)},
                 {"mimeType", std::move(mimeType)}};
    if (!description.empty()) {
        link["description"] = std::move(description);
    }
    content_.push_back(std::move(link));
    return *this;
}

json ToolResult::toJson() const {
    json out = {{"content", content_}, {"isError", error_}};
    if (structured_ && structured_->is_object()) {
        out["structuredContent"] = *structured_;
    }
    return out;
}

json ToolSpec::toListJson() const {
    return {{"name", name},
            {"title", title},
            {"description", description},
            {"inputSchema", inputSchema},
            {"annotations",
             {{"title", title},
              {"readOnlyHint", readOnly},
              {"destructiveHint", destructive},
              {"idempotentHint", idempotent},
              {"openWorldHint", false}}}};
}

json PromptSpec::toListJson() const {
    json args = json::array();
    for (const auto& a: arguments) {
        args.push_back({{"name", a.name}, {"description", a.description}, {"required", a.required}});
    }
    return {{"name", name}, {"title", title}, {"description", description}, {"arguments", std::move(args)}};
}

void Registry::addTool(ToolSpec spec) {
    if (findTool(spec.name)) {
        throw std::logic_error("duplicate MCP tool " + spec.name);
    }
    if (auto problems = schema::lint(spec.name, spec.inputSchema); !problems.empty()) {
        throw std::logic_error("non-portable schema: " + problems.front());
    }
    if (!spec.handler && !spec.asyncHandler) {
        throw std::logic_error("MCP tool without handler: " + spec.name);
    }
    tools_.push_back(std::move(spec));
}

void Registry::addPrompt(PromptSpec spec) {
    if (findPrompt(spec.name)) {
        throw std::logic_error("duplicate MCP prompt " + spec.name);
    }
    prompts_.push_back(std::move(spec));
}

void Registry::addResource(ResourceSpec spec) { resources_.push_back(std::move(spec)); }

const ToolSpec* Registry::findTool(const std::string& name) const {
    for (const auto& t: tools_) {
        if (t.name == name) {
            return &t;
        }
    }
    return nullptr;
}

const PromptSpec* Registry::findPrompt(const std::string& name) const {
    for (const auto& p: prompts_) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

}  // namespace xoj::mcp
