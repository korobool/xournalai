#include "ThinkingOverlay.h"

#include <algorithm>  // for count_if
#include <cmath>      // for M_PI

namespace xoj::assistant {

namespace {
/// `s` as valid UTF-8 (invalid bytes become U+FFFD)
std::string validUtf8(const std::string& s) {
    if (g_utf8_validate(s.c_str(), static_cast<gssize>(s.size()), nullptr)) {
        return s;
    }
    gchar* v = g_utf8_make_valid(s.c_str(), static_cast<gssize>(s.size()));
    std::string r(v);
    g_free(v);
    return r;
}
}  // namespace

namespace {
constexpr guint FRAME_MS = 50;
constexpr gint64 DONE_FADE_US = 1500 * 1000;
constexpr gint64 FAILED_KEEP_US = 5 * G_USEC_PER_SEC;
constexpr gint64 MAX_AGE_US = 10 * 60 * G_USEC_PER_SEC;  ///< a zone nobody finished disappears after this (the lease)
}  // namespace

struct ThinkingOverlay::Zone {
    int id = 0;
    size_t page = 0;
    xoj::util::Rectangle<double> area;
    std::string text;
    State state = State::Queued;
    gint64 createdUs = 0;
    gint64 stateUs = 0;
    GtkWidget* button = nullptr;  ///< weak: its stop button (click to cancel)
    GtkWidget* spinner = nullptr;  ///< (a still stop icon: a GtkSpinner redrew the window 60 times a second)
    GtkWidget* label = nullptr;
    GdkRectangle rect{0, 0, 0, 0};
    bool visible = false;
    GdkRectangle drawn{0, 0, 0, 0};  ///< where its outline and status pill were drawn last
    int shownState = -1;             ///< the state its button shows (-1: none yet)
    GdkRectangle drawnRect{0, 0, 0, 0};  ///< `rect` when its whole area was last redrawn
    int drawnState = -1;                 ///< likewise its state, and its text
    std::string drawnText;
    mutable GdkRectangle pill{0, 0, 0, 0};  ///< where its status pill was drawn (it animates, the veil does not)
};

namespace {
/// Where a zone is drawn: its outline (4 px out) and its status pill (above it, or below when there is no room;
/// up to ~330 px wide and kept on the canvas, so it may lie to the left of the zone)
GdkRectangle zoneArea(const GdkRectangle& r) {
    constexpr int PILL_W = 344, PILL_H = 4 + 6 + 32 + 4;
    return {r.x - PILL_W, r.y - PILL_H, r.width + 2 * PILL_W, r.height + 2 * PILL_H};
}

GdkRectangle unite(const GdkRectangle& a, const GdkRectangle& b) {
    if (a.width <= 0 || a.height <= 0) {
        return b;
    }
    if (b.width <= 0 || b.height <= 0) {
        return a;
    }
    GdkRectangle u;
    gdk_rectangle_union(&a, &b, &u);
    return u;
}

/// Positions an overlay child (through get-child-position), relaid out only when it moved
void placeChild(GtkWidget* w, int x, int y) {
    x = std::max(0, x) + 1;  // (+1: never NULL)
    y = std::max(0, y) + 1;
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "ai-x")) == x &&
        GPOINTER_TO_INT(g_object_get_data(G_OBJECT(w), "ai-y")) == y) {
        return;
    }
    g_object_set_data(G_OBJECT(w), "ai-x", GINT_TO_POINTER(x));
    g_object_set_data(G_OBJECT(w), "ai-y", GINT_TO_POINTER(y));
    gtk_widget_queue_resize(w);
}
}  // namespace

const char* ThinkingOverlay::name(State s) {
    switch (s) {
        case State::Queued:
            return "queued";
        case State::Thinking:
            return "thinking";
        case State::Done:
            return "done";
        case State::Failed:
            return "failed";
    }
    return "?";
}

