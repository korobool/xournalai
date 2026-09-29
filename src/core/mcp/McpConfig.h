/*
 * xournalai (based on Xournal++)
 *
 * Configuration of the embedded MCP server (~/.config/xournalpp/mcp.json)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>   // for uint16_t
#include <optional>  // for optional
#include <set>       // for set
#include <string>    // for string
#include <vector>    // for vector

#include "Registry.h"  // for Tier
#include "filesystem.h"

namespace xoj::mcp {

struct McpConfig {
    bool enabled = true;
    uint16_t port = 7474;
    std::string token;                     ///< bearer token clients must send; generated on first run
    std::set<Tier> tiers;                  ///< granted permission tiers
    std::string defaultLayer = "current";  ///< layer that receives agent drawings ("current", "AI" or a layer name)
    bool animate = true;                   ///< animate agent drawing by default
    bool agentsChooseLayer = false;        ///< false: AI drawings always go to defaultLayer, whatever an agent asks
    fs::path exportDir;                    ///< where exports and renders are written when no path is given
    bool backups = true;                   ///< save a copy of the document before risky agent operations
    /// The serving session in the AI terminal
    struct Assistant {
        bool autostart = true;                  ///< start it with the app
        std::string agent = "claude";           ///< "claude" or "codex"
        std::string permissionMode = "bypass";  ///< "bypass" (skip all permission prompts) or "normal"
        std::string command;                    ///< overrides the command line (advanced; tests use a fake agent)
        bool autoImprove = false;               ///< improve everything the user writes (the toolbar toggle)
        /// What Auto-improve does: any of "formulas", "text", "diagrams", "colours"
        std::vector<std::string> rules{"formulas", "text", "diagrams", "colours"};
        int wakeIdleMs = 2500;  ///< wake the session this long after the user stopped drawing
        int watchdogS = 20;     ///< resend a wake-up the session didn't react to after this long
        int maxParallel = 5;    ///< subagents / open edit transactions at once (1-5)
    } assistant;
    fs::path backupDir;  ///< where those copies go

    /// Defaults: everything granted except Tier::Destructive
    McpConfig();

    bool allows(Tier t) const { return tiers.count(t) > 0; }
    std::string url() const;

    /// Loads the file, creating it (with a fresh token) if missing or incomplete. Applies command line overrides.
    static McpConfig load();
    static fs::path path();
    void save() const;

    /// Overrides from the command line (--mcp, --no-mcp, --mcp-port)
    struct Overrides {
        std::optional<bool> enabled;
        std::optional<uint16_t> port;
    };
    static Overrides& overrides();

    static std::string generateToken();
};

}  // namespace xoj::mcp
