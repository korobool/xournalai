#include "Schema.h"

#include <algorithm>  // for find
#include <array>      // for array

namespace xoj::mcp::schema {

json object(std::vector<Property> properties, std::vector<std::string> required, std::string description) {
    json props = json::object();
    for (auto& [name, s]: properties) {
        props[name] = std::move(s);
    }
    json out = {{"type", "object"}, {"properties", std::move(props)}};
    if (!required.empty()) {
        out["required"] = std::move(required);
    }
    if (!description.empty()) {
        out["description"] = std::move(description);
    }
    return out;
}

static json typed(const char* type, std::string description) {
    return {{"type", type}, {"description", std::move(description)}};
}

json string(std::string description) { return typed("string", std::move(description)); }
json number(std::string description) { return typed("number", std::move(description)); }
json integer(std::string description) { return typed("integer", std::move(description)); }
json boolean(std::string description) { return typed("boolean", std::move(description)); }

json enumeration(std::string description, std::vector<std::string> values) {
    json out = typed("string", std::move(description));
    out["enum"] = std::move(values);
    return out;
}

json array(std::string description, json items) {
    json out = typed("array", std::move(description));
    out["items"] = std::move(items);
    return out;
}

json withDefault(json s, json defaultValue) {
    s["default"] = std::move(defaultValue);
    return s;
}

json range(json s, double minimum, double maximum) {
    s["minimum"] = minimum;
    s["maximum"] = maximum;
    return s;
}

bool isValidToolName(const std::string& name) {
    if (name.empty() || name.size() > 40 || name[0] < 'a' || name[0] > 'z') {
        return false;
    }
    return std::all_of(name.begin(), name.end(),
                       [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

namespace {
constexpr std::array FORBIDDEN_KEYS = {"$ref",
                                       "oneOf",
                                       "anyOf",
                                       "allOf",
                                       "not",
                                       "$schema",
                                       "$defs",
                                       "definitions",
                                       "if",
                                       "then",
                                       "else",
                                       "const",
                                       "additionalProperties",
                                       "patternProperties"};
constexpr std::array ALLOWED_TYPES = {"string", "number", "integer", "boolean", "array", "object"};

void lintNode(const std::string& path, const json& node, bool needsDescription, std::vector<std::string>& problems) {
    if (!node.is_object()) {
        problems.push_back(path + ": schema must be an object");
        return;
    }
    for (const char* key: FORBIDDEN_KEYS) {
        if (node.contains(key)) {
            problems.push_back(path + ": uses non-portable keyword '" + key + "'");
        }
    }
    auto type = node.find("type");
    if (type == node.end() || !type->is_string()) {
        problems.push_back(path + ": needs a single string 'type'");
        return;
    }
    const auto t = type->get<std::string>();
    if (std::find(ALLOWED_TYPES.begin(), ALLOWED_TYPES.end(), t) == ALLOWED_TYPES.end()) {
        problems.push_back(path + ": unknown type '" + t + "'");
    }
    if (needsDescription) {
        auto d = node.find("description");
        if (d == node.end() || !d->is_string() || d->get<std::string>().empty()) {
            problems.push_back(path + ": needs a description");
        }
    }
    if (t == "array") {
        if (!node.contains("items")) {
            problems.push_back(path + ": array needs 'items'");
        } else {
            lintNode(path + "[]", node["items"], false, problems);
        }
    }
    if (t == "object") {
        auto props = node.find("properties");
        if (props == node.end() || !props->is_object()) {
            problems.push_back(path + ": object needs 'properties'");
            return;
        }
        for (const auto& [name, child]: props->items()) {
            lintNode(path + "." + name, child, true, problems);
        }
        if (auto req = node.find("required"); req != node.end()) {
            for (const auto& r: *req) {
                if (!r.is_string() || !props->contains(r.get<std::string>())) {
                    problems.push_back(path + ": required entry " + r.dump() + " is not a property");
                }
            }
        }
    }
}
}  // namespace

std::vector<std::string> lint(const std::string& toolName, const json& inputSchema) {
    std::vector<std::string> problems;
    if (!isValidToolName(toolName)) {
        problems.push_back(toolName + ": tool name must match ^[a-z][a-z0-9_]{0,39}$");
    }
    if (!inputSchema.is_object() || inputSchema.value("type", "") != "object") {
        problems.push_back(toolName + ": input schema must be of type object");
        return problems;
    }
    lintNode(toolName, inputSchema, false, problems);
    return problems;
}

}  // namespace xoj::mcp::schema