ThinkingOverlay::ThinkingOverlay(GtkWidget* ov, Mapper m, CancelHandler cancel, Bounds b):
        overlay(ov), mapper(std::move(m)), onCancel(std::move(cancel)), bounds(std::move(b)) {
    // A widget without its own GdkWindow: pass-through only works for those (a GtkDrawingArea would swallow every
    // click and pen stroke in the window)
    canvas = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_has_window(canvas, FALSE);
    gtk_buildable_set_name(GTK_BUILDABLE(canvas), "aiThinkingOverlay");
    gtk_widget_set_halign(canvas, GTK_ALIGN_FILL);
    gtk_widget_set_valign(canvas, GTK_ALIGN_FILL);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), canvas);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), canvas, TRUE);  // your pen goes through
    g_signal_connect_after(canvas, "draw", G_CALLBACK(onDraw), this);
    // Place the spinner buttons exactly (only ours: other overlay children keep their normal placement)
    positionHandler =
            g_signal_connect(overlay, "get-child-position",
                             G_CALLBACK(+[](GtkOverlay*, GtkWidget* child, GdkRectangle* alloc, gpointer) -> gboolean {
                                 if (!g_object_get_data(G_OBJECT(child), "ai-x")) {
                                     return FALSE;
                                 }
                                 GtkRequisition nat;
                                 gtk_widget_get_preferred_size(child, nullptr, &nat);
                                 alloc->x = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "ai-x")) - 1;
                                 alloc->y = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child), "ai-y")) - 1;
                                 alloc->width = nat.width;
                                 alloc->height = nat.height;
                                 return TRUE;
                             }),
                             nullptr);
    gtk_widget_show(canvas);
    g_object_add_weak_pointer(G_OBJECT(overlay), reinterpret_cast<gpointer*>(&overlay));
    g_object_add_weak_pointer(G_OBJECT(canvas), reinterpret_cast<gpointer*>(&canvas));
}

ThinkingOverlay::~ThinkingOverlay() {
    if (timer) {
        g_source_remove(timer);
    }
    while (!list.empty()) {
        removeZone(list.size() - 1);
    }
    if (chip) {
        GtkWidget* c = chip;
        g_object_remove_weak_pointer(G_OBJECT(chip), reinterpret_cast<gpointer*>(&chip));
        gtk_widget_destroy(c);
    }
    if (canvas) {
        GtkWidget* c = canvas;
        g_object_remove_weak_pointer(G_OBJECT(canvas), reinterpret_cast<gpointer*>(&canvas));
        gtk_widget_destroy(c);
    }
    if (overlay) {
        if (positionHandler) {
            g_signal_handler_disconnect(overlay, positionHandler);
        }
        g_object_remove_weak_pointer(G_OBJECT(overlay), reinterpret_cast<gpointer*>(&overlay));
    }
}

int ThinkingOverlay::add(size_t page, const xoj::util::Rectangle<double>& area, const std::string& text, State state) {
    auto* z = new Zone;
    z->id = nextId++;
    z->page = page;
    z->area = area;
    z->text = text;
    z->state = state;
    z->createdUs = z->stateUs = g_get_monotonic_time();
    if (overlay) {
        // The spinner is a real button (click = cancel); the rest is drawn and lets input through
        z->button = gtk_button_new();
        gtk_buildable_set_name(GTK_BUILDABLE(z->button), "aiThinkingButton");
        gtk_widget_set_tooltip_text(z->button, "The AI is working here. Click to stop it.");
        gtk_widget_set_can_focus(z->button, FALSE);
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        z->spinner = gtk_image_new_from_icon_name("process-stop-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_box_pack_start(GTK_BOX(box), z->spinner, FALSE, FALSE, 0);  // the status text is drawn, not a widget
        gtk_widget_set_tooltip_text(z->button, (text + " (click to stop)").c_str());
        gtk_container_add(GTK_CONTAINER(z->button), box);
        g_object_set_data(G_OBJECT(z->button), "zone", GINT_TO_POINTER(z->id));
        g_signal_connect(z->button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer self) {
                             auto* ov = static_cast<ThinkingOverlay*>(self);
                             const int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "zone"));
                             if (ov->onCancel) {
                                 ov->onCancel(id);
                             }
                         }),
                         this);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), z->button);
        g_object_add_weak_pointer(G_OBJECT(z->button), reinterpret_cast<gpointer*>(&z->button));
    }
    list.push_back(z);
    layout();
    ensureTimer();
    return z->id;
}

