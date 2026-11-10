#include "QuestionPanel.h"

#include <algorithm>  // for find_if
#include <cstdio>     // for snprintf

#include "gui/DrawnText.h"  // for drawnTextSet

namespace xoj::assistant {

namespace {
const char* CSS = ".xoai-questions { padding: 6px; }"
                  ".xoai-questions-strip, .xoai-questions-strip viewport { background-color: @theme_bg_color; }"
                  ".xoai-questions-card { padding: 10px; border-radius: 16px; background-color: alpha(#1c1e23, 0.94);"
                  "  box-shadow: 0 4px 16px alpha(#000000, 0.45); }"
                  ".xoai-questions-card label, .xoai-questions-card .xoai-drawn-text { color: #ffffff; }"
                  ".xoai-question-who { font-size: 0.85em; font-weight: bold; opacity: 0.75; }"
                  ".xoai-question-text { font-size: 1.15em; font-weight: bold; }"
                  ".xoai-question-idle { opacity: 0.55; }"
                  ".xoai-question-left { font-size: 0.85em; opacity: 0.7; }"
                  ".xoai-question-locate { min-height: 34px; border-radius: 12px; font-weight: bold; }"
                  ".xoai-question-canvas { font-size: 0.85em; opacity: 0.8; }"
                  ".xoai-question-option { min-height: 46px; border-radius: 14px; font-size: 1.1em; font-weight: bold;"
                  "  color: #ffffff; border: none; background-image: linear-gradient(160deg, #4a90e2, #2d6fe0);"
                  "  box-shadow: 0 2px 6px alpha(#2d6fe0, 0.45), inset 0 1px alpha(#ffffff, 0.3); }"
                  ".xoai-question-option label { color: #ffffff; }"
                  ".xoai-question-option.xoai-question-decline { background-image: linear-gradient(160deg, #8a8f99,"
                  "  #5f646e); box-shadow: 0 2px 6px alpha(#000000, 0.3), inset 0 1px alpha(#ffffff, 0.25); }"
                  ".xoai-question-yes, .xoai-question-no { min-height: 54px; border-radius: 16px; font-size: 1.25em;"
                  "  font-weight: bold; color: #ffffff; border: none; }"
                  ".xoai-question-yes { background-image: linear-gradient(160deg, #3fc37a, #1e9a5a);"
                  "  box-shadow: 0 2px 8px alpha(#1e9a5a, 0.5), inset 0 1px alpha(#ffffff, 0.3); }"
                  ".xoai-question-no { background-image: linear-gradient(160deg, #ff7a6a, #d8453c);"
                  "  box-shadow: 0 2px 8px alpha(#d8453c, 0.5), inset 0 1px alpha(#ffffff, 0.3); }"
                  ".xoai-question-yes label, .xoai-question-no label { color: #ffffff; }"
                  ".xoai-question-yes:disabled, .xoai-question-no:disabled { background-image: none;"
                  "  background-color: alpha(#808080, 0.25); box-shadow: none; }"
                  ".xoai-question-yes:disabled label, .xoai-question-no:disabled label { color: alpha(#ffffff, 0.35); }";

void addCss() {
    static bool added = false;
    if (added) {
        return;
    }
    added = true;
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CSS, -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

GtkWidget* label(const char* cls, bool wrap) {
    GtkWidget* l = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    if (wrap) {
        gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(l), 24);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(l), cls);
    return l;
}
}  // namespace

QuestionPanel::QuestionPanel(Answered onAnswer): onAnswer(std::move(onAnswer)) {}

std::string QuestionPanel::markOf(int id) {
    static const char* const circled[] = {"①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧", "⑨", "⑩",
                                          "⑪", "⑫", "⑬", "⑭", "⑮", "⑯", "⑰", "⑱", "⑲", "⑳"};
    return circled[(std::max(id, 1) - 1) % 20];
}

uint32_t QuestionPanel::colourOf(int id) {
    static const uint32_t colours[] = {0x2d6fe0, 0xe0732d, 0x1e9a5a, 0xb04fd8, 0xd8453c, 0x13806f};
    return colours[(std::max(id, 1) - 1) % 6];
}

void QuestionPanel::setCanvasOn(bool on) {
    canvasOn = on;
    if (dock.canvasSwitch) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dock.canvasSwitch), on);
    }
}

