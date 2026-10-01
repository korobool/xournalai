/*
 * xournalai (based on Xournal++)
 *
 * Ready-made ways to connect an agent to this app's MCP server, with the token and paths filled in: universal ones
 * (an mcpServers JSON block, stdio or HTTP; the bare URL and header) and one command per known client (Claude Code,
 * Codex, Gemini CLI, OpenCode). Shown by "Connect an agent…" and written to mcp.json's _help.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

namespace xoj::mcp {

struct ConnectSnippet {
    std::string id;    ///< e.g. "claude-stdio"
    std::string name;  ///< e.g. "Claude Code (stdio, recommended)"
    std::string hint;  ///< where it goes / what to do with it
    std::string text;  ///< what is copied
};

/// `exe`: the xournalpp binary (for the stdio bridge)
std::vector<ConnectSnippet> connectSnippets(const std::string& url, const std::string& token, const std::string& exe);

/// This running binary (/proc/self/exe), else "xournalpp"
std::string runningExecutable();

}  // namespace xoj::mcp