void ThinkingOverlay::set(int id, State state, const std::string& text) {
    if (state == State::Done || state == State::Failed) {
        dropMarksOf(id);  // dealt with: the AI highlighter's marks go
    }
    for (Zone* z: list) {
        if (z->id == id) {
            z->state = state;
            z->stateUs = g_get_monotonic_time();
            if (!text.empty()) {
                z->text = text;
                if (z->button) {
                    gtk_widget_set_tooltip_text(z->button, (text + " (click to stop)").c_str());
                }
            }
        }
    }
    layout();
}

std::vector<int> ThinkingOverlay::inState(State s) const {
    std::vector<int> out;
    for (const Zone* z: list) {
        if (z->state == s) {
            out.push_back(z->id);
        }
    }
    return out;
}

size_t ThinkingOverlay::active() const {
    return static_cast<size_t>(std::count_if(list.begin(), list.end(), [](const Zone* z) {
        return z->state == State::Queued || z->state == State::Thinking;
    }));
}

std::vector<ThinkingOverlay::Info> ThinkingOverlay::zones() const {
    std::vector<Info> out;
    for (const Zone* z: list) {
        out.push_back({z->id, z->state, z->page, z->text});
    }
    return out;
}

void ThinkingOverlay::removeZone(size_t index) {
    Zone* z = list[index];
    invalidate(z->drawn);
    dropMarksOf(z->id);
    if (z->button) {
        GtkWidget* b = z->button;
        g_object_remove_weak_pointer(G_OBJECT(b), reinterpret_cast<gpointer*>(&z->button));
        gtk_widget_destroy(b);
    }
    delete z;
    list.erase(list.begin() + static_cast<std::ptrdiff_t>(index));
}

void ThinkingOverlay::layout() {
    for (Zone* z: list) {
        std::optional<GdkRectangle> r = mapper ? mapper(z->page, z->area) : std::nullopt;
        z->visible = r.has_value();
        if (r) {
            z->rect = *r;
        }
        // its whole part of the overlay, where it was and where it is now, when it moved (scrolling, zooming) or
        // changed; its animation redraws less (onTick)
        const GdkRectangle now = z->visible ? zoneArea(z->rect) : GdkRectangle{0, 0, 0, 0};
        if (!gdk_rectangle_equal(&now, &z->drawn) || z->drawnState != static_cast<int>(z->state) ||
            z->drawnText != z->text) {
            invalidate(z->drawn);
            invalidate(now);
            z->drawn = now;
            z->drawnState = static_cast<int>(z->state);
            z->drawnText = z->text;
        }
        if (!z->button) {
            continue;
        }
        if (!z->visible || z->state == State::Done) {
            if (gtk_widget_get_visible(z->button)) {
                gtk_widget_hide(z->button);
            }
            z->shownState = -1;
            continue;
        }
        // a compact spinner inside the zone's top-right corner: it only takes input where the AI is working (the
        // status text is drawn, so it never blocks your pen), and it never lands on the toolbars
        const auto area = bounds ? bounds() : std::nullopt;
        const int top = area ? area->y : 0, left = area ? area->x : 0;
        int x = z->rect.x + z->rect.width - 30;
        int y = z->rect.y + 2;
        x = std::max(x, std::max(z->rect.x, left + 2));
        y = std::max(y, top + 2);
        if (area) {
            x = std::min(x, area->x + area->width - 32);
            y = std::min(y, area->y + area->height - 32);
        }
        // positioned by get-child-position (margins would give the button an input area reaching the corner)
        placeChild(z->button, x, y);
        if (z->shownState != static_cast<int>(z->state)) {  // (each frame, this restyled and relaid out the window)
            z->shownState = static_cast<int>(z->state);
            gtk_widget_show_all(z->button);
            GtkStyleContext* ctx = gtk_widget_get_style_context(z->button);
            gtk_style_context_remove_class(ctx, "destructive-action");
            if (z->state == State::Failed) {
                gtk_style_context_add_class(ctx, "destructive-action");
            }
        }
    }
    if (chip) {
        const auto area = bounds ? bounds() : std::nullopt;
        if (chipText.empty() || !area) {
            if (gtk_widget_get_visible(chip)) {
                gtk_widget_hide(chip);
            }
        } else {
            GtkRequisition nat;
            gtk_widget_get_preferred_size(chip, nullptr, &nat);
            placeChild(chip, std::max(area->x, area->x + area->width - nat.width - 12), area->y + 8);
            if (!gtk_widget_get_visible(chip)) {
                gtk_widget_show(chip);
            }
        }
    }
}

