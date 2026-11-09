// Redraw tracer (diagnostics only; 2026-11-09, how 1.17.4 found its whole-window repaints).
// LD_PRELOAD it into a test app (never the owner's): it logs, with a short backtrace, every call that makes GTK redraw
// or relayout (queue_draw/resize, a label's or button's text, style classes, tooltips, visibility), frame-clock
// requests, tick callbacks, XCreatePixmap and every paint (gdk_window_begin_draw_frame: the window and its region).
//   gcc -shared -fPIC -O1 $(pkg-config --cflags gtk+-3.0 x11) redraw-trace.c -o redraw-trace.so -ldl \
//       $(pkg-config --libs gtk+-3.0 x11)
//   xoai.App(env={"LD_PRELOAD": ".../redraw-trace.so", "XOAI_REDRAW_LOG": ".../redraw.log"})
// Lines: "<ms> <what> [<detail>] <frames>"; xournalpp+0x… frames resolve with addr2line -f -C -e <binary>.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

static FILE* out;
static void logit(const char* what, const char* detail) {
    if (!out) {
        const char* p = getenv("XOAI_REDRAW_LOG");
        if (!p) return;
        out = fopen(p, "w");
        if (!out) return;
    }
    void* bt[14];
    int n = backtrace(bt, 14);
    fprintf(out, "%ld %s [%s]", (long)(g_get_monotonic_time() / 1000), what, detail ? detail : "");
    for (int i = 2; i < n; i++) {
        Dl_info info;
        if (dladdr(bt[i], &info) && info.dli_fname) {
            char buf[512];
            strncpy(buf, info.dli_fname, sizeof buf - 1);
            buf[sizeof buf - 1] = 0;
            if (info.dli_sname && strncmp(basename(buf), "xournalpp", 9) != 0)
                fprintf(out, " %s:%s", basename(buf), info.dli_sname);
            else
                fprintf(out, " %s+0x%lx", basename(buf), (unsigned long)((char*)bt[i] - (char*)info.dli_fbase));
        }
    }
    fputc('\n', out);
    fflush(out);
}
#define REAL(name, type) static __typeof__(&name) real_##name; if (!real_##name) real_##name = (__typeof__(&name))dlsym(RTLD_NEXT, #name)

void gtk_widget_queue_draw(GtkWidget* w) {
    REAL(gtk_widget_queue_draw, void (*)(GtkWidget*));
    logit("queue_draw", G_OBJECT_TYPE_NAME(w));
    real_gtk_widget_queue_draw(w);
}
void gtk_widget_queue_draw_area(GtkWidget* w, int x, int y, int ww, int hh) {
    REAL(gtk_widget_queue_draw_area, void (*)(GtkWidget*, int, int, int, int));
    char d[96];
    snprintf(d, sizeof d, "%s %dx%d", G_OBJECT_TYPE_NAME(w), ww, hh);
    logit("queue_draw_area", d);
    real_gtk_widget_queue_draw_area(w, x, y, ww, hh);
}
void gtk_widget_queue_resize(GtkWidget* w) {
    REAL(gtk_widget_queue_resize, void (*)(GtkWidget*));
    logit("queue_resize", G_OBJECT_TYPE_NAME(w));
    real_gtk_widget_queue_resize(w);
}
void gtk_label_set_text(GtkLabel* l, const char* s) {
    REAL(gtk_label_set_text, void (*)(GtkLabel*, const char*));
    const char* old = gtk_label_get_text(l);
    logit(old && s && strcmp(old, s) == 0 ? "label_set_text SAME" : "label_set_text", s);
    real_gtk_label_set_text(l, s);
}
void gtk_label_set_markup(GtkLabel* l, const char* s) {
    REAL(gtk_label_set_markup, void (*)(GtkLabel*, const char*));
    logit("label_set_markup", s);
    real_gtk_label_set_markup(l, s);
}
void gtk_button_set_label(GtkButton* b, const char* s) {
    REAL(gtk_button_set_label, void (*)(GtkButton*, const char*));
    const char* old = gtk_button_get_label(b);
    logit(old && s && strcmp(old, s) == 0 ? "button_set_label SAME" : "button_set_label", s);
    real_gtk_button_set_label(b, s);
}
void gtk_widget_set_tooltip_text(GtkWidget* w, const char* s) {
    REAL(gtk_widget_set_tooltip_text, void (*)(GtkWidget*, const char*));
    gchar* old = gtk_widget_get_tooltip_text(w);
    logit(old && s && strcmp(old, s) == 0 ? "tooltip SAME" : "tooltip", G_OBJECT_TYPE_NAME(w));
    g_free(old);
    real_gtk_widget_set_tooltip_text(w, s);
}
void gtk_style_context_add_class(GtkStyleContext* c, const char* cls) {
    REAL(gtk_style_context_add_class, void (*)(GtkStyleContext*, const char*));
    logit(gtk_style_context_has_class(c, cls) ? "add_class SAME" : "add_class", cls);
    real_gtk_style_context_add_class(c, cls);
}
void gtk_style_context_remove_class(GtkStyleContext* c, const char* cls) {
    REAL(gtk_style_context_remove_class, void (*)(GtkStyleContext*, const char*));
    logit(!gtk_style_context_has_class(c, cls) ? "remove_class SAME" : "remove_class", cls);
    real_gtk_style_context_remove_class(c, cls);
}
void gtk_widget_set_visible(GtkWidget* w, gboolean v) {
    REAL(gtk_widget_set_visible, void (*)(GtkWidget*, gboolean));
    logit(gtk_widget_get_visible(w) == !!v ? "set_visible SAME" : "set_visible", G_OBJECT_TYPE_NAME(w));
    real_gtk_widget_set_visible(w, v);
}
void gtk_widget_set_sensitive(GtkWidget* w, gboolean v) {
    REAL(gtk_widget_set_sensitive, void (*)(GtkWidget*, gboolean));
    logit(gtk_widget_get_sensitive(w) == !!v ? "set_sensitive SAME" : "set_sensitive", G_OBJECT_TYPE_NAME(w));
    real_gtk_widget_set_sensitive(w, v);
}
void gdk_window_invalidate_rect(GdkWindow* w, const GdkRectangle* r, gboolean c) {
    REAL(gdk_window_invalidate_rect, void (*)(GdkWindow*, const GdkRectangle*, gboolean));
    char d[64];
    snprintf(d, sizeof d, "%dx%d", r ? r->width : -1, r ? r->height : -1);
    logit("window_invalidate_rect", d);
    real_gdk_window_invalidate_rect(w, r, c);
}
void gdk_window_invalidate_region(GdkWindow* w, const cairo_region_t* r, gboolean c) {
    REAL(gdk_window_invalidate_region, void (*)(GdkWindow*, const cairo_region_t*, gboolean));
    cairo_rectangle_int_t e;
    cairo_region_get_extents(r, &e);
    char d[64];
    snprintf(d, sizeof d, "%dx%d", e.width, e.height);
    logit("window_invalidate_region", d);
    real_gdk_window_invalidate_region(w, r, c);
}

