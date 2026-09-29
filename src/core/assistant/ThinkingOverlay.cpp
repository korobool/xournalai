#include "ThinkingOverlay.h"

#include <algorithm>  // for count_if
#include <cmath>      // for M_PI

namespace xoj::assistant {

namespace {
constexpr guint FRAME_MS = 50;
constexpr gint64 DONE_FADE_US = 1500 * 1000;
constexpr gint64 FAILED_KEEP_US = 5 * G_USEC_PER_SEC;
constexpr gint64 MAX_AGE_US = 5 * 60 * G_USEC_PER_SEC;  ///< a zone nobody finished disappears after this
}  // namespace

struct ThinkingOverlay::Zone {
    int id = 0;
    size_t page = 0;
    xoj::util::Rectangle<double> area;
    std::string text;
    State state = State::Queued;
    gint64 createdUs = 0;
    gint64 stateUs = 0;
    GtkWidget* button = nullptr;  ///< weak: spinner button (click to cancel)
    GtkWidget* spinner = nullptr;
    GtkWidget* label = nullptr;
    GdkRectangle rect{0, 0, 0, 0};
    bool visible = false;
};

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
        z->spinner = gtk_spinner_new();
        z->label = gtk_label_new(text.c_str());
        gtk_label_set_ellipsize(GTK_LABEL(z->label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(z->label), 40);
        gtk_box_pack_start(GTK_BOX(box), z->spinner, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), z->label, FALSE, FALSE, 0);
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
    if (!timer) {
        timer = g_timeout_add(FRAME_MS, &ThinkingOverlay::onTick, this);
    }
    return z->id;
}

void ThinkingOverlay::set(int id, State state, const std::string& text) {
    for (Zone* z: list) {
        if (z->id == id) {
            z->state = state;
            z->stateUs = g_get_monotonic_time();
            if (!text.empty()) {
                z->text = text;
                if (z->label) {
                    gtk_label_set_text(GTK_LABEL(z->label), text.c_str());
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
        if (!z->button) {
            continue;
        }
        if (!z->visible || z->state == State::Done) {
            gtk_widget_hide(z->button);
            continue;
        }
        // above the zone's top-left corner, or inside it when that would leave the canvas (never on the toolbars)
        const auto area = bounds ? bounds() : std::nullopt;
        const int top = area ? area->y : 0, left = area ? area->x : 0;
        int y = z->rect.y - 28 >= top ? z->rect.y - 28 : z->rect.y + 2;
        y = std::max(y, top + 2);
        if (area) {
            y = std::min(y, area->y + area->height - 28);
        }
        const int x = std::max(z->rect.x, left + 2);
        // positioned by get-child-position (margins would give the button an input area reaching the corner)
        g_object_set_data(G_OBJECT(z->button), "ai-x", GINT_TO_POINTER(std::max(0, x) + 1));  // +1: never NULL
        g_object_set_data(G_OBJECT(z->button), "ai-y", GINT_TO_POINTER(std::max(0, y) + 1));
        gtk_widget_queue_resize(z->button);
        gtk_widget_show_all(z->button);
        if (z->state == State::Thinking) {
            gtk_spinner_start(GTK_SPINNER(z->spinner));
        } else {
            gtk_spinner_stop(GTK_SPINNER(z->spinner));
            if (z->state == State::Queued) {
                gtk_widget_hide(z->spinner);
            }
        }
        GtkStyleContext* ctx = gtk_widget_get_style_context(z->button);
        gtk_style_context_remove_class(ctx, "destructive-action");
        if (z->state == State::Failed) {
            gtk_style_context_add_class(ctx, "destructive-action");
        }
    }
    if (canvas) {
        gtk_widget_queue_draw(canvas);
    }
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
    if (self->list.empty()) {
        self->timer = 0;
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

gboolean ThinkingOverlay::onDraw(GtkWidget*, cairo_t* cr, gpointer data) {
    auto* self = static_cast<ThinkingOverlay*>(data);
    const gint64 now = g_get_monotonic_time();
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
        // done: a tick; failed: a cross (top-right corner)
        if (z->state == State::Done || z->state == State::Failed) {
            const double cx = x + w - 10, cy = y + 10;
            cairo_set_line_width(cr, 3);
            if (z->state == State::Done) {
                cairo_move_to(cr, cx - 6, cy);
                cairo_line_to(cr, cx - 1, cy + 5);
                cairo_line_to(cr, cx + 7, cy - 6);
            } else {
                cairo_move_to(cr, cx - 5, cy - 5);
                cairo_line_to(cr, cx + 5, cy + 5);
                cairo_move_to(cr, cx + 5, cy - 5);
                cairo_line_to(cr, cx - 5, cy + 5);
            }
            cairo_stroke(cr);
        }
    }
    return FALSE;
}

}  // namespace xoj::assistant
