#include <gtest/gtest.h>

#include "mcp/ConnectSnippets.h"
#include "mcp/Json.h"

using xoj::mcp::connectSnippets;

namespace {
std::string text(const std::string& id, const std::string& exe = "/opt/xoai/bin/xournalpp") {
    for (const auto& s: connectSnippets("http://127.0.0.1:7474/mcp", "TOKEN", exe)) {
        if (s.id == id) {
            EXPECT_FALSE(s.name.empty());
            EXPECT_FALSE(s.hint.empty());
            return s.text;
        }
    }
    ADD_FAILURE() << "no snippet " << id;
    return "";
}
}  // namespace

// Commands as the clients accept them (checked against Claude Code, Codex and Gemini CLI)
TEST(ConnectSnippets, commandsPerClient) {
    EXPECT_EQ(text("claude-stdio"), "claude mcp add -s user xournalai -- /opt/xoai/bin/xournalpp --mcp-stdio");
    EXPECT_EQ(text("claude-http"), "claude mcp add -s user --transport http xournalai http://127.0.0.1:7474/mcp "
                                   "--header \"Authorization: Bearer TOKEN\"");
    EXPECT_EQ(text("codex"), "codex mcp add xournalai -- /opt/xoai/bin/xournalpp --mcp-stdio");
    EXPECT_EQ(text("gemini"), "gemini mcp add -s user xournalai /opt/xoai/bin/xournalpp --mcp-stdio");  // (no --)
}

TEST(ConnectSnippets, universalJsonIsValidAndComplete) {
    const auto stdio = xoj::mcp::json::parse(text("universal-stdio"));
    EXPECT_EQ(stdio["mcpServers"]["xournalai"]["command"], "/opt/xoai/bin/xournalpp");
    EXPECT_EQ(stdio["mcpServers"]["xournalai"]["args"][0], "--mcp-stdio");
    const auto http = xoj::mcp::json::parse(text("universal-http"));
    EXPECT_EQ(http["mcpServers"]["xournalai"]["url"], "http://127.0.0.1:7474/mcp");
    EXPECT_EQ(http["mcpServers"]["xournalai"]["headers"]["Authorization"], "Bearer TOKEN");
    EXPECT_NE(text("url").find("Authorization: Bearer TOKEN"), std::string::npos);
    const auto oc = xoj::mcp::json::parse(text("opencode"));
    EXPECT_EQ(oc["mcp"]["xournalai"]["command"][1], "--mcp-stdio");
}

TEST(ConnectSnippets, pathsWithSpacesAreQuotedForTheShell) {
    EXPECT_EQ(text("codex", "/home/me/My Apps/xournalpp"), "codex mcp add xournalai -- '/home/me/My Apps/xournalpp' "
                                                           "--mcp-stdio");
    // JSON carries the path as it is
    EXPECT_EQ(xoj::mcp::json::parse(
                      text("universal-stdio", "/home/me/My Apps/xournalpp"))["mcpServers"]["xournalai"]["command"],
              "/home/me/My Apps/xournalpp");
}
