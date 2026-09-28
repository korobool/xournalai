/*
 * Xournal++ (xournalai)
 *
 * Drafts: hidden scratch layers where agents iterate on a drawing before committing it
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <map>     // for map
#include <string>  // for string

#include "model/PageRef.h"  // for PageRef

class Control;
class Layer;

namespace xoj::api {

struct Draft {
    std::string id;  ///< "d1", "d2", ...
    PageRef page;
    Layer* layer = nullptr;
    std::string layerName;  ///< pass this as `layer` to the drawing tools
};

/**
 * @brief Registry of open drafts.
 *
 * A draft is a hidden layer on top of a page. Content drawn into it creates no undo entries (the draft is private
 * scratch space); committing moves the content into a real layer as one undo step, discarding deletes it.
 */
class Drafts {
public:
    static Drafts& get();

    /// Creates a hidden draft layer on `page` (0-based)
    const Draft& begin(Control* control, size_t page);
    /// Throws std::invalid_argument if the draft does not exist (or its layer vanished, e.g. document replaced)
    Draft& find(Control* control, const std::string& id);
    /// True if `layer` is the layer of an open draft
    bool isDraftLayer(const Layer* layer) const;
    /// Removes the draft layer from its page and deletes it together with any remaining content
    void close(Control* control, const std::string& id);

    const std::map<std::string, Draft>& all() const { return drafts; }

private:
    std::map<std::string, Draft> drafts;
    unsigned counter = 0;
};

}  // namespace xoj::api