void ThinkingOverlay::invalidate(const GdkRectangle& r) {
    if (canvas && r.width > 0 && r.height > 0) {
        gtk_widget_queue_draw_area(canvas, r.x, r.y, r.width, r.height);
    }
}

GdkRectangle ThinkingOverlay::pointsArea(size_t page, const std::vector<xoj::util::Point<double>>& points,
                                         double size) const {
    GdkRectangle out{0, 0, 0, 0};
    for (const auto& p: points) {
        if (auto r = mapper ? mapper(page, xoj::util::Rectangle<double>(p.x - size / 2, p.y - size / 2, size, size)) :
                              std::nullopt) {
            out = unite(out, *r);
        }
    }
    if (out.width > 0) {
        out = {out.x - 4, out.y - 4, out.width + 8, out.height + 8};  // (the line's width, antialiasing)
    }
    return out;
}

void ThinkingOverlay::invalidateRecording() {
    const GdkRectangle now = (rec != Recording::Off || g_get_monotonic_time() < recMessageUntilUs) ?
                                     recordingRect() :
                                     GdkRectangle{0, 0, 0, 0};
    invalidate(unite(recordingDrawn, now));
    recordingDrawn = now;
}

void ThinkingOverlay::invalidateLasso() {
    const GdkRectangle now = lasso.size() > 1 ? pointsArea(lassoPage, lasso, 1) : GdkRectangle{0, 0, 0, 0};
    invalidate(unite(lassoDrawn, now));
    lassoDrawn = now;
}

void ThinkingOverlay::invalidateMarks() {
    GdkRectangle now{0, 0, 0, 0};
    for (const auto& set: markList) {
        for (const auto& m: set.marks) {
            now = unite(now, pointsArea(set.page, m.points, m.width));
        }
    }
    invalidate(unite(marksDrawn, now));
    marksDrawn = now;
}

void ThinkingOverlay::setPageChip(const std::string& text, std::function<void()> onTap) {
    chipText = text;
    chipTap = std::move(onTap);
    if (overlay && !chip && !text.empty()) {
        chip = gtk_button_new_with_label("");
        gtk_buildable_set_name(GTK_BUILDABLE(chip), "aiAgentPageChip");
        gtk_widget_set_can_focus(chip, FALSE);  // (never the keyboard's)
        gtk_style_context_add_class(gtk_widget_get_style_context(chip), "osd");
        gtk_widget_set_tooltip_text(chip, "An agent is changing this page. Click to go there.");
        g_signal_connect(chip, "clicked", G_CALLBACK(+[](GtkButton*, gpointer self) {
                             auto tap = static_cast<ThinkingOverlay*>(self)->chipTap;  // (it may hide the chip)
                             if (tap) {
                                 tap();
                             }
                         }),
                         this);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), chip);
        g_object_add_weak_pointer(G_OBJECT(chip), reinterpret_cast<gpointer*>(&chip));
    }
    if (chip) {
        gtk_button_set_label(GTK_BUTTON(chip), text.c_str());
    }
    layout();
}

