/*
 * xournalai (based on Xournal++)
 *
 * A collapsible terminal dock at the bottom of the main window. Runs the serving Claude Code session (and, on
 * demand, Codex, OpenCode or a shell) in VTE terminals. Hiding the dock keeps the processes running.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <filesystem>  // for path
#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

#include "assistant/TerminalCommands.h"  // for SavedCommand

namespace xoj::assistant {

/// What a terminal tab runs
struct TerminalSpec {
    std::string title;             ///< tab label, e.g. "Claude"
    std::string command;           ///< shell command line, run through the user's login shell (empty: a shell)
    std::string workingDirectory;  ///< empty: the home directory
    std::vector<std::string> env;  ///< extra "KEY=value" entries
};

class TerminalDock final {
public:
    /// Builds the dock into the main window: the window's content area and the dock share a vertical split
    TerminalDock(GtkWidget* mainBox, GtkWidget* contentArea);
    ~TerminalDock();
    TerminalDock(const TerminalDock&) = delete;
    TerminalDock& operator=(const TerminalDock&) = delete;

    /// Opens a new tab running `spec`; returns its index
    int openTab(const TerminalSpec& spec, bool focus = true);
    /// Restarts the process of a tab (same spec)
    void restartTab(int index);
    /// Closes a tab and ends its program; the other tabs keep their indices
    void closeTab(int index);
    /// The index of the tab shown, or -1
    int currentTab() const;
    /// Number of tabs ever opened (closed ones included: indices stay valid)
    int tabCount() const;
    /// Number of tabs still open
    int openTabCount() const;
    /// Index of the first tab with this title, or -1
    int findTab(const std::string& title) const;
    /// Whether the tab's process is running
    bool isRunning(int index) const;

    /// Types text into a tab's terminal as if the user had typed it (no newline added)
    bool feed(int index, const std::string& text);
    /// The visible text of a tab's terminal (for tests and diagnostics)
    std::string text(int index) const;

    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    /// When the user last pressed a key in any terminal tab (monotonic µs; 0 if never)
    gint64 lastInputUs() const { return lastInput; }
    /// Called with the tab index when a tab's process exits
    void setExitHandler(std::function<void(int index, int status)> handler) { onExit = std::move(handler); }
    /// Entries of the "+" menu: title → spec
    void setNewTabChoices(std::vector<TerminalSpec> choices);
    /// Where the owner's own commands are remembered, and the environment their tabs get
    void setCustomCommands(std::filesystem::path file, std::vector<std::string> env);
    /// Opens the "Custom command…" popover: a command line (pasted or typed), a tab name and a folder
    void promptCustomCommand();
    /// Opens a tab running the owner's command and remembers it
    int openCustom(const SavedCommand& cmd);

private:
    struct Tab;
    Tab* tabAt(int index) const;
    void spawn(Tab* tab);
    void buildHeader();
    void rebuildMenu();
    void fillSavedList(GtkWidget* list);

    GtkWidget* paned = nullptr;
    GtkWidget* dock = nullptr;
    GtkWidget* notebook = nullptr;
    GtkWidget* newTabMenu = nullptr;
    GtkWidget* plusButton = nullptr;
    std::filesystem::path commandsFile;
    std::vector<std::string> customEnv;
    std::vector<Tab*> tabs;
    std::vector<TerminalSpec> choices;
    std::function<void(int, int)> onExit;
    int lastHeight = 260;
    gint64 lastInput = 0;
};

}  // namespace xoj::assistant
