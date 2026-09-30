#include "RecordingIndicator.h"

#include <algorithm>  // for clamp, copy
#include <cmath>      // for sin, sqrt, M_PI
#include <string>     // for string
#include <utility>    // for move

namespace xoj::gui {

namespace {
constexpr int WAVE_WIDTH = 86;
constexpr int WAVE_HEIGHT = 18;
constexpr guint FRAME_MS = 50;

void named(GtkWidget* w, const char* name) {
    gtk_widget_set_name(w, name);
    gtk_buildable_set_name(GTK_BUILDABLE(w), name);
}
}  // namespace

RecordingIndicator::RecordingIndicator(GtkWidget* mainBox, Source src): source(std::move(src)) {
    widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    named(widget, "recordingIndicator");
    g_object_ref_sink(widget);  // it moves between containers
    gtk_widget_set_tooltip_text(widget, "The audio recorder is on");

    wave = gtk_drawing_area_new();
    named(wave, "recordingIndicatorWave");
    gtk_widget_set_size_request(wave, WAVE_WIDTH, WAVE_HEIGHT);
    gtk_widget_set_valign(wave, GTK_ALIGN_CENTER);
    g_signal_connect(wave, "draw", G_CALLBACK(onDraw), this);

    label = gtk_label_new("");
    named(label, "recordingIndicatorLabel");

    GtkWidget* stop = gtk_button_new_with_label("Stop");
    named(stop, "recordingIndicatorStop");
    gtk_widget_set_can_focus(stop, FALSE);
    gtk_button_set_relief(GTK_BUTTON(stop), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(stop, "Stop the audio recording");
    g_signal_connect(stop, "clicked", G_CALLBACK(+[](GtkButton*, gpointer self) {
                         auto* s = static_cast<RecordingIndicator*>(self);
                         if (s->source.stop) {
                             s->source.stop();
                         }
                     }),
                     this);

    gtk_box_pack_start(GTK_BOX(widget), wave, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(widget), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(widget), stop, FALSE, FALSE, 0);
    gtk_widget_show_all(widget);
    gtk_widget_set_no_show_all(widget, TRUE);
    gtk_widget_hide(widget);

    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
                                    "#recordingIndicator { background-color: rgba(220, 38, 38, 0.14);"
                                    "  border-radius: 12px; padding: 0 2px 0 10px; }"
                                    "#recordingIndicatorLabel { color: #dc2626; font-weight: bold; }"
                                    "#recordingIndicatorStop { color: #dc2626; font-weight: bold; padding: 0 8px;"
                                    "  min-height: 0; }",
                                    -1, nullptr);
    gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(widget), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    // Its own place: an empty box takes no room while the indicator is hidden or elsewhere
    slot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    named(slot, "recordingIndicatorSlot");
    g_object_set(slot, "margin-start", 8, "margin-end", 8, nullptr);
    g_object_ref(slot);
    if (mainBox && GTK_IS_BOX(mainBox)) {
        gtk_box_pack_end(GTK_BOX(mainBox), slot, FALSE, FALSE, 0);
    }
    gtk_widget_show(slot);
    gtk_box_pack_start(GTK_BOX(slot), widget, FALSE, FALSE, 1);
}

RecordingIndicator::~RecordingIndicator() {
    if (timer) {
        g_source_remove(timer);
    }
    g_signal_handlers_disconnect_by_data(wave, this);
    if (GtkWidget* parent = gtk_widget_get_parent(widget)) {
        gtk_container_remove(GTK_CONTAINER(parent), widget);
    }
    g_object_unref(widget);
    if (GtkWidget* parent = gtk_widget_get_parent(slot)) {
        gtk_container_remove(GTK_CONTAINER(parent), slot);
    }
    g_object_unref(slot);
}

void RecordingIndicator::setRecording(bool on) {
    if (on == recording) {
        return;
    }
    recording = on;
    if (on) {
        startedUs = g_get_monotonic_time();
        shownSeconds = -1;
        levels.fill(0.0f);
        if (source.level) {
            source.level();  // (forget what came before)
        }
        updateLabel();
        gtk_widget_show(widget);
        timer = g_timeout_add(FRAME_MS, onTick, this);
    } else {
        if (timer) {
            g_source_remove(timer);
            timer = 0;
        }
        gtk_widget_hide(widget);
    }
}

void RecordingIndicator::attachTo(GtkBox* box, int position) {
    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (parent) {
        gtk_container_remove(GTK_CONTAINER(parent), widget);
    }
    gtk_box_pack_start(box, widget, FALSE, FALSE, 0);
    gtk_box_reorder_child(box, widget, position);
}

void RecordingIndicator::detach() {
    if (gtk_widget_get_parent(widget) == slot) {
        return;
    }
    if (GtkWidget* parent = gtk_widget_get_parent(widget)) {
        gtk_container_remove(GTK_CONTAINER(parent), widget);
    }
    gtk_box_pack_start(GTK_BOX(slot), widget, FALSE, FALSE, 1);
}

gboolean RecordingIndicator::onTick(gpointer self) {
    static_cast<RecordingIndicator*>(self)->tick();
    return G_SOURCE_CONTINUE;
}

void RecordingIndicator::tick() {
    const float level = source.level ? source.level() : 0.0f;
    std::copy(levels.begin() + 1, levels.end(), levels.begin());
    levels.back() = std::clamp(std::sqrt(std::max(level, 0.0f)) * 1.3f, 0.0f, 1.0f);  // quiet mics still show
    updateLabel();
    gtk_widget_queue_draw(wave);
}

void RecordingIndicator::updateLabel() {
    const int seconds = static_cast<int>((g_get_monotonic_time() - startedUs) / G_USEC_PER_SEC);
    if (seconds == shownSeconds) {
        return;
    }
    shownSeconds = seconds;
    gchar* text = seconds >= 3600 ?
                          g_strdup_printf("Recording %d:%02d:%02d", seconds / 3600, seconds / 60 % 60, seconds % 60) :
                          g_strdup_printf("Recording %d:%02d", seconds / 60, seconds % 60);
    gtk_label_set_text(GTK_LABEL(label), text);
    g_free(text);
}

gboolean RecordingIndicator::onDraw(GtkWidget* area, cairo_t* cr, gpointer self) {
    auto* s = static_cast<RecordingIndicator*>(self);
    const double h = gtk_widget_get_allocated_height(area);
    const double mid = h / 2;

    // The dot breathes once a second and a half
    const double t = static_cast<double>(g_get_monotonic_time() - s->startedUs) / G_USEC_PER_SEC;
    const double pulse = 0.5 + 0.5 * std::sin(t * 2 * M_PI / 1.5);
    cairo_set_source_rgba(cr, 0.86, 0.15, 0.15, 0.18 + 0.22 * pulse);
    cairo_arc(cr, 7, mid, 5 + 2 * pulse, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.86, 0.15, 0.15);
    cairo_arc(cr, 7, mid, 4.5, 0, 2 * M_PI);
    cairo_fill(cr);

    // The level: newest on the right
    constexpr double x0 = 18;
    constexpr double step = 3.7;
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 2.2);
    for (int i = 0; i < BARS; i++) {
        const double v = s->levels[static_cast<size_t>(i)];
        const double half = std::max(1.0, v * (mid - 2));
        cairo_set_source_rgba(cr, 0.86, 0.15, 0.15, 0.35 + 0.65 * (i + 1.0) / BARS);
        cairo_move_to(cr, x0 + i * step, mid - half);
        cairo_line_to(cr, x0 + i * step, mid + half);
        cairo_stroke(cr);
    }
    return FALSE;
}

}  // namespace xoj::gui