QuestionPanel::~QuestionPanel() {
    if (timer) {
        g_source_remove(timer);
    }
    // (questions still open: their agents hear that nobody will answer)
    const auto open = queue;
    queue.clear();
    for (const auto& q: open) {
        if (onAnswer) {
            onAnswer(q.id, q.fallback, "cancelled");
        }
    }
    // its views go with it (their buttons call it)
    for (View* v: {&dock, &floating}) {
        if (v->root) {
            GtkWidget* root = v->root;
            g_object_remove_weak_pointer(G_OBJECT(root), reinterpret_cast<gpointer*>(&v->root));
            v->root = nullptr;
            gtk_widget_destroy(root);
        }
    }
}

int QuestionPanel::ask(Question q) {
    q.id = nextId++;
    queue.push_back(q);
    if (!timer) {
        timer = g_timeout_add_seconds(
                1,
                +[](gpointer d) -> gboolean {
                    static_cast<QuestionPanel*>(d)->tick();
                    return static_cast<QuestionPanel*>(d)->timer ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
                },
                this);
    }
    refresh();
    return q.id;
}

bool QuestionPanel::cancel(int id, const std::string& why) {
    auto it = std::find_if(queue.begin(), queue.end(), [id](const Question& q) { return q.id == id; });
    if (it == queue.end()) {
        return false;
    }
    finish(id, it->fallback, why);
    return true;
}

void QuestionPanel::answer(int id, const std::string& value) { finish(id, value, "owner"); }

void QuestionPanel::finish(int id, const std::string& value, const std::string& by) {
    auto it = std::find_if(queue.begin(), queue.end(), [id](const Question& q) { return q.id == id; });
    if (it == queue.end()) {
        return;
    }
    queue.erase(it);
    if (queue.empty() && timer) {
        g_source_remove(timer);
        timer = 0;
    }
    refresh();
    if (onAnswer) {
        onAnswer(id, value, by);
    }
}

void QuestionPanel::tick() {
    const gint64 now = g_get_monotonic_time();
    // (the one shown is the first; a later one whose time is up is answered with its default too)
    std::vector<std::pair<int, std::string>> due;
    for (const auto& q: queue) {
        if (q.deadlineUs && now >= q.deadlineUs) {
            due.emplace_back(q.id, q.fallback);
        }
    }
    for (const auto& [id, value]: due) {
        finish(id, value, "timeout");
    }
    for (View* v: {&dock, &floating}) {
        if (v->root && v->left && !queue.empty()) {
            const auto& q = queue.front();
            const gint64 s = q.deadlineUs ? std::max<gint64>(0, (q.deadlineUs - now) / G_USEC_PER_SEC) : -1;
            std::string fallback = q.fallback == "no" ? "No" : q.fallback == "yes" ? "Yes" : q.fallback;
            xoj::gui::drawnTextSet(v->left, s >= 0 ? "in " + std::to_string(s) + " s: " + fallback : "");
        }
    }
}

GtkWidget* QuestionPanel::dockView() {
    if (!dock.root) {
        build(dock, false);
    }
    return dock.root;
}

GtkWidget* QuestionPanel::floatView() {
    if (!floating.root) {
        build(floating, true);
    }
    return floating.root;
}

void QuestionPanel::setDockShown(bool shown) {
    if (shown != dockShown) {
        dockShown = shown;
        refresh();
    }
}

