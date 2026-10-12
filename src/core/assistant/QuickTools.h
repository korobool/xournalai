/*
 * xournalai (based on Xournal++)
 *
 * Tools: one big, colourful button to the right of the toolbars (as tall as all of them; no row of its own), or F8,
 * opens large buttons for the pen and fingers, in fixed groups: Select (lasso, rectangle, pointer), Draw, Page, AI.
 * For tablet mode, without a keyboard. How often each tool is used is still counted (quick-tools.json).
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

    /// Opens the grid next to `anchor` (null: the Tools button if shown, else the middle of the window)
    void open(GtkWidget* anchor = nullptr);
    /// Puts `rows` (toolbars in `mainBox`) into a column with the Tools button to their right
    void placeButton(GtkWidget* mainBox, const std::vector<GtkWidget*>& rows);
    void setButtonVisible(bool visible);
    bool isButtonVisible() const;
    /// Whether the owner last left the Tools button shown (remembered in the counts file; shown at first)
    bool buttonWanted() const { return wantButton; }
    /// The big Record button next to Tools (experimental): starts and stops audio recording
    void setRecordVisible(bool visible);
    bool recordWanted() const { return wantRecord; }
    /// The big Full screen button next to Record: toggles the app's full-screen mode
    void setFullscreenVisible(bool visible);
    bool fullscreenWanted() const { return wantFullscreen; }
    /// Focus: the menu bar and the toolbars hidden, only the big buttons shown (remembered)
    void setFocused(bool on);
    bool isFocused() const { return focused; }
    /// Called by the Focus button with the state wanted (the owner routes it through its action)
    std::function<void(bool)> onFocus;
    void setFocusVisible(bool visible);
    bool focusButtonWanted() const { return wantFocusButton; }
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
    GtkWidget* button = nullptr;                        ///< weak: the Tools button
    bool wantButton = true;
    GtkWidget* recordButton = nullptr;  ///< weak
    bool wantRecord = true;
    void refreshRecord();
    bool isRecording() const;
    void drawRecordFace(GtkWidget* w, cairo_t* cr);
    GtkWidget* recordFace = nullptr;   ///< weak
    GtkWidget* recordLabel = nullptr;  ///< (inside recordButton)
    guint recordPoll = 0;
    guint faceTick = 0;
    gint64 labelUs = 0;
    bool shownRecording = false;
    GtkWidget* fullscreenButton = nullptr;  ///< weak
    bool wantFullscreen = true;
    bool shownFullscreen = false;
    GAction* fullscreenAction() const;
    bool isFullscreen() const;
    void refreshFullscreen();
    void addColourTiles(GtkWidget* row);
    GtkWidget* focusButton = nullptr;    ///< weak
    GtkWidget* toolbarColumn = nullptr;  ///< weak: the toolbars' column
    GtkWidget* mainWindow = nullptr;
    bool wantFocusButton = true;
    bool focused = false;
    bool menubarBefore = true;  ///< the menu bar's state before Focus hid it
    bool sidebarBefore = false;  ///< the sidebar's, likewise
    std::vector<GtkWidget*> hiddenBars;  ///< the toolbars Focus hid
    double smoothLevel = 0;
    bool performing = false;  ///< a tap is being performed: its action isn't counted twice
};

}  // namespace xoj::assistant
