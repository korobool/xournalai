/*
 * xournalai (based on Xournal++)
 *
 * Quick tools: one big button (first in the AI toolbar, or F8) that opens a grid of large buttons with the tools the
 * owner uses most, for a thumb, a finger or the pen. How often each tool is used (from anywhere: toolbar, menu,
 * shortcut or this grid) is remembered in quick-tools.json and decides which ten are shown, and in which order.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <filesystem>  // for path
#include <functional>  // for function
#include <map>         // for map
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

class Control;

namespace xoj::assistant {

namespace quick_tools {
constexpr size_t SHOWN = 10;

struct Entry {
    const char* id;
    const char* label;
    const char* icon;
    const char* tip;
};
/// Every tool the grid can offer, in the order used before anything has been counted
const std::vector<Entry>& catalog();
/// The ids of the `n` tools to show: most used first; ties (and unknown ids) keep the catalog order
std::vector<std::string> ranked(const std::map<std::string, int>& counts, size_t n = SHOWN);
std::map<std::string, int> load(const std::filesystem::path& file);
bool save(const std::filesystem::path& file, const std::map<std::string, int>& counts);
}  // namespace quick_tools

class QuickTools final {
public:
    /// run: performs an id that is not a document tool (ask, command, terminal); the rest go to the app's actions
    QuickTools(Control* control, std::filesystem::path countsFile, std::function<void(const std::string&)> run);
    ~QuickTools();
    QuickTools(const QuickTools&) = delete;
    QuickTools& operator=(const QuickTools&) = delete;

    /// Opens the grid next to `anchor` (null: in the middle of the main window)
    void open(GtkWidget* anchor);
    /// Performs a tool as a tap on its button would (and counts it)
    void activate(const std::string& id);
    /// Counts one use of a tool
    void count(const std::string& id);
    const std::map<std::string, int>& counts() const { return uses; }

private:
    void connectCounting();
    void closePopover();

    Control* control;
    std::filesystem::path file;
    std::function<void(const std::string&)> run;
    std::map<std::string, int> uses;
    std::vector<std::pair<gpointer, gulong>> handlers;  ///< counting signal handlers on the app's actions
    GtkWidget* popover = nullptr;                       ///< weak
    bool performing = false;                            ///< a tap is being performed: its action isn't counted twice
};

}  // namespace xoj::assistant