GtkWidget* QuestionPanel::build(View& v, bool isFloating) {
    addCss();
    const std::string prefix = isFloating ? "aiQuestionCard" : "aiQuestion";
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    auto* ctx = gtk_widget_get_style_context(box);
    gtk_style_context_add_class(ctx, "xoai-questions");
    v.who = label("xoai-question-who", false);
    gtk_buildable_set_name(GTK_BUILDABLE(v.who), (prefix + "Who").c_str());
    v.text = label("xoai-question-text", true);
    gtk_buildable_set_name(GTK_BUILDABLE(v.text), (prefix + "Text").c_str());
    v.options = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    v.left = xoj::gui::drawnTextNew("");  // (a countdown: no re-layout each second)
    gtk_style_context_add_class(gtk_widget_get_style_context(v.left), "xoai-question-left");
    gtk_buildable_set_name(GTK_BUILDABLE(v.left), (prefix + "Left").c_str());
    gtk_box_pack_start(GTK_BOX(box), v.who, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), v.text, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), v.options, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), v.left, FALSE, FALSE, 0);
    if (!isFloating) {
        // A question about something on the page shows there too (a bubble); this turns that off
        v.canvasSwitch = gtk_check_button_new_with_label("Questions on the canvas too");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v.canvasSwitch), canvasOn);
        gtk_widget_set_can_focus(v.canvasSwitch, FALSE);
        gtk_style_context_add_class(gtk_widget_get_style_context(v.canvasSwitch), "xoai-question-canvas");
        gtk_buildable_set_name(GTK_BUILDABLE(v.canvasSwitch), "aiQuestionsOnCanvas");
        g_signal_connect(v.canvasSwitch, "toggled", G_CALLBACK(+[](GtkToggleButton* b, gpointer d) {
                             auto* self = static_cast<QuestionPanel*>(d);
                             self->canvasOn = gtk_toggle_button_get_active(b);
                             if (self->onCanvasToggle) {
                                 self->onCanvasToggle(self->canvasOn);
                             }
                         }),
                         this);
        gtk_box_pack_end(GTK_BOX(box), v.canvasSwitch, FALSE, FALSE, 0);
    }
    if (isFloating) {
        v.root = box;
        gtk_style_context_add_class(ctx, "xoai-questions-card");
        gtk_widget_set_size_request(v.root, 280, -1);
    } else {
        // In the terminal: the whole strip scrolls when the terminal is low (it never spills over the tabs); in a
        // terminal of normal height all of it shows, Да / Нет at the bottom
        v.root = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(v.root), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(v.root), 0);
        gtk_style_context_add_class(gtk_widget_get_style_context(v.root), "xoai-questions-strip");
        gtk_container_add(GTK_CONTAINER(v.root), box);
        // (resized with the terminal: all of it repainted, its own window too: else the old size's rest stays black)
        g_signal_connect(v.root, "size-allocate", G_CALLBACK(+[](GtkWidget* w, GdkRectangle*, gpointer) {
                             gtk_widget_queue_draw(w);
                             if (GtkWidget* child = gtk_bin_get_child(GTK_BIN(w))) {
                                 gtk_widget_queue_draw(child);
                             }
                         }),
                         nullptr);
    }
    gtk_buildable_set_name(GTK_BUILDABLE(v.root), isFloating ? "aiQuestionCard" : "aiQuestionPanel");
    g_object_add_weak_pointer(G_OBJECT(v.root), reinterpret_cast<gpointer*>(&v.root));
    if (!isFloating) {
        // the terminal shown or not (by any of its buttons): the card on the canvas follows
        g_signal_connect(v.root, "map", G_CALLBACK(+[](GtkWidget*, gpointer d) {
                             static_cast<QuestionPanel*>(d)->setDockShown(true);
                         }),
                         this);
        g_signal_connect(v.root, "unmap", G_CALLBACK(+[](GtkWidget*, gpointer d) {
                             static_cast<QuestionPanel*>(d)->setDockShown(false);
                         }),
                         this);
    }
    gtk_widget_show_all(v.root);
    refresh(v);
    return v.root;
}

void QuestionPanel::refresh() {
    refresh(dock);
    refresh(floating);
    tick();
}