gboolean ThinkingOverlay::onTick(gpointer data) {
    auto* self = static_cast<ThinkingOverlay*>(data);
    const gint64 now = g_get_monotonic_time();
    for (size_t i = self->list.size(); i-- > 0;) {
        const Zone* z = self->list[i];
        const bool expired = (z->state == State::Done && now - z->stateUs > DONE_FADE_US) ||
                             (z->state == State::Failed && now - z->stateUs > FAILED_KEEP_US) ||
                             now - z->createdUs > MAX_AGE_US;
        if (expired) {
            self->removeZone(i);
        }
    }
    self->phase += 0.6;
    self->layout();  // follows scrolling and zooming
    for (const Zone* z: self->list) {
        if (!z->visible) {
            continue;
        }
        // what moves: the dashed outline (4 px out, 2 px wide) and the status pill
        const GdkRectangle& r = z->rect;
        self->invalidate({r.x - 7, r.y - 7, r.width + 14, 5});
        self->invalidate({r.x - 7, r.y + r.height + 2, r.width + 14, 5});
        self->invalidate({r.x - 7, r.y - 7, 5, r.height + 14});
        self->invalidate({r.x + r.width + 2, r.y - 7, 5, r.height + 14});
        self->invalidate(z->pill);
    }
    self->invalidateRecording();
    if (self->list.empty() && self->rec == Recording::Off && g_get_monotonic_time() >= self->recMessageUntilUs) {
        self->timer = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

void ThinkingOverlay::ensureTimer() {
    if (!timer) {
        timer = g_timeout_add(FRAME_MS, &ThinkingOverlay::onTick, this);
    }
}

void ThinkingOverlay::setRecording(Recording r, std::optional<std::pair<size_t, xoj::util::Point<double>>> anchor) {
    if (r != Recording::Off && rec == Recording::Off) {
        levels.clear();
    }
    rec = r;
    if (anchor || r == Recording::Off) {
        recAnchor = anchor;
    }
    ensureTimer();
    invalidateRecording();
}

void ThinkingOverlay::flashRecording(const std::string& message) {
    recMessage = message;
    recMessageUntilUs = g_get_monotonic_time() + 2200 * 1000;
    ensureTimer();
    invalidateRecording();
}

void ThinkingOverlay::pushLevel(float rms) {
    levels.push_back(rms);
    if (levels.size() > 9) {
        levels.erase(levels.begin());
    }
}

namespace {
void pillPath(cairo_t* cr, double x, double y, double w, double h) {
    const double r = h / 2;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + r, y + r, r, M_PI / 2, 3 * M_PI / 2);
    cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, M_PI / 2);
    cairo_close_path(cr);
}
}  // namespace

