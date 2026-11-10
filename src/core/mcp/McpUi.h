/*
 * xournalai (based on Xournal++)
 *
 * In-app UI of the MCP server: status strip, "AI Agent" menu actions (pause, AI layer), highlights
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <map>
#include <memory>    // for unique_ptr
#include <optional>  // for optional
#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "model/PageRef.h"  // for PageRef
#include "util/Point.h"     // for Point
#include "util/Rectangle.h"

#include "assistant/VoiceActivity.h"  // for VoiceActivity
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

namespace xoj::assistant {
class QuickTools;
}  // namespace xoj::assistant

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
class QuestionPanel;
class RecordingChooser;
class ResearchBrowser;
class FocusGuard;
}  // namespace xoj::assistant
namespace xoj::api {
struct DocEvent;
}
namespace xoj::audio {
class TalkCapture;
}
namespace xoj::assistant {
class Transcriber;
}

namespace xoj::mcp {

class LiveNotes;

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
    /// "Connect an agent…": ready-made commands and configs for other agents (Claude Code, Codex, Gemini, …)
    void showConnect();

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
    /// Worker sessions: Claude Code in tabs of the AI terminal, each started by the serving session for one job (its
    /// brief), with only the tools of the live-annotation skill's safe list (permission mode dontAsk). Up to
    /// assistant.max_parallel at once. Errors as text in `error`.
    /// The zone a worker is shown busy in from its start (its slot): it is the worker's, and the brief tells it its id
    struct WorkerZone {
        size_t page = 0;
        xoj::util::Rectangle<double> area;
        std::string status;           ///< e.g. "«Notations»: thinking (effort high)…"
        std::optional<uint32_t> rgb;  ///< the topic's colour (0xRRGGBB)
    };
    json startWorker(const std::string& name, const std::string& brief, const std::string& effort, std::string& error,
                     const std::optional<WorkerZone>& zone = std::nullopt);
    bool closeWorker(const std::string& name);
    /// Ends the zones a worker still shows as busy (failed: `why`)
    void endWorkerZones(const std::string& name, const std::string& why);
    json workers() const;
#endif
    /// The question panel (1.17.2): an agent asks, the owner taps an answer. `options`: (label, value); none = a yes/no
    /// question. `done` gets (value, by: owner | timeout | cancelled). Returns the question's id (0: no panel)
    int askOwner(const std::string& who, const std::string& text,
                 const std::vector<std::pair<std::string, std::string>>& options, const std::string& fallback,
                 int timeoutS, std::function<void(const std::string& value, const std::string& by)> done);
    bool cancelQuestion(int id);
    /// An intent for the serving session (delivered when it is idle); nothing without the terminal
    void wakeServing(const std::string& text);
    /// The research browser (🔎; made when first needed)
    assistant::ResearchBrowser* research();
    void researchStarting();             ///< (a pop-up: it starts)
    void researchRunning(bool running);  ///< «📌 Insert selection» shows while it runs
    /// Opens it for signing in (Google refuses to sign in while the app is connected to it); back when he closes it
    void researchSignIn();
    void researchUnavailable(const std::string& error);  ///< (a pop-up: why not, or that it is open for signing in)
    /// (tests) the focus guard's state, the «↩ Back» pill, «N new»
    nlohmann::json researchFocusState() const;
    /// «📌 Insert selection»: what the owner selected in the research browser goes on the page he looks at, as a quote
    /// with its source (undoable, his)
    void insertResearchSelection();
    /// The questions waiting, and the answers of the last ones
    json questions() const;
    std::optional<json> questionAnswer(int id) const;
    /// Local speech to text for Ask (null if disabled or not built)
    assistant::SpeechToText* speech() const { return speechToText.get(); }
    /// Ask (pen button held: listen; the lasso tells where): its state and the last request, for app_status
    json askStatus() const;
    /// Recordings waiting for the owner's choice, and the last one chosen
    json recordingStatus() const;

    /// Live notes (experimental): starts transcribing the recording `name` (which has just started) while it goes on;
    /// false with `error` if it can't
    /// `mode`: one of LiveModes.h's (empty: the default, 🧠 conspect); the options are the remembered ones
    bool beginLive(const std::string& name, std::string& error, const std::string& mode = {});
    /// The live-notes session going on, or the last one (null if none)
    LiveNotes* liveNotes() const { return live.get(); }
    /// ✂: the piece being spoken goes to the recognizer now
    void cutLive();

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
    GtkWidget* settings = nullptr;       ///< weak (strip, label, pauseButton too)
    GtkWidget* connectDialog = nullptr;  ///< weak
    GtkWidget* flashPopover = nullptr;   ///< the "drawn here" hint, reused (owned)
    guint flashTimer = 0;
    void buildToolbar();
    std::unique_ptr<assistant::AiToolbar> aiToolbar;
    std::unique_ptr<assistant::QuickTools> quickTools;
    void buildThinking();
    void onServingChanged();
    std::unique_ptr<assistant::ThinkingOverlay> thinkingOverlay;
    int lastServingState = 0;

    // Where an agent works out of the user's view: a chip "AI ↓ p. N" instead of taking the view there
    void noteAgentWork(const api::DocEvent& e);
    void updateAgentChip();
    PageRef agentPage;
    gint64 agentWorkUs = 0;
    guint agentChipTimer = 0;

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
    /// AI highlighter: highlights like the highlighter while listening; a pause after the last stroke opens Ask
    void armHighlighter(bool on);
    gulong toolWatch = 0;
    /// The owner picked another tool (anywhere): the AI highlighter and lasso end
    void onToolPicked();
    void finishHighlight();
    bool highlighterArmed = false;
    PageRef highlightPage;
    std::vector<std::vector<xoj::util::Point<double>>> highlightPaths;  ///< the marks being drawn (page points)
    int highlightKey = 0;      ///< their set in the overlay
    int nextHighlightKey = 1;
    bool highlightListening = false;
    guint highlightTimer = 0;  ///< the mode bar's tick (time, marks, the auto-finish countdown)
    gint64 highlightStartUs = 0, lastMarkUs = 0, lastVoiceUs = 0;
    assistant::VoiceActivity voice;  ///< talking keeps the AI highlighter on (judged against the mic's own noise)
    gulong escapeHandler = 0;
    void highlightTick();

    /// Pen button held while the lasso is armed or the popover is open: dictation into it
    void dictate(bool pressed);
    void onAsk(const assistant::AskCapture& c);
    /// A Xournal++ recording ended: the owner chooses what it is (instructions, notes, just audio) before anything
    /// is sent
    void onRecordingFinished(const std::string& file, const std::string& name, int64_t durationMs);
    /// The owner's choice: tell the serving session (file, length, page, strokes written meanwhile) accordingly
    void onRecordingChosen(const std::string& file, const std::string& name, int64_t durationMs, int choice,
                           const std::string& request);
    std::unique_ptr<assistant::RecordingChooser> recordingChooser;
    std::unique_ptr<LiveNotes> live;  ///< the session going on, or the last one
    std::vector<std::unique_ptr<LiveNotes>> pastLive;  ///< ended, still transcribing their last pieces
    void endLive(const std::string& name);
    void startLiveRecording(const std::string& mode);  ///< a mode's tile of the long press on Record
    bool liveEnabled = false;   ///< (the experimental switch)
    json lastRecording;  ///< the last choice (app_status)
    /// Zones of handed-over recordings still being worked on, with "Notes" / "Instructions": the status line shows
    /// their progress
    std::vector<std::pair<int, std::string>> recordingZones;
    void updateRecordingProgress();

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
    /// «Держи и говори»: held, the microphone is recorded on its own; let go, the phrase goes to the speech recognizer
    /// (the live notes' one) and then, as a command, to the serving session. `device`: false: sound only from tests
public:
    void talkPressed(bool down, bool device = true);
    void talkFeed(const float* frames, size_t count, int channels, int rate);
private:
    void onTalkResult(const std::string& id, bool ok, const std::string& text, const std::string& error);
    std::unique_ptr<xoj::audio::TalkCapture> talkCapture;
    std::unique_ptr<assistant::Transcriber> talkRecognizer;
    std::map<std::string, std::pair<gint64, gint64>> talkTimes;  ///< a phrase's id: when it was said
    gint64 talkStartUs = 0;
    int talkSeq = 0;
    guint talkIdle = 0;  ///< lets the recognizer go after a while without phrases
    std::unique_ptr<assistant::ResearchBrowser> researchBrowser;
    std::unique_ptr<assistant::FocusGuard> focusGuard;  ///< (with the research browser: X11)
    guint seenTimer = 0;                                ///< (while he is in the research browser)
    guint adoptIdle = 0;  ///< (at start: the research browser still running from before is taken up again)
    void researchOwnerInBrowser(bool in);
    void buildQuestions();
    void onQuestionAnswered(int id, const std::string& value, const std::string& by);
    std::unique_ptr<assistant::QuestionPanel> questionPanel;
    std::map<int, std::function<void(const std::string&, const std::string&)>> questionDone;
    std::map<int, json> questionAnswers;  ///< the last answers (for agents that look later)
#ifdef ENABLE_AI_TERMINAL
    void buildTerminal();
    assistant::TerminalSpec terminalSpec(const std::string& kind, bool serving) const;
    std::unique_ptr<assistant::TerminalDock> dock;
    int servingTab = -1;
    std::map<std::string, int> workerTabs;  ///< worker name → its tab
    /// What the app knows of a worker from its own hooks (the app follows its workers; the agents need not report)
    struct WorkerStatus {
        std::string state = "starting";  ///< starting, busy, finished (its turn ended: its one job is over), ended
        std::string activity = "starting";  ///< what it does now, e.g. "searching the web"
        gint64 startedUs = 0;
    };
    std::map<std::string, WorkerStatus> workerStatus;
    void onWorkerHook(const json& report);
    std::string workerActivity(const std::string& event, const json& payload) const;
    bool servingClaude = false;  ///< the serving tab runs Claude Code (not Codex, not a test's command)
    /// At exit: stops a serving Claude Code session left in its background daemon
    void stopBackgroundServing();
    std::unique_ptr<assistant::EventPump> eventPump;
#endif
};

}  // namespace xoj::mcp
