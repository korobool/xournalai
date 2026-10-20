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

#include "util/Point.h"  // for Point
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
    /// The Ask lasso being drawn (page coordinates): a dashed teal path; empty points clear it
    void setLasso(size_t page, std::vector<xoj::util::Point<double>> points);

    /// The AI highlighter's marks: translucent violet strokes over the page, not ink. A set of marks (`key`) is
    /// tied to a request's zone and goes away when the zone is done, failed or removed.
    struct Mark {
        std::vector<xoj::util::Point<double>> points;  ///< page coordinates
        double width = 12;                             ///< page points
    };
    /// Shows (or, with no marks, removes) the set `key` on `page`
    void setMarks(int key, size_t page, std::vector<Mark> marks);
    /// Ties the set `key` to `zone`: it goes away with it
    void bindMarks(int key, int zone);
    size_t markSets() const { return markList.size(); }  ///< (tests)

    /// The recording indicator ("on air"): a pill next to the pen with a pulsing red dot and level bars that move
    /// with the voice (Listening), or bouncing dots (Transcribing)
    enum class Recording { Off, Listening, Transcribing };
    /// `anchor`: the pen's position (page, page coordinates); none: the top of the canvas
    void setRecording(Recording r, std::optional<std::pair<size_t, xoj::util::Point<double>>> anchor = std::nullopt);
    Recording recording() const { return rec; }
    size_t levelCount() const { return levels.size(); }  ///< levels received while recording (tests)
    /// The microphone's level (RMS)
    void pushLevel(float rms);
    /// A short message in the recording pill for a moment (e.g. "Didn't catch that")
    void flashRecording(const std::string& message);

    /// Where an agent works out of the user's view (instead of taking the view there): a small chip at the top right
    /// of the canvas, e.g. "AI ↓ p. 7"; a tap calls `onTap`. Empty text hides it.
    void setPageChip(const std::string& text, std::function<void()> onTap = {});
    const std::string& pageChip() const { return chipText; }
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
    Recording rec = Recording::Off;
    std::optional<std::pair<size_t, xoj::util::Point<double>>> recAnchor;
    std::vector<float> levels;  ///< recent levels, oldest first
    std::string recMessage;
    gint64 recMessageUntilUs = 0;
    void drawRecording(cairo_t* cr);
    void drawZonePill(cairo_t* cr, const Zone& z, double zx, double zy, double zw, double zh, double alpha);
    void ensureTimer();
    struct MarkSet {
        int key;
        size_t page;
        std::vector<Mark> marks;
        int zone = 0;
    };
    std::vector<MarkSet> markList;
    void dropMarksOf(int zone);
    size_t lassoPage = 0;
    std::vector<xoj::util::Point<double>> lasso;
    int nextId = 1;
    guint timer = 0;
    gulong positionHandler = 0;
    double phase = 0;
    GtkWidget* chip = nullptr;  ///< weak
    std::string chipText;
    std::function<void()> chipTap;
};

}  // namespace xoj::assistant
