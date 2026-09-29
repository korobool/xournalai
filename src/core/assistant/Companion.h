/*
 * xournalai (based on Xournal++)
 *
 * The companion folder: where xournalai's serving Claude Code (or Codex) session runs. The app keeps its
 * instructions (CLAUDE.md / AGENTS.md), its MCP configuration and its Claude Code settings up to date; your own
 * additions outside the marked blocks are kept.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for uint16_t
#include <string>   // for string

#include "filesystem.h"  // for path

namespace xoj::assistant {

struct CompanionSetup {
    uint16_t port = 7474;    ///< this app's MCP port
    std::string executable;  ///< this app's binary (for the stdio bridge and the hooks)
};

class Companion final {
public:
    /// ~/.local/share/xournalai/companion (XDG data dir)
    static fs::path folder();

    /// Creates or refreshes the folder's files; returns the folder (never throws, logs problems)
    static fs::path ensure(const CompanionSetup& setup);

    /// The instructions for the serving session (the part the app owns)
    static std::string instructions(const CompanionSetup& setup);

    /// Replaces the block between the xournalai markers in `existing`, or appends it; keeps everything else
    static std::string mergeManagedBlock(const std::string& existing, const std::string& block);

    static constexpr const char* BEGIN_MARK =
            "<!-- xournalai:begin (managed by xournalai; edits here are replaced) -->";
    static constexpr const char* END_MARK = "<!-- xournalai:end -->";
};

}  // namespace xoj::assistant
