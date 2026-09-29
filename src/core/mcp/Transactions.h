/*
 * xournalai (based on Xournal++)
 *
 * Edit transactions: agents (the serving session's subagents) think in parallel, but their edits land one at a
 * time, each as ONE ordered block and ONE undo step.
 *
 * A transaction claims an area of a page and gets a private draft (a hidden layer) to prepare new content in. Its
 * commit is an ordered list of operations (draw the draft instantly or like a stylus, delete, restyle, move) played
 * in order; commits are serialized. At commit time the change log is checked: if strokes the transaction depends on
 * changed since it began, the commit is refused (the draft is kept, so the agent can re-read and commit again).
 * Limits: at most N open transactions, no two claims overlapping, a lease after which a forgotten one is aborted.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for uint64_t
#include <deque>       // for deque
#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for unique_ptr
#include <optional>    // for optional
#include <string>      // for string
#include <vector>      // for vector

#include <glib.h>  // for gint64

#include "util/Rectangle.h"

#include "Json.h"

class Control;

namespace xoj::mcp {

class McpServer;

struct Transaction {
    std::string id;                                      ///< "t1", "t2", ...
    size_t page = 0;                                     ///< 0-based
    std::optional<xoj::util::Rectangle<double>> region;  ///< the claimed area
    std::vector<std::string> base;                       ///< element ids the work is based on
    std::string draft;                                   ///< its private draft (id)
    std::string draftLayer;                              ///< draw into this layer
    std::string label;                                   ///< what it does (shown to the user)
    int zone = 0;                                        ///< the thinking zone it belongs to (0: none)
    uint64_t cursor = 0;                                 ///< change-log position the work is based on
    gint64 beganUs = 0;
    bool committing = false;
};

class Transactions final {
public:
    explicit Transactions(McpServer& server): server(server) {}

    static constexpr gint64 LEASE_US = 10 * 60 * G_USEC_PER_SEC;

    /// Opens a transaction; throws std::invalid_argument when the limit is reached or the area is claimed
    const Transaction& begin(size_t page, std::optional<xoj::util::Rectangle<double>> region,
                             std::vector<std::string> base, std::string label, int zone);
    /// Plays `ops` (ordered) as one undo step. `done(result)` is called once: {"committed": true, …} or an error
    /// message in "error" (then "conflict": true when strokes changed meanwhile; the transaction stays open)
    void commit(const std::string& id, json ops, std::function<void(json)> done);
    void abort(const std::string& id, const std::string& why);
    /// Stop: every open transaction is aborted
    void abortAll(const std::string& why);

    std::vector<Transaction> list();
    size_t open() const { return txns.size(); }

    /// Called when a transaction ends: (zone, state "done" | "failed", reason)
    void setZoneListener(std::function<void(int zone, const std::string& state, const std::string& why)> l) {
        zoneListener = std::move(l);
    }

private:
    struct Pending {
        std::string id;
        json ops;
        std::function<void(json)> done;
    };
    void expire();
    void pump();
    void play(Transaction t, json ops, std::function<void(json)> done);
    std::optional<std::string> conflict(const Transaction& t, const std::vector<std::string>& touched);
    void finish(const std::string& id, const std::string& state, const std::string& why);

    McpServer& server;
    std::map<std::string, Transaction> txns;
    std::deque<Pending> queue;
    bool playing = false;
    unsigned counter = 0;
    std::map<std::string, std::string> aborted;  ///< recently aborted ids → why (so their commit gets a clear answer)
    std::function<void(int, const std::string&, const std::string&)> zoneListener;
};

}  // namespace xoj::mcp
