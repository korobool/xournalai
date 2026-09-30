#include "AiToolbar.h"

namespace xoj::assistant {

namespace {
struct Button {
    const char* kind;
    const char* label;
    const char* icon;
    const char* tip;
};

constexpr Button BUTTONS[] = {
        {"improve", "*! Improve", "document-edit-symbolic",
         "Improve the strokes of the selection (or the last thing you drew): same object, pose and size, drawn better"},
        {"illustrate", "**! Illustrate", "applications-graphics-symbolic",
         "A professional illustration of the selected object"},
        {"web", "*w! Web", "web-browser-symbolic", "Search the web for it and write a short summary next to it"},
        {"image", "*r! Image", "image-x-generic-symbolic", "A real image (a photo or a generated one) of it"},
        {"command", "*c! Command…", "mail-send-symbolic",
         "Type an instruction for the AI about the selection (or the page)"},
        {"revise", "Revise page", "view-refresh-symbolic", "A careful full pass over the whole page"},
};
}  // namespace

const char* AiToolbar::marker(const std::string& kind) {
    if (kind == "improve") {
        return "*!";
    }
    if (kind == "illustrate") {
        return "**!";
    }
    if (kind == "web") {
        return "*w!";
    }
    if (kind == "image") {
        return "*r!";
    }
    if (kind == "command") {
        return "*c!";
    }
    return "";
}

AiToolbar::AiToolbar(GtkWidget* mainBox, int position, ActionHandler h): handler(std::move(h)) {
    toolbar = gtk_toolbar_new();
    gtk_buildable_set_name(GTK_BUILDABLE(toolbar), "aiToolbar");
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);

    GtkToolItem* title = gtk_tool_item_new();
    GtkWidget* label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(label), "<b>AI</b>");
    g_object_set(label, "margin-start", 6, "margin-end", 6, nullptr);
    gtk_container_add(GTK_CONTAINER(title), label);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), title, -1);

    // Ask: the AI lasso. Circle an area with the pen or the mouse, then say (hold the pen button) or type what you want
    GtkToolItem* askItem = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(askItem), "Ask");
    gtk_tool_button_set_icon_widget(
            GTK_TOOL_BUTTON(askItem),
            gtk_image_new_from_icon_name("audio-input-microphone-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_tool_item_set_is_important(askItem, TRUE);
    gtk_tool_item_set_tooltip_text(
            askItem, "Ask (Ctrl+Alt+A): circle an area with the pen or the mouse, then say (hold the "
                     "pen button) or type what you want. Quicker: hold the pen button, speak and circle.");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(askItem), "win.ai-ask");
    gtk_buildable_set_name(GTK_BUILDABLE(askItem), "aiAsk");
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), askItem, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

    for (const auto& b: BUTTONS) {
        GtkToolItem* item =
                gtk_tool_button_new(gtk_image_new_from_icon_name(b.icon, GTK_ICON_SIZE_SMALL_TOOLBAR), b.label);
        gtk_tool_item_set_is_important(item, TRUE);  // show the label next to the icon
        gtk_tool_item_set_tooltip_text(item, b.tip);
        gtk_buildable_set_name(GTK_BUILDABLE(item), (std::string("aiAct-") + b.kind).c_str());
        g_object_set_data(G_OBJECT(item), "ai-kind", const_cast<char*>(b.kind));
        g_signal_connect(item, "clicked", G_CALLBACK(+[](GtkToolButton* btn, gpointer d) {
                             auto* self = static_cast<AiToolbar*>(d);
                             const std::string kind =
                                     static_cast<const char*>(g_object_get_data(G_OBJECT(btn), "ai-kind"));
                             if (kind == "command" && self->commandOpener) {
                                 self->commandOpener();
                             } else if (kind == "command") {
                                 self->promptCommand();
                             } else if (self->handler) {
                                 self->handler(kind, "");
                             }
                         }),
                         this);
        if (std::string(b.kind) == "command") {
            commandItem = item;
        }
        if (std::string(b.kind) == "revise") {
            gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
        }
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    }

    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
    auto actionItem = [&](GtkToolItem* item, const char* action, const char* icon, const char* tip, const char* name) {
        gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(item),
                                        gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_SMALL_TOOLBAR));
        gtk_tool_item_set_is_important(item, TRUE);
        gtk_tool_item_set_tooltip_text(item, tip);
        gtk_actionable_set_action_name(GTK_ACTIONABLE(item), action);
        gtk_buildable_set_name(GTK_BUILDABLE(item), name);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), item, -1);
    };
    GtkToolItem* autoItem = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(autoItem), "Auto-improve");
    actionItem(autoItem, "win.ai-auto-improve", "starred-symbolic",
               "On: the AI improves everything you write. Off: only markers and commands.", "aiAutoImprove");
    // the rules of Auto-improve, right next to it
    {
        GMenu* rules = g_menu_new();
        g_menu_append(rules, "Formulas → LaTeX", "win.ai-rule-formulas");
        g_menu_append(rules, "Text in my handwriting", "win.ai-rule-text");
        g_menu_append(rules, "Diagrams redrawn", "win.ai-rule-diagrams");
        g_menu_append(rules, "Consistent colours", "win.ai-rule-colours");
        GtkWidget* mb = gtk_menu_button_new();
        gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(mb), G_MENU_MODEL(rules));
        g_object_unref(rules);
        gtk_button_set_relief(GTK_BUTTON(mb), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text(mb, "What Auto-improve does");
        gtk_widget_set_can_focus(mb, FALSE);
        gtk_buildable_set_name(GTK_BUILDABLE(mb), "aiRulesMenu");
        GtkToolItem* holder = gtk_tool_item_new();
        gtk_container_add(GTK_CONTAINER(holder), mb);
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), holder, -1);
    }
    GtkToolItem* stop = gtk_tool_button_new(nullptr, "Stop");
    actionItem(stop, "win.ai-stop", "process-stop-symbolic", "Stop what the AI is doing now (it stays on)", "aiStop");
    GtkToolItem* pause = gtk_toggle_tool_button_new();
    gtk_tool_button_set_label(GTK_TOOL_BUTTON(pause), "Pause");
    actionItem(pause, "win.mcp-paused", "media-playback-pause-symbolic", "Stop the AI at once (Ctrl+Alt+Esc)",
               "aiPause");
    GtkToolItem* term = gtk_tool_button_new(nullptr, "Terminal");
    actionItem(term, "win.ai-terminal", "utilities-terminal-symbolic", "Show or hide the AI terminal (Ctrl+`)",
               "aiTerminal");

    gtk_box_pack_start(GTK_BOX(mainBox), toolbar, FALSE, FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(mainBox), toolbar, position);
    gtk_widget_show_all(toolbar);
    g_object_add_weak_pointer(G_OBJECT(toolbar), reinterpret_cast<gpointer*>(&toolbar));
}

