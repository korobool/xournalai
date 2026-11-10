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
    GtkWidget* recordWidget() const { return recordButton; }  ///< (null before placeButton)
    /// Live notes (experimental): whether a long press on Record offers its modes
    std::function<bool()> liveOffered;
    /// The long press's menu (since 1.17.0): the modes as big tiles, the options of the next recording as big
    /// toggles above them (remembered). A short tap on Record stays a plain recording.
    struct RecordChoice {
        std::string id, emoji, label, hint;
        std::string badges;  ///< the options that apply to it (their signs)
        bool last = false;   ///< the mode used last (highlighted)
        bool available = true;
    };
    struct RecordToggle {
        std::string id, label, off;  ///< `off`: its label when off ("" = the same, unticked)
        bool on = false;
        bool available = true;
    };
    std::function<std::vector<RecordChoice>()> recordChoices;
    std::function<std::vector<RecordToggle>()> recordToggles;
    std::function<void(const std::string& id, bool on)> onRecordToggle;
    std::function<void(const std::string& mode)> onLiveRecord;  ///< a mode's tile chosen
    std::function<void()> onCut;         ///< ✂ tapped
    std::function<void(bool down)> onTalk;  ///< 🎙 «Hold to talk» pressed / let go
    std::function<void()> onPinSelection;   ///< «📌 Insert selection» (the research browser's selection to the page)
    /// The research browser runs: «📌 Insert selection» shows
    void setResearchActive(bool on);
    /// A live-notes recording goes on: ✂ (send the piece being spoken now) shows next to Record; in voice control
    /// the big talk button instead (hold it while saying a command)
    void setLiveActive(bool on, bool voice = false);
    /// The big Full screen button next to Record: toggles the app's full-screen mode
    void setFullscreenVisible(bool visible);
    bool fullscreenWanted() const { return wantFullscreen; }
    /// Focus: the menu bar and the toolbars hidden, only the big buttons shown (remembered)
    void setFocused(bool on);
    bool isFocused() const { return focused; }
    /// The AI highlighter's mode bar: while it is on, the strip shows only the level, `info` and Discard / Done
    /// (as tall as the strip was, so the page doesn't move)
    void showModeBar(bool on, std::function<void()> done = {}, std::function<void()> discard = {});
    void setModeInfo(const std::string& info, const std::string& doneLabel = "Done");
    void pushModeLevel(float rms);
    /// The auto-finish countdown drawn on Done: the part still to go (1 … 0), or <0 for none
    void setModeCountdown(double left);
    bool modeBarShown() const { return modeOn; }
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
    GtkWidget* cutButton = nullptr;  ///< weak: ✂, next to Record during live notes
    GtkWidget* talkButton = nullptr;  ///< weak: 🎙 «Hold to talk», next to Record
    GtkWidget* pinButton = nullptr;   ///< weak: 📌 «Insert selection», while the research browser runs
    GtkGesture* talkPress = nullptr;  ///< (owned) its press and release, by finger, pen or mouse
    GtkGesture* recordLongPress = nullptr;  ///< (owned: a GTK 3 gesture is not kept by its widget)
    bool suppressRecordClick = false;  ///< the long press opened the menu: its release is not a click
    void openRecordMenu();
    void refreshRecordBadges();       ///< the mode tiles' badges follow the option toggles
    GtkWidget* recordGrid = nullptr;  ///< weak: the Record menu's tiles while it is open
    void toggleRecording();
    GtkWidget* recordFace = nullptr;   ///< weak
    GtkWidget* recordLabel = nullptr;  ///< (inside recordButton)
    guint recordPoll = 0;
    guint faceTick = 0;  ///< the face's animation timer while recording (FACE_MS)
    static constexpr guint FACE_MS = 125;  ///< (8 frames a second: lively enough, half the cost of 12)
    int shownRecordState = -1;     ///< (what the button shows: set again only when it changes)
    std::string shownRecordText;
    bool shownRecording = false;
    GtkWidget* fullscreenButton = nullptr;  ///< weak
    bool wantFullscreen = true;
    bool shownFullscreen = false;
    GAction* fullscreenAction() const;
    bool isFullscreen() const;
    void refreshFullscreen();
    void addColourTiles(GtkWidget* row);
    GtkWidget* outerBox = nullptr;       ///< weak: the strip
    GtkWidget* modeBar = nullptr;        ///< weak
    GtkWidget* modeInfo = nullptr;       ///< (in modeBar)
    GtkWidget* modeDone = nullptr;       ///< (in modeBar)
    GtkWidget* modeLevel = nullptr;      ///< (in modeBar)
    std::vector<float> modeLevels;
    double modeCountdown = -1;
    std::vector<std::pair<GtkWidget*, bool>> modeHidden;  ///< the strip's children and whether they were shown
    std::function<void()> modeOnDone, modeOnDiscard;
    bool modeOn = false;
    /// Focus: big icon buttons on the left of the strip (TABLET in priority order) and the pen's colours
    void buildTablet(GtkWidget* outer);
    void refreshTablet();
    /// In Focus: hides again what something else (full screen) showed
    void reapplyFocus();
    void openColours(GtkWidget* anchor);
    GtkWidget* tabletBox = nullptr;     ///< weak
    GtkWidget* colourButton = nullptr;  ///< weak
    std::vector<std::pair<std::string, GtkWidget*>> tabletTiles;  ///< weak; reserved: their addresses are watched
    int tabletFits = -1;
    std::map<std::string, int> tabletWidths;  ///< natural widths: of each switch, and ("") of the other tiles
    int touchShown = -1;  ///< what the touch switch shows (-1: not yet)
    int autoShown = -1;   ///< what the Auto-improve switch shows
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
