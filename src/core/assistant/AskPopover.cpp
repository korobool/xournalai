#include "AskPopover.h"

#include <utility>  // for move

namespace xoj::assistant {

const std::vector<AskPopover::Command>& AskPopover::commands() {
    static const std::vector<Command> list = {
            {"improve", "edit-clear-all-symbolic", "Improve: tidy it up (handwriting, formulas, layout)"},
            {"illustrate", "insert-image-symbolic", "Illustrate: add a sketch or diagram"},
            {"write", "document-edit-symbolic", "Write: continue or write it out"},
            {"revise", "emblem-ok-symbolic", "Revise: check and correct"},
            {"style", "applications-graphics-symbolic", "Style: colours and emphasis"},
            {"explain", "dialog-information-symbolic", "Explain: add an explanation next to it"},
            {"summarize", "view-list-symbolic", "Summarize: sum it up"},
    };
    return list;
}

AskPopover::AskPopover(GtkWidget* canvas, Submit submit, Mic mic): onSubmit(std::move(submit)), onMic(std::move(mic)) {
    popover = gtk_popover_new(canvas);
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "ask-popover");
    gtk_popover_set_modal(GTK_POPOVER(popover), false);  // the pen keeps drawing elsewhere
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_RIGHT);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box), 6);

    GtkWidget* icons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    for (const auto& c: commands()) {
        GtkWidget* b = gtk_button_new_from_icon_name(c.icon, GTK_ICON_SIZE_BUTTON);
        gtk_buildable_set_name(GTK_BUILDABLE(b), (std::string("ask-") + c.id).c_str());
        gtk_widget_set_tooltip_text(b, c.label);
        gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
        g_object_set_data(G_OBJECT(b), "ask-command", const_cast<char*>(c.id));
        g_signal_connect(b, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer self) {
                             static_cast<AskPopover*>(self)->submit(
                                     static_cast<const char*>(g_object_get_data(G_OBJECT(b), "ask-command")));
                         }),
                         this);
        gtk_box_pack_start(GTK_BOX(icons), b, false, false, 0);
    }
    GtkWidget* closeButton = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_buildable_set_name(GTK_BUILDABLE(closeButton), "ask-close");
    gtk_widget_set_tooltip_text(closeButton, "Close (Esc)");
    gtk_button_set_relief(GTK_BUTTON(closeButton), GTK_RELIEF_NONE);
    g_signal_connect_swapped(closeButton, "clicked",
                             G_CALLBACK(+[](gpointer self) { static_cast<AskPopover*>(self)->close(); }), this);
    gtk_box_pack_end(GTK_BOX(icons), closeButton, false, false, 0);
    gtk_box_pack_start(GTK_BOX(box), icons, false, false, 0);

    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    entry = gtk_entry_new();
    gtk_buildable_set_name(GTK_BUILDABLE(entry), "ask-text");
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Say (hold the mic or the pen button) or type what you want");
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 44);
    g_signal_connect(entry, "activate",
                     G_CALLBACK(+[](GtkEntry*, gpointer self) { static_cast<AskPopover*>(self)->submit(""); }), this);
    gtk_box_pack_start(GTK_BOX(row), entry, true, true, 0);

    GtkWidget* micButton = gtk_button_new_from_icon_name("audio-input-microphone-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_buildable_set_name(GTK_BUILDABLE(micButton), "ask-mic");
    gtk_widget_set_tooltip_text(micButton, "Hold to talk");
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

    GtkWidget* send = gtk_button_new_from_icon_name("mail-send-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_buildable_set_name(GTK_BUILDABLE(send), "ask-send");
    gtk_widget_set_tooltip_text(send, "Send (Enter)");
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
