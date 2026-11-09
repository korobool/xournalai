#include "AskPopover.h"

#include <utility>  // for move

namespace xoj::assistant {

const std::vector<AskPopover::Command>& AskPopover::commands() {
    static const std::vector<Command> list = {
            {"improve", "edit-clear-all-symbolic", "Improve: tidy it up (handwriting, formulas, layout)", "Improve"},
            {"illustrate", "insert-image-symbolic", "Illustrate: add a sketch or diagram", "Illustrate"},
            {"write", "document-edit-symbolic", "Write: continue or write it out", "Write"},
            {"revise", "emblem-ok-symbolic", "Revise: check and correct", "Revise"},
            {"style", "applications-graphics-symbolic", "Style: colours and emphasis", "Style"},
            {"explain", "dialog-information-symbolic", "Explain: add an explanation next to it", "Explain"},
            {"summarize", "view-list-symbolic", "Summarize: sum it up", "Summarize"},
    };
    return list;
}

AskPopover::AskPopover(GtkWidget* canvas, Submit submit, Mic mic): onSubmit(std::move(submit)), onMic(std::move(mic)) {
    popover = gtk_popover_new(canvas);
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "ask-popover");
    gtk_popover_set_modal(GTK_POPOVER(popover), false);  // the pen keeps drawing elsewhere
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_RIGHT);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover*, gpointer d) {
                         auto* self = static_cast<AskPopover*>(d);
                         if (self->onClosed) {
                             self->onClosed();
                         }
                     }),
                     this);
    // Big tiles like the Tools grid's, for a finger or a stylus
    static GtkCssProvider* css = [] {
        GtkCssProvider* p = gtk_css_provider_new();
        gtk_css_provider_load_from_data(
                p,
                ".xoai-ask .xoai-ask-title { font-size: 0.8em; font-weight: bold; opacity: 0.6; }"
                ".xoai-ask entry { font-size: 16px; min-height: 52px; border-radius: 12px; }"
                ".xoai-ask .xoai-cmd { min-width: 88px; min-height: 78px; padding: 4px; border-radius: 12px; }"
                ".xoai-ask .xoai-cmd label { font-size: 0.92em; }"
                ".xoai-ask .xoai-big { min-width: 76px; min-height: 64px; padding: 2px 4px; border-radius: 12px; }"
                ".xoai-ask .xoai-big label { font-size: 0.85em; }"
                ".xoai-ask .xoai-ask-close { min-width: 52px; min-height: 52px; border-radius: 14px; }",
                -1, nullptr);
        return p;
    }();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_style_context_add_class(gtk_widget_get_style_context(popover), "xoai-ask");

    auto tile = [](const char* id, const char* icon, int px, const char* label, const char* tip) {
        GtkWidget* b = gtk_button_new();
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        GtkWidget* image = gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_DND);
        gtk_image_set_pixel_size(GTK_IMAGE(image), px);
        gtk_box_pack_start(GTK_BOX(inner), image, true, true, 0);
        gtk_box_pack_start(GTK_BOX(inner), gtk_label_new(label), false, false, 0);
        gtk_container_add(GTK_CONTAINER(b), inner);
        gtk_widget_set_can_focus(b, false);  // (Escape and typing stay with the text)
        gtk_buildable_set_name(GTK_BUILDABLE(b), id);
        gtk_widget_set_tooltip_text(b, tip);
        return b;
    };

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    GtkWidget* head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* titleLabel = gtk_label_new("WHAT SHOULD AI DO HERE?");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "xoai-ask-title");
    gtk_box_pack_start(GTK_BOX(head), titleLabel, true, true, 0);
    GtkWidget* closeButton = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_buildable_set_name(GTK_BUILDABLE(closeButton), "ask-close");
    gtk_widget_set_tooltip_text(closeButton, "Close (Esc)");
    gtk_button_set_relief(GTK_BUTTON(closeButton), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(closeButton, false);
    gtk_style_context_add_class(gtk_widget_get_style_context(closeButton), "xoai-ask-close");
    g_signal_connect_swapped(closeButton, "clicked",
                             G_CALLBACK(+[](gpointer self) { static_cast<AskPopover*>(self)->close(); }), this);
    gtk_box_pack_end(GTK_BOX(head), closeButton, false, false, 0);
    gtk_box_pack_start(GTK_BOX(box), head, false, false, 0);

    // The commands: one tap each, four to a row
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), true);
    int i = 0;
    for (const auto& c: commands()) {
        GtkWidget* b = tile((std::string("ask-") + c.id).c_str(), c.icon, 32, c.name, c.label);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "xoai-cmd");
        g_object_set_data(G_OBJECT(b), "ask-command", const_cast<char*>(c.id));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer self) {
                             static_cast<AskPopover*>(self)->submit(
                                     static_cast<const char*>(g_object_get_data(G_OBJECT(b), "ask-command")));
                         }),
                         this);
        gtk_grid_attach(GTK_GRID(grid), b, i % 4, i / 4, 1, 1);
        i++;
    }
    gtk_box_pack_start(GTK_BOX(box), grid, false, false, 0);

    // Or in words: said (hold the mic or the pen button) or typed
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget* micButton = tile("ask-mic", "audio-input-microphone-symbolic", 30, "Hold to say", "Hold to talk");
    gtk_style_context_add_class(gtk_widget_get_style_context(micButton), "xoai-big");
    g_signal_connect(micButton, "button-press-event", G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer self) -> gboolean {
                         static_cast<AskPopover*>(self)->onMic(true);
                         return false;
                     }),
                     this);
    g_signal_connect(micButton, "button-release-event",
                     G_CALLBACK(+[](GtkWidget*, GdkEvent*, gpointer self) -> gboolean {
                         static_cast<AskPopover*>(self)->onMic(false);
                         return false;
                     }),
                     this);
    gtk_box_pack_start(GTK_BOX(row), micButton, false, false, 0);

    entry = gtk_entry_new();
    gtk_buildable_set_name(GTK_BUILDABLE(entry), "ask-text");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Or say / type what you want");
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 24);
    gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(+[](GtkEntry*, gpointer self) { static_cast<AskPopover*>(self)->submit(""); }), this);
    gtk_box_pack_start(GTK_BOX(row), entry, true, true, 0);

    GtkWidget* send = tile("ask-send", "mail-send-symbolic", 30, "Send", "Send (Enter)");
    gtk_style_context_add_class(gtk_widget_get_style_context(send), "xoai-big");
    gtk_style_context_add_class(gtk_widget_get_style_context(send), "suggested-action");
    g_signal_connect_swapped(send, "clicked",
                             G_CALLBACK(+[](gpointer self) { static_cast<AskPopover*>(self)->submit(""); }), this);
    gtk_box_pack_start(GTK_BOX(row), send, false, false, 0);
    gtk_box_pack_start(GTK_BOX(box), row, false, false, 0);

    status = gtk_label_new("");
    gtk_buildable_set_name(GTK_BUILDABLE(status), "ask-status");
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "dim-label");
    gtk_box_pack_start(GTK_BOX(box), status, false, false, 0);

    gtk_container_add(GTK_CONTAINER(popover), box);
    gtk_widget_show_all(box);
    gtk_widget_hide(status);
    g_object_ref(popover);  // kept while this object lives
}