void gdk_frame_clock_request_phase(GdkFrameClock* c, GdkFrameClockPhase p) {
    REAL(gdk_frame_clock_request_phase, void (*)(GdkFrameClock*, GdkFrameClockPhase));
    char d[32];
    snprintf(d, sizeof d, "phase %d", (int)p);
    logit("frame_request", d);
    real_gdk_frame_clock_request_phase(c, p);
}
void gdk_frame_clock_begin_updating(GdkFrameClock* c) {
    REAL(gdk_frame_clock_begin_updating, void (*)(GdkFrameClock*));
    logit("frame_begin_updating", "");
    real_gdk_frame_clock_begin_updating(c);
}
void gdk_frame_clock_end_updating(GdkFrameClock* c) {
    REAL(gdk_frame_clock_end_updating, void (*)(GdkFrameClock*));
    logit("frame_end_updating", "");
    real_gdk_frame_clock_end_updating(c);
}
guint gtk_widget_add_tick_callback(GtkWidget* w, GtkTickCallback cb, gpointer d, GDestroyNotify n) {
    REAL(gtk_widget_add_tick_callback, guint (*)(GtkWidget*, GtkTickCallback, gpointer, GDestroyNotify));
    logit("add_tick_callback", G_OBJECT_TYPE_NAME(w));
    return real_gtk_widget_add_tick_callback(w, cb, d, n);
}
#include <X11/Xlib.h>
Pixmap XCreatePixmap(Display* d, Drawable w, unsigned int width, unsigned int height, unsigned int depth) {
    REAL(XCreatePixmap, Pixmap (*)(Display*, Drawable, unsigned int, unsigned int, unsigned int));
    char det[48];
    snprintf(det, sizeof det, "%ux%u d%u", width, height, depth);
    logit("XCreatePixmap", det);
    return real_XCreatePixmap(d, w, width, height, depth);
}
GdkDrawingContext* gdk_window_begin_draw_frame(GdkWindow* w, const cairo_region_t* r) {
    REAL(gdk_window_begin_draw_frame, GdkDrawingContext* (*)(GdkWindow*, const cairo_region_t*));
    gpointer ud = NULL;
    gdk_window_get_user_data(w, &ud);
    cairo_rectangle_int_t e = {0, 0, 0, 0};
    if (r) cairo_region_get_extents(r, &e);
    char det[160];
    snprintf(det, sizeof det, "%s %dx%d+%d+%d", ud ? G_OBJECT_TYPE_NAME(ud) : "?", e.width, e.height, e.x, e.y);
    logit("PAINT", det);
    return real_gdk_window_begin_draw_frame(w, r);
}
