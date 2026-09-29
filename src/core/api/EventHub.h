/*
 * xournalai (based on Xournal++)
 *
 * EventHub: a cursor-based log of what changes in the document, and who changed it (user or agent)
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>        // for uint64_t
#include <deque>          // for deque
#include <functional>     // for function
#include <string>         // for string
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector

#include <glib.h>  // for gint64

#include "model/DocumentListener.h"  // for DocumentListener
#include "model/PageRef.h"           // for PageRef
#include "undo/UndoRedoHandler.h"    // for UndoRedoListener
#include "util/Rectangle.h"          // for Rectangle

class Control;
class Element;

namespace xoj::api {

struct DocEvent {
    uint64_t seq = 0;
    gint64 timeUs = 0;   ///< g_get_monotonic_time()
    std::string type;    ///< element_added | element_removed | element_changed | page_inserted | page_deleted |
                         ///< document_replaced
    std::string origin;  ///< "user" or "agent"
    size_t page = 0;     ///< 0-based
    std::vector<std::string> ids;
    xoj::util::Rectangle<double> area;  ///< affected area (page points), empty for page events
    std::string description;            ///< undo description of the step (e.g. "Draw stroke")
};

/**
 * @brief Watches the document. Every undoable change (drawing, erasing, moving, deleting, ...) is diffed per page,
 * pages inserted/deleted and document replacements are recorded. Events made while an agent tool runs are
 * attributed to the agent. Must live on the main thread; unregisters itself on destruction.
 */
class EventHub: public UndoRedoListener, public DocumentListener {
public:
    explicit EventHub(Control* control);
    ~EventHub() override;
    EventHub(const EventHub&) = delete;
    EventHub& operator=(const EventHub&) = delete;

    /// Runs queued diffs now (before reading the log or starting an agent operation)
    void flush();

    /// Events with seq > cursor (at most `max`, oldest first)
    std::vector<DocEvent> since(uint64_t cursor, size_t max) const;
    uint64_t lastSeq() const { return seq; }
    /// Monotonic time of the latest change made by the user (0 if none)
    gint64 lastUserChangeUs() const { return lastUserUs; }

    /// Agent tool calls bracket their work with these (nesting allowed)
    void beginAgentWork() { agentDepth++; }
    void endAgentWork() {
        if (agentDepth > 0) {
            agentDepth--;
        }
    }

    /// Called for every new event (e.g. to push notifications)
    void setListener(std::function<void(const DocEvent&)> l) { listener = std::move(l); }
    /// Additional listeners (e.g. the assistant's event pump); called after the main one
    void addListener(std::function<void(const DocEvent&)> l) { extraListeners.push_back(std::move(l)); }

    // UndoRedoListener
    void undoRedoChanged() override {}
    void undoRedoPageChanged(PageRef page) override;
    // DocumentListener
    void documentChanged(DocumentChangeType type) override;
    void pageInserted(size_t page) override;
    void pageDeleted(size_t page) override;
    void pageChanged(size_t page) override;

private:
    struct Snapshot {
        std::unordered_map<const Element*, xoj::util::Rectangle<double>> boxes;
    };
    void snapshotAll();
    Snapshot take(const PageRef& page) const;
    /// Queues a diff for when the application is idle (undo steps are registered before elements are inserted)
    void scheduleDiff(const PageRef& page);
    void diff(const PageRef& page, const std::string& origin);
    static gboolean runPending(gpointer self);
    void push(DocEvent e);

    Control* control;
    std::unordered_map<const XojPage*, Snapshot> pages;
    std::deque<DocEvent> log;
    uint64_t seq = 0;
    int agentDepth = 0;
    gint64 lastUserUs = 0;
    std::function<void(const DocEvent&)> listener;
    std::vector<std::function<void(const DocEvent&)>> extraListeners;
    std::vector<std::pair<PageRef, std::string>> pending;  ///< page, origin at notification time
    guint idleSource = 0;
};

}  // namespace xoj::api
