/*
 * xournalai (based on Xournal++)
 *
 * Configuration of the embedded MCP server (~/.config/xournalpp/mcp.json)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <nlohmann/json.hpp>  // for json (the speech recognizer, as written)

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
        int wakeIdleMs = 2500;                ///< wake the session this long after the user stopped drawing
        int watchdogS = 20;                   ///< resend a wake-up the session didn't react to after this long
        int maxParallel = 8;                  ///< workers (subagents, worker tabs) / open edit transactions at once (1-8)
        bool speech = true;                   ///< Ask: listen while the pen's button is held (local speech to text)
        std::string speechModel = "base.en";  ///< whisper.cpp model (ggml-<name>.bin in the models folder)
        bool shareRecordings = true;          ///< tell the serving session when an audio recording ends
        /// GNOME's on-screen keyboard (tablet mode): "manual" (only when called: swipe up from the bottom edge) or
        /// "auto" (it opens whenever a text field gets the keyboard)
        std::string screenKeyboard = "manual";
        /// Live notes (experimental): a recording transcribed while it goes on (docs/mcp/TRANSCRIBERS.md)
        struct LiveNotes {
            bool enabled = false;         ///< a long press on the big Record button offers it
            /// The speech recognizer, as docs/mcp/TRANSCRIBERS.md describes it ({"kind": "file" | "stream",
            /// "command": …, more per kind}); kept as written (a save never drops what this app doesn't know)
            nlohmann::json transcriber = nlohmann::json::object();
            int pauseMs = 1500;  ///< a pause this long ends a piece
            int softS = 15;      ///< from this length a piece ends at a short pause
            int hardS = 25;      ///< and at this length anyway
            std::string language = "auto";
            /// The transcript column's font size, points (3.8: half the first 7.5, the owner's wish 2026-11-09)
            double transcriptSize = 3.8;
            /// The option «Просьбы по слову»: a request (a voice command) starts with one of these phrases (any case;
            /// punctuation between the words ignored). The owner's choice: «Hey Assistant» (2026-11-09)
            std::vector<std::string> triggerWords{"hey assistant", "эй ассистент", "хей ассистент", "hey assistent"};
        } liveNotes;
        /// The research browser (assistant/ResearchBrowser.h): kept as written
        nlohmann::json researchBrowser = nlohmann::json::object();
        /// A question about something on the page shows there too (a bubble), not only in the question panel (1.19.0)
        bool questionsOnCanvas = true;
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
