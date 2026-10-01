#include "ConnectSnippets.h"

#include <glib.h>

#include "Json.h"  // for json

namespace xoj::mcp {

namespace {
/// Quoted for a POSIX shell only when it has to be
std::string shellArg(const std::string& s) {
    for (char c: s) {
        if (!g_ascii_isalnum(c) && std::string("_-./:=@%+,").find(c) == std::string::npos) {
            gchar* q = g_shell_quote(s.c_str());
            std::string r(q);
            g_free(q);
            return r;
        }
    }
    return s;
}
}  // namespace

std::string runningExecutable() {
    if (gchar* self = g_file_read_link("/proc/self/exe", nullptr)) {
        std::string s(self);
        g_free(self);
        return s;
    }
    return "xournalpp";
}

std::vector<ConnectSnippet> connectSnippets(const std::string& url, const std::string& token, const std::string& exe) {
    const std::string bearer = "Bearer " + token;
    const std::string header = "Authorization: " + bearer;
    const std::string e = shellArg(exe);
    using ojson = nlohmann::ordered_json;  // (keys in the order people write them)
    const ojson stdioJson = {{"mcpServers", {{"xournalai", {{"command", exe}, {"args", {"--mcp-stdio"}}}}}}};
    const ojson httpJson = {
            {"mcpServers",
             {{"xournalai", {{"type", "http"}, {"url", url}, {"headers", {{"Authorization", bearer}}}}}}}};
    const ojson opencodeJson = {
            {"mcp", {{"xournalai", {{"type", "local"}, {"command", {exe, "--mcp-stdio"}}, {"enabled", true}}}}}};
    return {
            {"universal-stdio", "Any MCP client: JSON, stdio (recommended)",
             "Paste into the client's MCP configuration (its mcpServers section). The bridge reads the token itself "
             "and connects whenever xournalai is running.",
             stdioJson.dump(2)},
            {"universal-http", "Any MCP client: JSON, HTTP",
             "Paste into the client's MCP configuration (Cursor, Gemini CLI, VS Code, …). Works while xournalai "
             "is running; the token is in it.",
             httpJson.dump(2)},
            {"url", "URL and header",
             "For anything else: MCP Streamable HTTP at this URL, with this header on every request.",
             "URL:    " + url + "\nHeader: " + header},
            {"claude-stdio", "Claude Code (stdio, recommended)",
             "Run once in a terminal; xournalai is then available in all your Claude Code projects.",
             "claude mcp add -s user xournalai -- " + e + " --mcp-stdio"},
            {"claude-http", "Claude Code (HTTP)", "Run once in a terminal.",
             "claude mcp add -s user --transport http xournalai " + url + " --header \"" + header + "\""},
            {"codex", "Codex", "Run once in a terminal; it goes to ~/.codex/config.toml.",
             "codex mcp add xournalai -- " + e + " --mcp-stdio"},
            {"gemini", "Gemini CLI", "Run once in a terminal; it goes to ~/.gemini/settings.json.",
             "gemini mcp add -s user xournalai " + e + " --mcp-stdio"},
            {"opencode", "OpenCode", "Merge into opencode.json (the project's or ~/.config/opencode/opencode.json).",
             opencodeJson.dump(2)},
    };
}

}  // namespace xoj::mcp