void ThinkingOverlay::drawZonePill(cairo_t* cr, const Zone& z, double zx, double zy, double zw, double zh,
                                   double alpha) {
    constexpr double H = 32;
    std::string text = z.text.empty() ? (z.state == State::Queued ? "waiting for the AI…" : "working…") : z.text;
    if (z.state == State::Done) {
        text = "done";
    }
    cairo_save(cr);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    // Valid UTF-8 only, and cut by whole characters: an invalid string puts the window's cairo context in an error
    // state, and the whole window stops painting
    text = validUtf8(text);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text.c_str(), &ext);
    if (ext.x_advance > 280) {  // long statuses are cut
        std::string body = text;
        while (ext.x_advance > 280 && g_utf8_strlen(body.c_str(), -1) > 1) {
            body.resize(static_cast<size_t>(g_utf8_find_prev_char(body.c_str(), body.c_str() + body.size()) -
                                            body.c_str()));
            text = body + "…";
            cairo_text_extents(cr, text.c_str(), &ext);
        }
    }
    const double W = ext.x_advance + 50;
    double x = zx, y = zy - H - 6;
    const auto area = bounds ? bounds() : std::nullopt;
    if (area && y < area->y + 4) {
        y = zy + zh + 6;  // no room above: below the zone
    }
    if (area) {
        x = std::clamp(x, area->x + 4.0, std::max(area->x + 4.0, area->x + area->width - W - 4.0));
    }
    z.pill = {static_cast<int>(x) - 3, static_cast<int>(y) - 3, static_cast<int>(W) + 8, static_cast<int>(H) + 9};
    cairo_push_group(cr);
    pillPath(cr, x + 1, y + 3, W, H);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
    cairo_fill(cr);
    pillPath(cr, x, y, W, H);
    cairo_set_source_rgba(cr, 0.11, 0.12, 0.14, 0.92);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.12);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    const double cx = x + 18, cy = y + H / 2;
    switch (z.state) {
        case State::Queued: {
            const double breathe = 0.5 + 0.5 * std::sin(phase / 3.0);  // waiting: a slow breathing dot
            cairo_arc(cr, cx, cy, 4 + 2 * breathe, 0, 2 * M_PI);
            cairo_set_source_rgba(cr, 0.75, 0.77, 0.8, 0.6 + 0.4 * breathe);
            cairo_fill(cr);
            break;
        }
        case State::Thinking:  // thinking: three teal dots orbiting
            for (int i = 0; i < 3; i++) {
                const double a = phase / 2.2 + i * 2 * M_PI / 3;
                cairo_new_sub_path(cr);
                cairo_arc(cr, cx + 7 * std::cos(a), cy + 7 * std::sin(a), 2.6 - 0.5 * i, 0, 2 * M_PI);
            }
            cairo_set_source_rgb(cr, 0.35, 0.85, 0.78);
            cairo_fill(cr);
            break;
        case State::Done:  // a green tick
            cairo_set_line_width(cr, 3);
            cairo_set_source_rgb(cr, 0.35, 0.8, 0.35);
            cairo_move_to(cr, cx - 6, cy);
            cairo_line_to(cr, cx - 1, cy + 5);
            cairo_line_to(cr, cx + 7, cy - 6);
            cairo_stroke(cr);
            break;
        case State::Failed:  // a red cross
            cairo_set_line_width(cr, 3);
            cairo_set_source_rgb(cr, 0.95, 0.3, 0.3);
            cairo_move_to(cr, cx - 5, cy - 5);
            cairo_line_to(cr, cx + 5, cy + 5);
            cairo_move_to(cr, cx + 5, cy - 5);
            cairo_line_to(cr, cx - 5, cy + 5);
            cairo_stroke(cr);
            break;
    }
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_move_to(cr, x + 36, cy + 5);
    cairo_show_text(cr, text.c_str());
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, alpha);
    cairo_restore(cr);
}

namespace {
constexpr double REC_W = 190, REC_H = 40;
}

GdkRectangle ThinkingOverlay::recordingRect() const {
    constexpr double W = REC_W, H = REC_H;
    // Where: next to the pen (not under the hand), else at the top of the canvas; always inside the canvas
    const auto area = bounds ? bounds() : std::nullopt;
    double x = area ? area->x + (area->width - W) / 2 : 40, y = area ? area->y + 12 : 40;
    if (recAnchor) {
        if (auto r = mapper(recAnchor->first,
                            xoj::util::Rectangle<double>(recAnchor->second.x, recAnchor->second.y, 0.01, 0.01))) {
            x = r->x + 28;
            y = r->y - H - 24;
        }
    }
    if (area) {
        x = std::clamp(x, area->x + 6.0, area->x + area->width - W - 6.0);
        y = std::clamp(y, area->y + 6.0, area->y + area->height - H - 6.0);
    }
    // (with its shadow)
    return {static_cast<int>(x) - 4, static_cast<int>(y) - 4, static_cast<int>(W) + 10, static_cast<int>(H) + 12};
}

