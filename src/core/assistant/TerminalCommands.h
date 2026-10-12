/*
 * xournalai (based on Xournal++)
 *
 * The owner's own terminal commands (e.g. several OpenCode configurations): typed or pasted once in the AI
 * terminal's "Custom command…", remembered in ai-terminals.json and offered again in its + menu.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <filesystem>  // for path
#include <string>      // for string
#include <vector>      // for vector

namespace xoj::assistant {

struct SavedCommand {
    std::string title;    ///< tab label (empty: derived from the command)
    std::string command;  ///< shell command line (empty: a shell)
    std::string folder;   ///< working directory (empty: home; "~/" is expanded)

    bool operator==(const SavedCommand& o) const { return command == o.command && folder == o.folder; }
};

namespace terminal_commands {
constexpr size_t MAX_SAVED = 12;

/// The list stored in `file` (empty if missing or unreadable)
std::vector<SavedCommand> load(const std::filesystem::path& file);
/// Writes the list; false on failure
bool save(const std::filesystem::path& file, const std::vector<SavedCommand>& list);
/// `cmd` moved (or added) to the front; the same command in the same folder is kept once; at most MAX_SAVED
std::vector<SavedCommand> remember(std::vector<SavedCommand> list, const SavedCommand& cmd);
/// The list without `cmd`
std::vector<SavedCommand> forget(std::vector<SavedCommand> list, const SavedCommand& cmd);
/// The tab label: the title, else the program's name (e.g. "opencode" for "opencode --config ~/w.json"), else "Shell"
std::string tabTitle(const SavedCommand& cmd);
/// The folder with a leading "~" expanded (empty stays empty)
std::string expandFolder(const std::string& folder);
}  // namespace terminal_commands

}  // namespace xoj::assistant
