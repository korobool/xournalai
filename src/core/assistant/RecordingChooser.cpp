#include "RecordingChooser.h"

#include <algorithm>  // for max
#include <utility>    // for move

namespace xoj::assistant {

namespace {
struct Option {
    RecordingChooser::Choice choice;
    const char* id;
    const char* icon;
    const char* label;
    const char* hint;
    const char* tooltip;
};

const Option OPTIONS[] = {
        {RecordingChooser::Choice::Instructions, "recording-instructions", "mail-send-symbolic", "Instructions",
         "AI does what I said", "Transcribe it and do what it asks (about this page)"},
        {RecordingChooser::Choice::Notes, "recording-notes", "document-edit-symbolic", "Notes", "transcript to keep",
         "Transcribe it into notes for later, aligned with the ink. What is said in it is never taken as "
         "instructions (your request above is)"},
        {RecordingChooser::Choice::Keep, "recording-keep", "audio-x-generic-symbolic", "Keep audio", "nothing else",
         "Keep only the recording; nothing is transcribed or sent"},
};

struct Chip {
    const char* id;
    const char* label;
    const char* phrase;  ///< added to the request
};
const Chip CHIPS[] = {
        {"recording-chip-summary", "Summarize", "Summarize it"},
        {"recording-chip-points", "Key points", "List the key points"},
        {"recording-chip-actions", "Action items", "List the action items"},
        {"recording-chip-flashcards", "Flashcards", "Make flashcards"},
        {"recording-chip-translate", "Translate…", "Translate into "},
};
}  // namespace

const char* RecordingChooser::name(Choice c) {
    switch (c) {
        case Choice::Instructions:
            return "instructions";
        case Choice::Notes:
            return "notes";
        case Choice::Keep:
            return "keep";
    }
    return "?";
}

RecordingChooser::RecordingChooser(GtkWidget* canvas, Chosen chosen): canvas(canvas), onChosen(std::move(chosen)) {
    popover = gtk_popover_new(canvas);
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "recording-chooser");
    gtk_popover_set_modal(GTK_POPOVER(popover), false);  // stays until a choice is made; the pen keeps working
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_TOP);
    static GtkCssProvider* css = [] {
        GtkCssProvider* p = gtk_css_provider_new();
        gtk_css_provider_load_from_data(p,
                                        ".xoai-rec .xoai-choice { min-width: 132px; min-height: 84px; padding: 4px; }"
                                        ".xoai-rec .xoai-choice .xoai-choice-name { font-size: 15px; "
                                        "  font-weight: bold; }"
                                        ".xoai-rec .xoai-choice .xoai-choice-hint { font-size: 11px; }"
                                        ".xoai-rec .xoai-rec-title { font-size: 15px; font-weight: bold; }"
                                        ".xoai-rec entry { font-size: 15px; min-height: 40px; }"
                                        ".xoai-rec .xoai-big { min-width: 52px; min-height: 44px; }"
                                        ".xoai-rec .xoai-chip { min-height: 36px; padding: 0 10px; }",
                                        -1, nullptr);
        return p;
    }();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_style_context_add_class(gtk_widget_get_style_context(popover), "xoai-rec");

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);

    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    title = gtk_label_new("");
    gtk_buildable_set_name(GTK_BUILDABLE(title), "recording-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "xoai-rec-title");
    details = gtk_label_new("");
    gtk_buildable_set_name(GTK_BUILDABLE(details), "recording-details");
    gtk_label_set_xalign(GTK_LABEL(details), 0);
    gtk_label_set_ellipsize(GTK_LABEL(details), PANGO_ELLIPSIZE_MIDDLE);
    gtk_style_context_add_class(gtk_widget_get_style_context(details), "dim-label");
    gtk_box_pack_start(GTK_BOX(texts), title, false, false, 0);
    gtk_box_pack_start(GTK_BOX(texts), details, false, false, 0);
    gtk_box_pack_start(GTK_BOX(head), texts, true, true, 0);
    GtkWidget* closeButton = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_widget_set_valign(closeButton, GTK_ALIGN_START);
    gtk_buildable_set_name(GTK_BUILDABLE(closeButton), "recording-close");
    gtk_widget_set_tooltip_text(closeButton, "Keep only the audio");
    gtk_button_set_relief(GTK_BUTTON(closeButton), GTK_RELIEF_NONE);
    g_signal_connect_swapped(
            closeButton, "clicked",
            G_CALLBACK(+[](gpointer self) { static_cast<RecordingChooser*>(self)->choose(Choice::Keep); }), this);
    gtk_box_pack_end(GTK_BOX(head), closeButton, false, false, 0);
    gtk_box_pack_start(GTK_BOX(box), head, false, false, 0);

    // What the AI should do: typed, dictated (hold the mic or the pen button) or picked from the chips
    GtkWidget* ask = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    request = gtk_entry_new();
    gtk_buildable_set_name(GTK_BUILDABLE(request), "recording-request");
    gtk_entry_set_placeholder_text(GTK_ENTRY(request),
                                   "What should AI do? (optional: type, or hold the mic or the pen button)");
    gtk_entry_set_width_chars(GTK_ENTRY(request), 44);
    g_signal_connect(request, "activate", G_CALLBACK(+[](GtkEntry*, gpointer self) {
                         static_cast<RecordingChooser*>(self)->choose(Choice::Notes);  // Enter: notes, as asked
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(ask), request, true, true, 0);
    GtkWidget* mic = gtk_button_new_from_icon_name("audio-input-microphone-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(mic), "xoai-big");
    gtk_buildable_set_name(GTK_BUILDABLE(mic), "recording-mic");
    gtk_widget_set_tooltip_text(mic, "Hold to talk (English)");
    g_signal_connect(mic, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer self) -> gboolean {
                         auto* s = static_cast<RecordingChooser*>(self);
                         if (s->onMic) {
                             s->onMic(true);
                         }
                         return false;
                     }),
                     this);
    g_signal_connect(mic, "button-release-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer self) -> gboolean {
                         auto* s = static_cast<RecordingChooser*>(self);
                         if (s->onMic) {
                             s->onMic(false);
                         }
                         return false;
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(ask), mic, false, false, 0);
    GtkWidget* send = gtk_button_new_from_icon_name("mail-send-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(send), "xoai-big");
    gtk_buildable_set_name(GTK_BUILDABLE(send), "recording-send");
    gtk_widget_set_tooltip_text(send, "Notes, with this request (Enter)");
    g_signal_connect_swapped(
            send, "clicked",
            G_CALLBACK(+[](gpointer self) { static_cast<RecordingChooser*>(self)->choose(Choice::Notes); }), this);
    gtk_box_pack_start(GTK_BOX(ask), send, false, false, 0);
    gtk_box_pack_start(GTK_BOX(box), ask, false, false, 0);

    GtkWidget* chips = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    for (const auto& [id, label, phrase]: CHIPS) {
        GtkWidget* chip = gtk_button_new_with_label(label);
        gtk_style_context_add_class(gtk_widget_get_style_context(chip), "xoai-chip");
        gtk_buildable_set_name(GTK_BUILDABLE(chip), id);
        g_object_set_data(G_OBJECT(chip), "chip-phrase", const_cast<char*>(phrase));
        g_signal_connect(chip, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer self) {
                             static_cast<RecordingChooser*>(self)->addPhrase(
                                     static_cast<const char*>(g_object_get_data(G_OBJECT(b), "chip-phrase")));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(chips), chip, false, false, 0);
    }
    gtk_box_pack_start(GTK_BOX(box), chips, false, false, 0);

    status = gtk_label_new("");
    gtk_buildable_set_name(GTK_BUILDABLE(status), "recording-status");
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "dim-label");
    gtk_box_pack_start(GTK_BOX(box), status, false, false, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(row), true);
    for (const auto& o: OPTIONS) {
        GtkWidget* b = gtk_button_new();
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_box_pack_start(GTK_BOX(inner), gtk_image_new_from_icon_name(o.icon, GTK_ICON_SIZE_DND), false, false, 0);
        GtkWidget* n = gtk_label_new(o.label);
        gtk_style_context_add_class(gtk_widget_get_style_context(n), "xoai-choice-name");
        gtk_box_pack_start(GTK_BOX(inner), n, false, false, 0);
        GtkWidget* h = gtk_label_new(o.hint);
        gtk_style_context_add_class(gtk_widget_get_style_context(h), "xoai-choice-hint");
        gtk_style_context_add_class(gtk_widget_get_style_context(h), "dim-label");
        gtk_box_pack_start(GTK_BOX(inner), h, false, false, 0);
        gtk_container_add(GTK_CONTAINER(b), inner);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-choice");
        if (o.choice == Choice::Instructions) {
            gtk_style_context_add_class(gtk_widget_get_style_context(b), "suggested-action");
        }
        gtk_buildable_set_name(GTK_BUILDABLE(b), o.id);
        gtk_widget_set_tooltip_text(b, o.tooltip);
        g_object_set_data(G_OBJECT(b), "recording-choice", GINT_TO_POINTER(static_cast<int>(o.choice)));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer self) {
                             static_cast<RecordingChooser*>(self)->choose(static_cast<Choice>(
                                     GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "recording-choice"))));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(row), b, true, true, 0);
    }
    gtk_box_pack_start(GTK_BOX(box), row, false, false, 0);

    gtk_container_add(GTK_CONTAINER(popover), box);
    gtk_widget_show_all(box);
    gtk_widget_hide(status);
    g_object_ref(popover);  // kept while this object lives
}