void ThinkingOverlay::drawRecording(cairo_t* cr) {
    constexpr double W = REC_W, H = REC_H, R = H / 2;
    const GdkRectangle where = recordingRect();
    const double x = where.x + 4, y = where.y + 4;
    auto pill = [&](double px, double py) { pillPath(cr, px, py, W, H); };
    // shadow, body, hairline
    pill(x + 1, y + 3);
    cairo_set_source_rgba(cr, 0, 0, 0, 0.25);
    cairo_fill(cr);
    pill(x, y);
    cairo_set_source_rgba(cr, 0.11, 0.12, 0.14, 0.92);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.12);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);

    if (rec == Recording::Off) {  // a message ("Didn't catch that"): grey microphone dot, fading at the end
        const double left = static_cast<double>(recMessageUntilUs - g_get_monotonic_time()) / 1e6;
        cairo_arc(cr, x + 22, y + R, 7, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 0.7, 0.72, 0.75, std::min(1.0, left * 2));
        cairo_fill(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13);
        cairo_set_source_rgba(cr, 1, 1, 1, std::min(1.0, left * 2));
        cairo_move_to(cr, x + 40, y + R + 5);
        cairo_show_text(cr, validUtf8(recMessage).c_str());
        return;
    }
    const double cx = x + 22, cy = y + R;
    const bool listening = rec == Recording::Listening;
    const double pulse = std::fmod(phase / 7.0, 1.0);  // ~0.6 s
    if (listening) {
        // the red "on air" dot, with a halo growing from it
        cairo_arc(cr, cx, cy, 7 + 9 * pulse, 0, 2 * M_PI);
        cairo_set_source_rgba(cr, 0.95, 0.2, 0.25, 0.45 * (1 - pulse));
        cairo_fill(cr);
        cairo_arc(cr, cx, cy, 7, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.95, 0.18, 0.22);
        cairo_fill(cr);
        // level bars, newest on the right, moving with the voice
        const int n = static_cast<int>(levels.size());
        for (int i = 0; i < 9; i++) {
            const int j = n - 9 + i;  // the newest level on the right; empty bars on the left at first
            const float lv = j >= 0 ? levels[static_cast<size_t>(j)] : 0.0f;
            // decibels: quiet room (-50 dB) = flat, a normal voice about half, loud (-10 dB) = full
            const double v = lv > 0 ? std::clamp((20 * std::log10(static_cast<double>(lv)) + 50) / 40, 0.0, 1.0) : 0.0;
            const double h = 4 + 20 * v;
            const double bx = x + 40 + i * 6;
            cairo_rectangle(cr, bx, cy - h / 2, 3, h);
        }
        cairo_set_source_rgba(cr, 0.35, 0.85, 0.78, 0.95);  // teal, like the zones
        cairo_fill(cr);
    } else {
        cairo_arc(cr, cx, cy, 7, 0, 2 * M_PI);
        cairo_set_source_rgb(cr, 0.35, 0.85, 0.78);
        cairo_fill(cr);
        for (int i = 0; i < 3; i++) {
            const double bounce = std::max(0.0, std::sin((phase / 2.0) - i * 0.9));
            cairo_new_sub_path(cr);
            cairo_arc(cr, x + 44 + i * 11, cy + 3 - 6 * bounce, 3, 0, 2 * M_PI);
        }
        cairo_set_source_rgba(cr, 1, 1, 1, 0.85);
        cairo_fill(cr);
    }
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13);
    cairo_set_source_rgba(cr, 1, 1, 1, 0.95);
    cairo_move_to(cr, listening ? x + 100 : x + 84, cy + 5);
    cairo_show_text(cr, listening ? "Listening" : "Transcribing");
}

void ThinkingOverlay::setMarks(int key, size_t page, std::vector<Mark> marks) {
    auto it = std::find_if(markList.begin(), markList.end(), [&](const MarkSet& m) { return m.key == key; });
    if (marks.empty()) {
        if (it != markList.end()) {
            markList.erase(it);
        }
    } else if (it != markList.end()) {
        it->page = page;
        it->marks = std::move(marks);
    } else {
        markList.push_back({key, page, std::move(marks), 0});
    }
    invalidateMarks();
}

void ThinkingOverlay::bindMarks(int key, int zone) {
    for (auto& m: markList) {
        if (m.key == key) {
            m.zone = zone;
        }
    }
}

void ThinkingOverlay::dropMarksOf(int zone) {
    const size_t before = markList.size();
    markList.erase(std::remove_if(markList.begin(), markList.end(),
                                  [&](const MarkSet& m) { return zone != 0 && m.zone == zone; }),
                   markList.end());
    if (markList.size() != before) {
        invalidateMarks();
    }
}

