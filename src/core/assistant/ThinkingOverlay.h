/*
 * xournalai (based on Xournal++)
 *
 * Visible thinking: where the AI is working, the canvas shows a translucent grey veil with an animated outline, a
 * spinner button (click to cancel) and a one-line status. States: queued → thinking → done (a green tick that
 * fades) or failed (a red mark). Never silent.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <optional>    // for optional
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

#include "util/Rectangle.h"

namespace xoj::assistant {

class ThinkingOverlay final {
public:
    enum class State { Queued, Thinking, Done, Failed };

    /// Maps a page area (page index, page points) to the overlay's coordinates; nullopt when not visible
    using Mapper = std::function<std::optional<GdkRectangle>(size_t page, const xoj::util::Rectangle<double>&)>;
    using CancelHandler = std::function<void(int zoneId)>;
    /// The visible canvas area in the overlay's coordinates (buttons never leave it, e.g. onto the toolbars)
    using Bounds = std::function<std::optional<GdkRectangle>()>;

    /// `overlay` is the window's GtkOverlay (above the whole window content)
    ThinkingOverlay(GtkWidget* overlay, Mapper mapper, CancelHandler onCancel, Bounds bounds);
    ~ThinkingOverlay();
    ThinkingOverlay(const ThinkingOverlay&) = delete;
    ThinkingOverlay& operator=(const ThinkingOverlay&) = delete;

    /// A new zone; returns its id
    int add(size_t page, const xoj::util::Rectangle<double>& area, const std::string& text, State state);
    /// Changes a zone's state and (if not empty) its text; unknown ids are ignored
    void set(int id, State state, const std::string& text = {});
    /// Zones in a state
    std::vector<int> inState(State s) const;
    /// Number of queued or thinking zones
    size_t active() const;
    /// For tests and the status tool: zones as "id state page text"
    struct Info {
        int id;
        State state;
        size_t page;
        std::string text;
    };
    std::vector<Info> zones() const;
    static const char* name(State s);

private:
    struct Zone;
    static gboolean onDraw(GtkWidget* w, cairo_t* cr, gpointer self);
    static gboolean onTick(gpointer self);
    void layout();
    void removeZone(size_t index);

    GtkWidget* overlay = nullptr;  ///< weak
    GtkWidget* canvas = nullptr;   ///< weak: the drawing area (input passes through)
    Mapper mapper;
    CancelHandler onCancel;
    Bounds bounds;
    std::vector<Zone*> list;
    int nextId = 1;
    guint timer = 0;
    gulong positionHandler = 0;
    double phase = 0;
};

}  // namespace xoj::assistant