void QuestionPanel::refresh(View& v) {
    if (!v.root) {
        return;
    }
    if (&v == &floating) {
        gtk_widget_set_visible(v.root, !queue.empty() && !dockShown);
    }
    const int front = queue.empty() ? 0 : queue.front().id;
    // "② Claude asks · 1/2": its number in its colour, as on the canvas
    auto setWho = [&v](const Question& q, size_t count) {
        const std::string plain =
                markOf(q.id) + " " + q.who + " asks" + (count > 1 ? " · 1/" + std::to_string(count) : "");
        if (plain == gtk_label_get_text(GTK_LABEL(v.who))) {
            return;  // (set only when it changed: a label re-lays out)
        }
        char colour[8];
        std::snprintf(colour, sizeof colour, "#%06x", colourOf(q.id));
        gchar* rest = g_markup_escape_text(plain.substr(markOf(q.id).size()).c_str(), -1);
        const std::string markup =
                std::string("<span foreground=\"") + colour + "\" size=\"larger\">" + markOf(q.id) + "</span>" + rest;
        g_free(rest);
        gtk_label_set_markup(GTK_LABEL(v.who), markup.c_str());
    };
    if (front == v.shownId) {
        if (!queue.empty()) {  // (only the counter may have changed)
            setWho(queue.front(), queue.size());
        }
        return;  // (the same question: its buttons stay, a tap on them is never lost)
    }
    v.shownId = front;
    if (GTK_IS_SCROLLED_WINDOW(v.root)) {  // (a new question is read from its top)
        gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(v.root)), 0);
    }
    GList* kids = gtk_container_get_children(GTK_CONTAINER(v.options));
    for (GList* k = kids; k; k = k->next) {
        gtk_widget_destroy(GTK_WIDGET(k->data));
    }
    g_list_free(kids);
    auto* textCtx = gtk_widget_get_style_context(v.text);
    if (queue.empty()) {
        gtk_label_set_text(GTK_LABEL(v.who), "❓ Agents' questions");  // (plain again)
        gtk_label_set_text(GTK_LABEL(v.text), "No questions");
        gtk_style_context_add_class(textCtx, "xoai-question-idle");
        xoj::gui::drawnTextSet(v.left, "");
        return;
    }
    gtk_style_context_remove_class(textCtx, "xoai-question-idle");
    const auto& q = queue.front();
    setWho(q, queue.size());
    gtk_label_set_text(GTK_LABEL(v.text), q.text.c_str());
    if (!q.place.empty()) {
        // «↗ page 3»: what it is about (on the canvas too, outlined in its colour)
        GtkWidget* locate = gtk_button_new_with_label(("↗ " + q.place).c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(locate), "xoai-question-locate");
        gtk_widget_set_can_focus(locate, FALSE);
        gtk_buildable_set_name(GTK_BUILDABLE(locate), (&v == &floating ? "aiQuestionCardLocate" : "aiQuestionLocate"));
        g_object_set_data(G_OBJECT(locate), "question", GINT_TO_POINTER(q.id));
        g_signal_connect(locate, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             auto* self = static_cast<QuestionPanel*>(d);
                             if (self->onLocate) {
                                 self->onLocate(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "question")));
                             }
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(v.options), locate, FALSE, FALSE, 0);
        gtk_widget_show(locate);
    }
    const std::string prefix = &v == &floating ? "aiQuestionCardOption" : "aiQuestionOption";
    // only the asker's buttons (the owner, 2026-11-09: a panel of dynamic buttons); a yes/no question: Да, Нет
    std::vector<Option> options = q.options;
    if (options.empty()) {
        options = {{"Yes", "yes"}, {"No", "no"}};
    }
    for (size_t i = 0; i < options.size(); i++) {
        GtkWidget* b = gtk_button_new_with_label(options[i].label.c_str());
        gtk_label_set_line_wrap(GTK_LABEL(gtk_bin_get_child(GTK_BIN(b))), TRUE);
        gtk_buildable_set_name(GTK_BUILDABLE(b), (prefix + std::to_string(i + 1)).c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-question-option");
        gtk_widget_set_can_focus(b, FALSE);
        g_object_set_data_full(G_OBJECT(b), "value", g_strdup(options[i].value.c_str()), g_free);
        if (options[i].value == "no" || options[i].label == "No" || options[i].label == "Not now" ||
            options[i].label == "Нет" || options[i].label == "Не надо") {
            gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-question-decline");
        }
        g_object_set_data(G_OBJECT(b), "question", GINT_TO_POINTER(q.id));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer d) {
                             const int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "question"));
                             const std::string value = static_cast<const char*>(g_object_get_data(G_OBJECT(b), "value"));
                             // (after the click's own handlers: the button goes with the refresh)
                             auto* self = static_cast<QuestionPanel*>(d);
                             self->answer(id, value);
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(v.options), b, FALSE, FALSE, 0);
        gtk_widget_show(b);
    }
}

}  // namespace xoj::assistant