AskPopover::~AskPopover() {
    if (popover) {
        gtk_widget_destroy(popover);
        g_object_unref(popover);
    }
}

void AskPopover::show(const GdkRectangle& rect, const std::string& text) {
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_entry_set_text(GTK_ENTRY(entry), text.c_str());
    setStatus("");
    gtk_popover_popup(GTK_POPOVER(popover));
    gtk_widget_grab_focus(entry);
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

void AskPopover::close() { gtk_popover_popdown(GTK_POPOVER(popover)); }

bool AskPopover::visible() const { return gtk_widget_get_visible(popover); }

std::string AskPopover::text() const { return gtk_entry_get_text(GTK_ENTRY(entry)); }

void AskPopover::appendText(const std::string& more) {
    std::string t = text();
    if (!t.empty() && t.back() != ' ') {
        t += ' ';
    }
    t += more;
    gtk_entry_set_text(GTK_ENTRY(entry), t.c_str());
    gtk_editable_set_position(GTK_EDITABLE(entry), -1);
}

void AskPopover::setStatus(const std::string& s) {
    gtk_label_set_text(GTK_LABEL(status), s.c_str());
    gtk_widget_set_visible(status, !s.empty());
}

void AskPopover::submit(const std::string& command) {
    const std::string t = text();
    if (command.empty() && t.empty()) {
        setStatus("Say or type what you want, or tap a command");
        return;
    }
    close();
    if (onSubmit) {
        onSubmit(command, t);
    }
}

}  // namespace xoj::assistant
