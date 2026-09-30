/*
 * xournalai (based on Xournal++)
 *
 * In-app UI of the MCP server: status strip, "AI Agent" menu actions (pause, AI layer), highlights
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>    // for unique_ptr
#include <optional>  // for optional
#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "model/PageRef.h"  // for PageRef
#include "util/Point.h"     // for Point
#include "util/Rectangle.h"

#include "Json.h"             // for json
#include "config-features.h"  // for ENABLE_AI_TERMINAL

#ifdef ENABLE_AI_TERMINAL
#include <memory>  // for unique_ptr

namespace xoj::assistant {
class TerminalDock;
class EventPump;
struct TerminalSpec;
}  // namespace xoj::assistant
#endif

namespace xoj::mcp {

class McpServer;

}  // namespace xoj::mcp

namespace xoj::assistant {
class AiToolbar;
class ThinkingOverlay;
class SpeechToText;
class AskController;
struct AskCapture;
class AskPopover;
}  // namespace xoj::assistant
namespace xoj::api {
struct DocEvent;
}

namespace xoj::mcp {

class McpUi final {
public:
    explicit McpUi(McpServer& server);
    ~McpUi();
    McpUi(const McpUi&) = delete;
    McpUi& operator=(const McpUi&) = delete;

    /// A tool started/finished (shown in the status strip)
    void toolStarted(const std::string& name);
    void toolFinished();

    /// Briefly marks an area where the agent added content
    void flash(size_t page, const xoj::util::Rectangle<double>& area);

    /// Refreshes the status text
    void update();

    /// Opens the MCP settings dialog (user only; agents cannot operate it)
    void showSettings();

    /// An AI toolbar action: improve | illustrate | web | image | command | revise (text: the typed command).
    /// Becomes an intent for the selection (or the last piece drawn, or the page); returns its description.
    std::string aiAction(const std::string& kind, const std::string& text);

    /// Visible thinking on the canvas (null if the window has no overlay)
    assistant::ThinkingOverlay* thinking() const { return thinkingOverlay.get(); }
    /// Stops the AI's current work (one zone, or all with zone = 0): interrupts the serving session (Esc)
    void stopWork(int zone);
    /// The whole page's area in page points (for page-level zones)
    xoj::util::Rectangle<double> pageArea(size_t page) const;

#ifdef ENABLE_AI_TERMINAL
    /// The AI terminal dock (null if the window has no place for it)
    assistant::TerminalDock* terminal() const { return dock.get(); }
    /// Opens a terminal tab: "claude", "codex", "opencode" or "shell"
    void openTerminal(const std::string& kind);
    /// Starts the serving session (Claude Code or Codex in the companion folder) unless it runs already
    void startServing();
    /// Delivers canvas events to the serving session (null without the terminal)
    assistant::EventPump* pump() const { return eventPump.get(); }
#endif
    /// Local speech to text for Ask (null if disabled or not built)
    assistant::SpeechToText* speech() const { return speechToText.get(); }
    /// Ask (pen button held: listen; the lasso tells where): its state and the last request, for app_status
    json askStatus() const;

private:
    static gboolean onTick(gpointer self);
    void installActions();
    void buildStrip();
    void buildMenu();
    void removeMenu();
    void clearLayer(bool merge);
    void applySettings(GtkWidget* dialog);

    /// Tracks a GObject without owning it: `ptr` becomes null when the object is destroyed (the main window is
    /// destroyed before Control, and so before this class, when the application quits)
    template <typename T>
    void watch(T*& ptr) {
        if (ptr) {
            g_object_add_weak_pointer(G_OBJECT(ptr), reinterpret_cast<gpointer*>(&ptr));
        }
    }
    template <typename T>
    void unwatch(T*& ptr) {
        if (ptr) {
            g_object_remove_weak_pointer(G_OBJECT(ptr), reinterpret_cast<gpointer*>(&ptr));
        }
    }

    McpServer& server;
    GtkWidget* window = nullptr;    ///< weak
    GMenuModel* menubar = nullptr;  ///< weak
    GtkWidget* strip = nullptr;
    GtkWidget* label = nullptr;
    GtkWidget* pauseButton = nullptr;
    std::string currentTool;
    int running = 0;
    guint timer = 0;
    guint menuIdle = 0;
    std::string lastText;
    GtkWidget* settings = nullptr;  ///< weak (strip, label, pauseButton too)
    void buildToolbar();
    std::unique_ptr<assistant::AiToolbar> aiToolbar;
    void buildThinking();
    void onServingChanged();
    std::unique_ptr<assistant::ThinkingOverlay> thinkingOverlay;
    int lastServingState = 0;

    // Handwritten markers (*! **! *w! *c! *r!) among the user's fresh strokes
    void noteUserStroke(const api::DocEvent& e);
    void scanMarkers();
    struct RecentStroke {
        size_t page;
        std::string id;
        gint64 timeUs;
    };
    std::vector<RecentStroke> recentStrokes;
    std::vector<std::string> markerIdsDone;
    guint markerTimer = 0;
    std::unique_ptr<assistant::SpeechToText> speechToText;
    guint speechWarmUp = 0;
    std::unique_ptr<assistant::AskController> ask;  ///< after speechToText: destroyed before it
    std::string askStatusText;
    json lastAsk;
    std::unique_ptr<assistant::AskPopover> askPopover;
    std::unique_ptr<assistant::AskCapture> pendingAsk;  ///< what the open popover is about
    bool micHeld = false;
    bool lassoArmed = false;                            ///< Ask: the next pen / mouse stroke is the lasso
    std::vector<xoj::util::Point<double>> lassoPoints;  ///< being drawn
    PageRef lassoPage;
    std::string pendingDictation;
    /// Where the recording indicator is shown: the pen when its button was pressed (page, page coordinates)
    std::optional<std::pair<size_t, xoj::util::Point<double>>>
            recordAnchor;  ///< said with the pen button while the lasso was drawn (for the popover)
    /// Arms (or disarms) the Ask lasso
    void armLasso(bool on);
    /// Pen button held while the lasso is armed or the popover is open: dictation into it
    void dictate(bool pressed);
    void onAsk(const assistant::AskCapture& c);
    /// A Xournal++ recording ended: tell the serving session (file, length, page, strokes written meanwhile)
    void onRecordingFinished(const std::string& file, const std::string& name, int64_t durationMs);

public:
    /// (tests) as if a recording ended
    void recordingFinished(const std::string& file, const std::string& name, int64_t durationMs) {
        onRecordingFinished(file, name, durationMs);
    }

private:
    /// Opens the Ask popover for `c` with `text`
    void showAsk(const assistant::AskCapture& c, const std::string& text);
    /// Ask about the selection (or the visible part of the page): the toolbar's Command…
    void openAskForSelection();
    /// The popover's Send / command icons: the request goes to the serving session as a zone
    void submitAsk(const std::string& command, const std::string& text);
    /// `area` of `page` in canvas widget coordinates (null if not shown)
    std::optional<GdkRectangle> canvasRect(size_t page, const xoj::util::Rectangle<double>& area) const;
#ifdef ENABLE_AI_TERMINAL
    void buildTerminal();
    assistant::TerminalSpec terminalSpec(const std::string& kind, bool serving) const;
    std::unique_ptr<assistant::TerminalDock> dock;
    int servingTab = -1;
    std::unique_ptr<assistant::EventPump> eventPump;
#endif
};

}  // namespace xoj::mcp
