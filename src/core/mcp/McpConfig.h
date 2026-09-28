/*
 * Xournal++ (xournalai)
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

#include "Registry.h"  // for Tier
#include "filesystem.h"

namespace xoj::mcp {

struct McpConfig {
    bool enabled = true;
    uint16_t port = 7474;
    std::string token;                ///< bearer token clients must send; generated on first run
    std::set<Tier> tiers;             ///< granted permission tiers
    std::string defaultLayer = "AI";  ///< layer that receives agent drawings ("AI", "current" or a layer name)
    bool animate = true;              ///< animate agent drawing by default
    fs::path exportDir;               ///< where exports and renders are written when no path is given

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
