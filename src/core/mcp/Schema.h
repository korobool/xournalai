/*
 * Xournal++ (xournalai)
 *
 * Builders for portable tool input schemas, plus a lint that keeps them portable across MCP clients.
 *
 * Several clients and model providers only accept a small subset of JSON Schema (for example, Gemini rejects
 * $ref/oneOf/anyOf/additionalProperties). Every tool schema is therefore a flat object whose properties each
 * have a single "type" and a "description".
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Json.h"

namespace xoj::mcp::schema {

using Property = std::pair<std::string, json>;

json object(std::vector<Property> properties, std::vector<std::string> required = {}, std::string description = {});
json string(std::string description);
json number(std::string description);
json integer(std::string description);
json boolean(std::string description);
json enumeration(std::string description, std::vector<std::string> values);
json array(std::string description, json items);
/// Returns a copy of `s` with a "default" value documented
json withDefault(json s, json defaultValue);
/// Returns a copy of `s` with numeric bounds
json range(json s, double minimum, double maximum);

/// Checks a tool name and its input schema against the portability rules. Returns human-readable problems.
std::vector<std::string> lint(const std::string& toolName, const json& inputSchema);

/// Tool name rule: ^[a-z][a-z0-9_]{0,39}$
bool isValidToolName(const std::string& name);

}  // namespace xoj::mcp::schema