void RecordingChooser::setMic(Mic mic) { onMic = std::move(mic); }

void RecordingChooser::addPhrase(const std::string& phrase) {
    std::string t = gtk_entry_get_text(GTK_ENTRY(request));
    while (!t.empty() && (t.back() == ' ' || t.back() == ';')) {
        t.pop_back();
    }
    t += (t.empty() ? "" : "; ") + phrase;
    gtk_entry_set_text(GTK_ENTRY(request), t.c_str());
    gtk_widget_grab_focus(request);
    gtk_editable_set_position(GTK_EDITABLE(request), -1);  // (e.g. "Translate into " waits for the language)
}

void RecordingChooser::appendText(const std::string& more) {
    if (more.empty()) {
        return;
    }
    std::string t = gtk_entry_get_text(GTK_ENTRY(request));
    t += (t.empty() || t.back() == ' ' ? "" : " ") + more;
    gtk_entry_set_text(GTK_ENTRY(request), t.c_str());
    gtk_editable_set_position(GTK_EDITABLE(request), -1);
}

void RecordingChooser::setStatus(const std::string& text) {
    gtk_label_set_text(GTK_LABEL(status), text.c_str());
    gtk_widget_set_visible(status, !text.empty());
}

RecordingChooser::~RecordingChooser() {
    if (popover) {
        gtk_widget_destroy(popover);
        g_object_unref(popover);
    }
}