AiToolbar::~AiToolbar() {
    if (toolbar) {
        g_object_remove_weak_pointer(G_OBJECT(toolbar), reinterpret_cast<gpointer*>(&toolbar));
        gtk_widget_destroy(toolbar);
    }
}

void AiToolbar::setVisible(bool visible) {
    if (toolbar) {
        gtk_widget_set_visible(toolbar, visible);
    }
}

bool AiToolbar::isVisible() const { return toolbar && gtk_widget_get_visible(toolbar); }

void AiToolbar::promptCommand() {
    if (!toolbar || !commandItem) {
        return;
    }
    GtkWidget* popover = gtk_popover_new(GTK_WIDGET(commandItem));
    gtk_buildable_set_name(GTK_BUILDABLE(popover), "aiCommandPopover");
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_set(box, "margin", 8, nullptr);
    GtkWidget* entry = gtk_entry_new();
    gtk_buildable_set_name(GTK_BUILDABLE(entry), "aiCommandEntry");
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 48);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "What should the AI do with the selection (or the page)?");
    gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(popover), box);
    g_signal_connect(entry, "activate", G_CALLBACK(+[](GtkEntry* e, gpointer d) {
                         auto* self = static_cast<AiToolbar*>(d);
                         const std::string text = gtk_entry_get_text(e);
                         GtkWidget* pop = gtk_widget_get_ancestor(GTK_WIDGET(e), GTK_TYPE_POPOVER);
                         if (pop) {
                             gtk_widget_destroy(pop);
                         }
                         if (!text.empty() && self->handler) {
                             self->handler("command", text);
                         }
                     }),
                     this);
    g_signal_connect(popover, "closed", G_CALLBACK(+[](GtkPopover* p, gpointer) {
                         // destroyed after closing (idle: not inside its own signal)
                         g_idle_add(
                                 [](gpointer w) -> gboolean {
                                     if (GTK_IS_WIDGET(w)) {
                                         gtk_widget_destroy(GTK_WIDGET(w));
                                     }
                                     g_object_unref(w);
                                     return G_SOURCE_REMOVE;
                                 },
                                 g_object_ref(p));
                     }),
                     nullptr);
    gtk_widget_show_all(popover);
    gtk_popover_popup(GTK_POPOVER(popover));
    gtk_widget_grab_focus(entry);
}

}  // namespace xoj::assistant
