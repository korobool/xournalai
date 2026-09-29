/*
 * xournalai (based on Xournal++)
 *
 * In-app UI of the MCP server: status strip, "AI Agent" menu actions (pause, AI layer), highlights
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr
#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "util/Rectangle.h"

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
}
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
#ifdef ENABLE_AI_TERMINAL
    void buildTerminal();
    assistant::TerminalSpec terminalSpec(const std::string& kind, bool serving) const;
    std::unique_ptr<assistant::TerminalDock> dock;
    int servingTab = -1;
    std::unique_ptr<assistant::EventPump> eventPump;
#endif
};

}  // namespace xoj::mcp