bool RecordingChooser::visible() const { return gtk_widget_get_visible(popover); }

void RecordingChooser::offer(Recording r) {
    queue.push_back(std::move(r));
    showFront(queue.size() == 1);  // (else only the count of waiting ones changes)
}

void RecordingChooser::showFront(bool fresh) {
    if (queue.empty()) {
        gtk_popover_popdown(GTK_POPOVER(popover));
        return;
    }
    const Recording& r = queue.front();
    const int64_t s = std::max<int64_t>(0, r.durationMs / 1000);
    gchar* t = g_strdup_printf("Recording %lld:%02lld stopped: what is it?", static_cast<long long>(s / 60),
                               static_cast<long long>(s % 60));
    gtk_label_set_text(GTK_LABEL(title), t);
    g_free(t);
    std::string d = "page " + std::to_string(r.page + 1) + ", " + std::to_string(r.strokes) +
                    " stroke(s) written meanwhile  ·  " + r.name;
    if (queue.size() > 1) {
        d += "  ·  " + std::to_string(queue.size() - 1) + " more waiting";
    }
    gtk_label_set_text(GTK_LABEL(details), d.c_str());
    if (!fresh) {
        return;
    }
    gtk_entry_set_text(GTK_ENTRY(request), "");
    setStatus("");
    // Above the status line, in the middle of the canvas
    const GdkRectangle at{gtk_widget_get_allocated_width(canvas) / 2, gtk_widget_get_allocated_height(canvas) - 8, 1,
                          1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &at);
    gtk_popover_popup(GTK_POPOVER(popover));
}

void RecordingChooser::choose(Choice c) {
    if (queue.empty()) {
        return;
    }
    const Recording r = std::move(queue.front());
    queue.pop_front();
    const std::string n = gtk_entry_get_text(GTK_ENTRY(request));
    showFront(true);
    if (onChosen) {
        onChosen(r, c, n);
    }
}

}  // namespace xoj::assistant
