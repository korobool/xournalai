/*
 * Xournal++ (xournalai)
 *
 * UTF-8 <-> filesystem path helpers for the MCP module (JSON strings are UTF-8)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>

#include "filesystem.h"

namespace xoj::mcp {

inline std::string toUtf8(const fs::path& p) {
    const auto s = p.u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}

inline fs::path pathFromUtf8(const std::string& s) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

}  // namespace xoj::mcp