void ThinkingOverlay::setLasso(size_t page, std::vector<xoj::util::Point<double>> points) {
    lassoPage = page;
    lasso = std::move(points);
    invalidateLasso();
}

gboolean ThinkingOverlay::onDraw(GtkWidget*, cairo_t* cr, gpointer data) {
    auto* self = static_cast<ThinkingOverlay*>(data);
    const gint64 now = g_get_monotonic_time();
    // The AI highlighter's marks (violet, translucent, like a highlighter)
    for (const auto& set: self->markList) {
        for (const auto& m: set.marks) {
            bool first = true;
            double width = 0;
            for (const auto& p: m.points) {
                auto r = self->mapper(set.page, xoj::util::Rectangle<double>(p.x, p.y, m.width, m.width));
                if (!r) {
                    continue;
                }
                width = r->width;
                first ? cairo_move_to(cr, r->x, r->y) : cairo_line_to(cr, r->x, r->y);
                first = false;
            }
            if (first) {
                continue;
            }
            if (m.points.size() == 1) {
                cairo_rel_line_to(cr, 0.01, 0);
            }
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_set_line_width(cr, std::max(2.0, width));
            cairo_set_source_rgba(cr, 0.71, 0.48, 1.0, 0.45);
            cairo_stroke(cr);
        }
    }
    if (self->lasso.size() > 1) {
        // The Ask lasso: where the user is pointing
        bool first = true;
        for (const auto& p: self->lasso) {
            if (auto r = self->mapper(self->lassoPage, xoj::util::Rectangle<double>(p.x, p.y, 0.01, 0.01))) {
                first ? cairo_move_to(cr, r->x, r->y) : cairo_line_to(cr, r->x, r->y);
                first = false;
            }
        }
        const double dashes[] = {5.0, 3.0};
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_line_width(cr, 2.0);
        cairo_set_source_rgba(cr, 0.05, 0.58, 0.53, 0.95);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
    }
    for (const Zone* z: self->list) {
        if (!z->visible) {
            continue;
        }
        const double x = z->rect.x - 4, y = z->rect.y - 4, w = z->rect.width + 8, h = z->rect.height + 8;
        double alpha = 1.0;
        if (z->state == State::Done) {
            alpha = std::max(0.0, 1.0 - static_cast<double>(now - z->stateUs) / DONE_FADE_US);
        }
        // the veil: "working here" (your ink stays visible underneath)
        if (z->state == State::Thinking) {
            cairo_set_source_rgba(cr, 0.45, 0.47, 0.5, 0.16);
            cairo_rectangle(cr, x, y, w, h);
            cairo_fill(cr);
        }
        // the outline: dashed, moving while thinking
        const double dashes[] = {6.0, 4.0};
        cairo_set_line_width(cr, 2.0);
        switch (z->state) {
            case State::Queued:
                cairo_set_source_rgba(cr, 0.45, 0.47, 0.5, 0.8);
                cairo_set_dash(cr, dashes, 2, 0);
                break;
            case State::Thinking:
                cairo_set_source_rgba(cr, 0.05, 0.58, 0.53, 0.95);  // teal
                cairo_set_dash(cr, dashes, 2, -self->phase * 4);
                break;
            case State::Done:
                cairo_set_source_rgba(cr, 0.13, 0.55, 0.13, 0.9 * alpha);
                cairo_set_dash(cr, nullptr, 0, 0);
                break;
            case State::Failed:
                cairo_set_source_rgba(cr, 0.75, 0.1, 0.1, 0.9);
                cairo_set_dash(cr, dashes, 2, 0);
                break;
        }
        cairo_rectangle(cr, x, y, w, h);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0);
        // The status pill above the zone (drawn: it lets the pen through): an animated sign of what is going on
        self->drawZonePill(cr, *z, x, y, w, h, alpha);
    }
    if (self->rec != Recording::Off || now < self->recMessageUntilUs) {  // on top of everything
        cairo_save(cr);
        self->drawRecording(cr);
        cairo_restore(cr);
    }
    return FALSE;
}

}  // namespace xoj::assistant
