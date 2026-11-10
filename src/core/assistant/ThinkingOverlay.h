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
#include <cstdint>     // for uint32_t
#include <optional>    // for optional
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

#include "util/Point.h"  // for Point
#include "util/Rectangle.h"

namespace xoj::assistant {

/// A tap on a question's bubble on the canvas counts only from a finger or a mouse: never the pen, nor a touch this
/// soon after the pen (`sincePenUs`: the palm resting while he writes)
bool bubbleTapAllowed(GdkInputSource source, gint64 sincePenUs);

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

    /// A new zone; returns its id. `owner`: the worker session it belongs to (empty: the serving session or the app):
    /// a worker's zone lasts until it is ended, or the worker is (an hour at most). `rgb` (0xRRGGBB): its outline
    /// and veil in that colour (e.g. its topic's) instead of teal and grey.
    int add(size_t page, const xoj::util::Rectangle<double>& area, const std::string& text, State state,
            const std::string& owner = {}, std::optional<uint32_t> rgb = std::nullopt);
    /// A zone an agent opened itself (`thinking` op=start): it lasts until the agent ends it (or the owner stops it,
    /// or its lease), not only for the agent's turn
    void setKept(int id);
    bool isKept(int id) const;
    /// The worker a zone belongs to (empty: none, or no such zone)
    std::string ownerOf(int id) const;
    /// The queued or thinking zones of a worker
    std::vector<int> activeOf(const std::string& owner) const;
    /// A worker's zone: what the app sees it doing ("searching the web"), shown under its status with the time it
    /// has been working
    void setDetail(int id, const std::string& detail);
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
        std::string owner;
        std::string detail;
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
    /// Voice control: what was just heard pops up at the top of the canvas for a few seconds (a command: ▶ and teal;
    /// context: 🎙 and grey), then fades
    void flashHeard(const std::string& text, bool command,
                    std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> where = std::nullopt);
    /// An agent's short answer (voice control's dialogue): a violet tip by the region (or at the top), longer shown
    void flashReply(const std::string& text,
                    std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> where = std::nullopt);
    std::string heard() const;  ///< (tests: the pop-up shown now, "" when none)

    /// Where an agent works out of the user's view (instead of taking the view there): a small chip at the top right
    /// of the canvas, e.g. "AI ↓ p. 7"; a tap calls `onTap`. Empty text hides it.
    void setPageChip(const std::string& text, std::function<void()> onTap = {});

    /// A question about something on the page (1.19.0; the owner: «если я нарисовал бабочку и ожидаю, что она
    /// улучшится, а слева внизу появился тип, я могу это не заметить»): a dashed outline round it in the question's
    /// colour and, at once, a bubble beside it (on the side away from his hand, never over it) with the short question
    /// and its buttons. It follows scrolling and zooming and hides while the area is off screen; a thin bar under it
    /// shows the time left. The full question stays in the question panel, with the same number and colour.
    struct QuestionBubble {
        int id = 0;
        std::string mark;  ///< its number, as in the panel ("②")
        size_t page = 0;
        xoj::util::Rectangle<double> area;
        std::string text;                                          ///< short
        std::vector<std::pair<std::string, std::string>> options;  ///< (label, value); none: Yes / No
        uint32_t rgb = 0x2d6fe0;
        gint64 startUs = 0;
        gint64 deadlineUs = 0;  ///< (0: no time limit)
    };
    using BubbleAnswer = std::function<void(int id, const std::string& value)>;
    void showQuestion(QuestionBubble q, BubbleAnswer onAnswer);
    void hideQuestion(int id);
    /// (tests) the questions shown on the canvas now
    std::vector<int> questionsShown() const;
    const std::string& pageChip() const { return chipText; }
    static const char* name(State s);

private:
    struct Zone;
    struct Bubble;
    std::vector<Bubble*> bubbleList;
    void layoutBubbles();
    void drawBubbles(cairo_t* cr);
    void removeBubble(size_t index);
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
    std::string heardText;
    bool heardCommand = false;
    gint64 heardUntilUs = 0;
    double heardWidth = 0;
    int heardKind = 0;  ///< 0 heard, 1 a command, 2 an agent's reply
    std::vector<std::string> heardLines;
    void showTip(const std::string& text, int kind,
                 std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> where);
    double heardHeight() const;
    std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> heardWhere;  ///< the region it is about
    GdkRectangle heardDrawn{0, 0, 0, 0};
    GdkRectangle heardRect() const;
    void invalidateHeard();
    void drawHeard(cairo_t* cr);
    void drawRecording(cairo_t* cr);
    /// Where the recording pill is (next to the pen, else at the top of the canvas)
    GdkRectangle recordingRect() const;
    /*
     * Redraws: only the parts that change. A redraw of the whole overlay, which lies over the whole window,
     * repainted the window with its toolbars and icons at every frame while an agent worked: ~40 000 X requests a
     * second, which used up the app's X resource ids within minutes; the 2026-10-20 15:23 freeze waited for a new
     * range of them (XC-MISC)
     */
    void invalidate(const GdkRectangle& r);
    /// The area of points on a page (each `size` page points wide), in the overlay's coordinates
    GdkRectangle pointsArea(size_t page, const std::vector<xoj::util::Point<double>>& points, double size) const;
    void invalidateRecording();
    void invalidateLasso();
    void invalidateMarks();
    GdkRectangle lassoDrawn{}, marksDrawn{}, recordingDrawn{};
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
