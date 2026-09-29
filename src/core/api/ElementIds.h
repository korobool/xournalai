/*
 * xournalai (based on Xournal++)
 *
 * Stable, session-wide identifiers for document elements
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>        // for uint64_t
#include <mutex>          // for mutex
#include <optional>       // for optional
#include <string>         // for string
#include <unordered_map>  // for unordered_map
#include <vector>         // for vector

class Element;

namespace xoj::api {

/**
 * @brief Gives every element a short id ("e42") that stays the same for the whole session.
 *
 * Ids survive moves, restyling and undo/redo, because the undo stack keeps the same Element objects alive. When an
 * element object is destroyed its id is retired, so a later object at the same address never inherits it (the
 * registry observes Element destruction). Thread-safe: elements may be destroyed on worker threads.
 */
class ElementIds final {
public:
    /// The process-wide registry (installs the Element destruction observer on first use)
    static ElementIds& get();

    /// Returns the id of `e`, assigning a new one if `e` has none yet
    std::string idOf(const Element* e);
    /// Returns the id of `e` if it has one
    std::optional<std::string> existingId(const Element* e) const;
    /// Returns the element with this id ("e42" or "42"), or nullptr if the id is unknown or its element was destroyed
    const Element* lookup(const std::string& id) const;

    /// Number of live ids (for tests and diagnostics)
    size_t size() const;

    /// Attribution: remembers which agent operation created an element (e.g. "op7"); cleared on destruction
    void setOrigin(const Element* e, const std::string& operation);
    std::optional<std::string> origin(const Element* e) const;
    /// All live elements created by `operation`
    std::vector<const Element*> elementsOfOperation(const std::string& operation) const;

    /// Parses "e42" / "42" into 42; returns std::nullopt for malformed ids
    static std::optional<uint64_t> parse(const std::string& id);

private:
    ElementIds() = default;
    static void onElementDestroyed(const Element* e);

    mutable std::mutex mutex;
    std::unordered_map<const Element*, uint64_t> byElement;
    std::unordered_map<uint64_t, const Element*> byId;
    std::unordered_map<const Element*, std::string> origins;
    uint64_t next = 1;
};

}  // namespace xoj::api
