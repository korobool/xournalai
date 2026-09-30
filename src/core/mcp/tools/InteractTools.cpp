// Tools: ui_interact, ui_keys, ui_file_chooser, ui_wait_for_window

#include <algorithm>  // for transform

#include <gtk/gtk.h>

#include "api/AgentGate.h"     // for AgentGate
#include "api/UiAutomation.h"  // for ui
#include "control/Control.h"   // for Control
#include "gui/MainWindow.h"    // for MainWindow
#include "mcp/McpServer.h"
#include "mcp/PathText.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"
#include "UiCommon.h"

namespace xoj::mcp::tools {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

void click(GtkWidget* w) {
    if (GTK_IS_BUTTON(w)) {
        gtk_button_clicked(GTK_BUTTON(w));
    } else if (GTK_IS_MENU_ITEM(w)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(w));
    } else if (GTK_IS_TOOL_BUTTON(w)) {
        // Click its inner button, like the user: toggles (and actionable tool buttons) react to that one
        GtkWidget* inner = gtk_bin_get_child(GTK_BIN(w));
        if (inner && GTK_IS_BUTTON(inner)) {
            gtk_button_clicked(GTK_BUTTON(inner));
        } else {
            g_signal_emit_by_name(w, "clicked");
        }
    } else if (GTK_IS_LIST_BOX_ROW(w)) {
        gtk_widget_activate(w);
    } else if (!gtk_widget_activate(w)) {
        throw ToolError(std::string("This widget (") + G_OBJECT_TYPE_NAME(w) + ") cannot be clicked");
    }
}

bool setComboByText(GtkComboBox* combo, const std::string& text) {
    GtkTreeModel* model = gtk_combo_box_get_model(combo);
    if (!model) {
        return false;
    }
    int column = GTK_IS_COMBO_BOX_TEXT(combo) ? 0 : gtk_combo_box_get_entry_text_column(combo);
    if (column < 0) {
        column = 0;
    }
    if (gtk_tree_model_get_column_type(model, column) != G_TYPE_STRING) {
        return false;
    }
    GtkTreeIter it;
    int index = 0;
    for (bool ok = gtk_tree_model_get_iter_first(model, &it); ok; ok = gtk_tree_model_iter_next(model, &it), index++) {
        gchar* s = nullptr;
        gtk_tree_model_get(model, &it, column, &s, -1);
        const bool match = s && lower(s) == lower(text);
        g_free(s);
        if (match) {
            gtk_combo_box_set_active(combo, index);
            return true;
        }
    }
    return false;
}

void setValue(GtkWidget* w, const json& v) {
    auto text = [&] { return v.is_string() ? v.get<std::string>() : v.dump(); };
    if (GTK_IS_SPIN_BUTTON(w)) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), Args::toNumber(v, "value"));
        gtk_spin_button_update(GTK_SPIN_BUTTON(w));
    } else if (GTK_IS_ENTRY(w)) {
        gtk_entry_set_text(GTK_ENTRY(w), text().c_str());
    } else if (GTK_IS_TOGGLE_BUTTON(w)) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), Args::toBool(v, "value"));
    } else if (GTK_IS_TOGGLE_TOOL_BUTTON(w)) {
        gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(w), Args::toBool(v, "value"));
    } else if (GTK_IS_CHECK_MENU_ITEM(w)) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), Args::toBool(v, "value"));
    } else if (GTK_IS_SWITCH(w)) {
        gtk_switch_set_active(GTK_SWITCH(w), Args::toBool(v, "value"));
    } else if (GTK_IS_RANGE(w)) {
        gtk_range_set_value(GTK_RANGE(w), Args::toNumber(v, "value"));
    } else if (GTK_IS_COMBO_BOX(w)) {
        if (v.is_number_integer()) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(w), v.get<int>());
        } else if (!setComboByText(GTK_COMBO_BOX(w), text())) {
            throw ToolError("The combo box has no entry '" + text() + "' (or give its index as a number)");
        }
    } else if (GTK_IS_COLOR_CHOOSER(w)) {
        GdkRGBA c;
        if (!gdk_rgba_parse(&c, text().c_str())) {
            throw ToolError("Invalid color '" + text() + "'");
        }
        gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(w), &c);
        if (GTK_IS_COLOR_BUTTON(w)) {
            g_signal_emit_by_name(w, "color-set");
        }
    } else if (GTK_IS_FONT_CHOOSER(w)) {
        gtk_font_chooser_set_font(GTK_FONT_CHOOSER(w), text().c_str());
        if (GTK_IS_FONT_BUTTON(w)) {
            g_signal_emit_by_name(w, "font-set");
        }
    } else if (GTK_IS_TEXT_VIEW(w)) {
        gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(w)), text().c_str(), -1);
    } else if (GTK_IS_NOTEBOOK(w)) {
        auto* nb = GTK_NOTEBOOK(w);
        if (v.is_number_integer()) {
            gtk_notebook_set_current_page(nb, v.get<int>());
            return;
        }
        for (int i = 0; i < gtk_notebook_get_n_pages(nb); i++) {
            const char* label = gtk_notebook_get_tab_label_text(nb, gtk_notebook_get_nth_page(nb, i));
            if (label && lower(label) == lower(text())) {
                gtk_notebook_set_current_page(nb, i);
                return;
            }
        }
        throw ToolError("No tab '" + text() + "'");
    } else {
        throw ToolError(std::string("Cannot set a value on ") + G_OBJECT_TYPE_NAME(w));
    }
}

void selectRow(GtkWidget* w, const json& v) {
    if (GTK_IS_TREE_VIEW(w)) {
        auto* tv = GTK_TREE_VIEW(w);
        GtkTreeModel* model = gtk_tree_view_get_model(tv);
        GtkTreeIter it;
        int index = 0;
        for (bool ok = gtk_tree_model_get_iter_first(model, &it); ok;
             ok = gtk_tree_model_iter_next(model, &it), index++) {
            bool match = v.is_number_integer() && v.get<int>() == index;
            for (int c = 0; !match && v.is_string() && c < gtk_tree_model_get_n_columns(model); c++) {
                if (gtk_tree_model_get_column_type(model, c) == G_TYPE_STRING) {
                    gchar* s = nullptr;
                    gtk_tree_model_get(model, &it, c, &s, -1);
                    match = s && lower(s) == lower(v.get<std::string>());
                    g_free(s);
                }
            }
            if (match) {
                GtkTreePath* path = gtk_tree_model_get_path(model, &it);
                gtk_tree_view_set_cursor(tv, path, nullptr, FALSE);
                gtk_tree_view_row_activated(tv, path, gtk_tree_view_get_column(tv, 0));
                gtk_tree_path_free(path);
                return;
            }
        }
        throw ToolError("No row " + v.dump());
    }
    if (GTK_IS_LIST_BOX(w)) {
        if (!v.is_number_integer()) {
            throw ToolError("For list boxes give the row index");
        }
        GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(w), v.get<int>());
        if (!row) {
            throw ToolError("No row " + v.dump());
        }
        gtk_list_box_select_row(GTK_LIST_BOX(w), row);
        gtk_widget_activate(GTK_WIDGET(row));
        return;
    }
    throw ToolError(std::string("select_row needs a list (GtkTreeView/GtkListBox), not ") + G_OBJECT_TYPE_NAME(w));
}

/// Sends a key press+release to the focused window (for keys that are not action shortcuts)
void sendKey(GtkWidget* window, guint keyval, GdkModifierType mods) {
    GdkWindow* gdkWin = gtk_widget_get_window(window);
    if (!gdkWin) {
        throw ToolError("The window is not realized");
    }
    GdkKeymapKey* keys = nullptr;
    gint nKeys = 0;
    guint16 hw = 0;
    if (gdk_keymap_get_entries_for_keyval(gdk_keymap_get_for_display(gdk_display_get_default()), keyval, &keys,
                                          &nKeys) &&
        nKeys > 0) {
        hw = static_cast<guint16>(keys[0].keycode);
    }
    g_free(keys);
    GdkDevice* kbd = gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_display_get_default()));
    for (GdkEventType type: {GDK_KEY_PRESS, GDK_KEY_RELEASE}) {
        GdkEvent* ev = gdk_event_new(type);
        ev->key.window = GDK_WINDOW(g_object_ref(gdkWin));
        ev->key.send_event = TRUE;
        ev->key.time = GDK_CURRENT_TIME;
        ev->key.state = mods;
        ev->key.keyval = keyval;
        ev->key.hardware_keycode = hw;
        if (kbd) {
            gdk_event_set_device(ev, kbd);
        }
        gtk_main_do_event(ev);
        gdk_event_free(ev);
    }
}

GtkWidget* focusedWindow(Control* ctrl) {
    auto wins = api::ui::windows(ctrl->getWindow()->getWindow());
    for (const auto& w: wins) {
        if (w.focused) {
            refuseUserOnly(w.window);
            return w.window;
        }
    }
    GtkWidget* w = wins.empty() ? ctrl->getWindow()->getWindow() : wins.front().window;
    refuseUserOnly(w);
    return w;
}

}  // namespace

void registerInteractTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec interact;
    interact.name = "ui_interact";
    interact.title = "Operate a widget";
    interact.description =
            "Operates a widget found with ui_inspect: click (buttons, menu items, toolbar buttons), set_value "
            "(entry text, spin/scale number, check/toggle/switch true/false, combo item text or index, color "
            "\"#rrggbb\", font \"Sans 12\", tab label or index, text view text), select_row (list row index or text), "
            "activate (like pressing Enter on it), focus, close (a window: like its close button).";
    interact.inputSchema = schema::object(
            {{"op",
              schema::enumeration("Operation", {"click", "set_value", "select_row", "activate", "focus", "close"})},
             {"target", schema::string("Widget or window id from ui_inspect / ui_windows")},
             {"value", schema::string("set_value/select_row: the value (text, number or true/false)")}},
            {"op", "target"});
    interact.tier = Tier::Ui;
    interact.asyncHandler = [ctrl, srv = &server](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "target", "value"});
        const std::string op =
                args.choice("op", {"click", "set_value", "select_row", "activate", "focus", "close"}, "");
        GtkWidget* w = resolveWidget(ctrl, args, "target", false);
        if (op != "close" && op != "focus" && !gtk_widget_is_sensitive(w)) {
            throw ToolError("The widget is disabled");
        }
        if (op == "click") {
            static const std::vector<std::string> destructive = {
                    "discard", "don't save", "do not save", "close without saving", "quit", "discard changes"};
            const std::string label = lower(api::ui::widgetText(w));
            if (std::find(destructive.begin(), destructive.end(), label) != destructive.end()) {
                srv->requireTier(Tier::Destructive, "clicking '" + api::ui::widgetText(w) + "'");
                srv->backup("before-ui-discard");
            }
            click(w);
        } else if (op == "set_value") {
            setValue(w, args.raw("value"));
        } else if (op == "select_row") {
            selectRow(w, args.raw("value"));
        } else if (op == "activate") {
            if (!gtk_widget_activate(w)) {
                if (GTK_IS_ENTRY(w)) {
                    g_signal_emit_by_name(w, "activate");
                } else {
                    click(w);
                }
            }
        } else if (op == "focus") {
            gtk_widget_grab_focus(w);
        } else {
            GtkWidget* top = gtk_widget_get_toplevel(w);
            if (top == ctrl->getWindow()->getWindow()) {
                throw ToolError("Closing the main window would quit; use action_run(\"app.quit\") if you mean it");
            }
            gtk_window_close(GTK_WINDOW(top));
        }
        // Let the UI react (dialogs close, values propagate), then report the windows
        whenReady([] { return false; },
                  [ctrl, respond](bool) {
                      respond(ToolResult::structured({{"done", true}, {"windows", windowsJson(ctrl)}}));
                  },
                  120);
    };
    server.getRegistry().addTool(std::move(interact));

    ToolSpec keys;
    keys.name = "ui_keys";
    keys.title = "Keyboard";
    keys.description =
            "Keyboard input to the focused window: op=shortcut with an accelerator like \"<Ctrl>z\", "
            "\"<Ctrl><Shift>s\", "
            "\"Delete\", \"Escape\", \"Return\", \"Page_Down\" (application shortcuts run their action directly), or "
            "op=type to type text into the focused text field.";
    keys.inputSchema = schema::object({{"op", schema::enumeration("Operation", {"shortcut", "type"})},
                                       {"keys", schema::string("shortcut: GTK accelerator string")},
                                       {"text", schema::string("type: text to type")}},
                                      {"op"});
    keys.tier = Tier::Ui;
    keys.asyncHandler = [ctrl, &server](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"op", "keys", "text"});
        const std::string op = args.choice("op", {"shortcut", "type"}, "");
        GtkWidget* window = focusedWindow(ctrl);
        json out = {{"window", api::ui::idOf(window)}};
        if (op == "type") {
            const std::string text = args.str("text");
            GtkWidget* focus = gtk_window_get_focus(GTK_WINDOW(window));
            if (focus && GTK_IS_EDITABLE(focus)) {
                gint pos = gtk_editable_get_position(GTK_EDITABLE(focus));
                gtk_editable_insert_text(GTK_EDITABLE(focus), text.c_str(), static_cast<gint>(text.size()), &pos);
                gtk_editable_set_position(GTK_EDITABLE(focus), pos);
            } else if (focus && GTK_IS_TEXT_VIEW(focus)) {
                gtk_text_buffer_insert_at_cursor(gtk_text_view_get_buffer(GTK_TEXT_VIEW(focus)), text.c_str(), -1);
            } else {
                for (const char* p = text.c_str(); *p; p = g_utf8_next_char(p)) {
                    sendKey(window, gdk_unicode_to_keyval(g_utf8_get_char(p)), GdkModifierType(0));
                }
            }
            out["typed"] = text;
        } else {
            const std::string accel = args.str("keys");
            guint key = 0;
            GdkModifierType mods = GdkModifierType(0);
            gtk_accelerator_parse(accel.c_str(), &key, &mods);
            if (key == 0) {
                throw ToolError("Cannot parse the shortcut '" + accel + "' (examples: \"<Ctrl>z\", \"Escape\")");
            }
            gchar* normalized = gtk_accelerator_name(key, mods);
            GtkApplication* app = gtk_window_get_application(GTK_WINDOW(ctrl->getWindow()->getWindow()));
            gchar** actions = app ? gtk_application_get_actions_for_accel(app, normalized) : nullptr;
            bool ran = false;
            for (gchar** a = actions; a && *a && !ran; a++) {
                std::string full = *a;
                if (full == "app.quit") {
                    server.requireTier(Tier::Destructive, "quitting the application");
                }
                if (api::AgentGate::userOnlyAction(full)) {
                    throw ToolError("'" + full + "' is for the user only");
                }
                const auto dot = full.find('.');
                GActionGroup* group = full.rfind("app.", 0) == 0 ? G_ACTION_GROUP(app) :
                                                                   G_ACTION_GROUP(ctrl->getWindow()->getWindow());
                gchar* name = nullptr;
                GVariant* target = nullptr;
                if (g_action_parse_detailed_name(full.c_str() + dot + 1, &name, &target, nullptr)) {
                    if (window == ctrl->getWindow()->getWindow() && g_action_group_get_action_enabled(group, name)) {
                        g_action_group_activate_action(group, name, target);
                        out["action"] = full;
                        ran = true;
                    }
                    g_free(name);
                    if (target) {
                        g_variant_unref(target);
                    }
                }
            }
            g_strfreev(actions);
            g_free(normalized);
            if (!ran) {
                sendKey(window, key, mods);
            }
            out["keys"] = accel;
        }
        whenReady([] { return false; }, [out, respond](bool) { respond(ToolResult::structured(out)); }, 100);
    };
    server.getRegistry().addTool(std::move(keys));

    ToolSpec chooser;
    chooser.name = "ui_file_chooser";
    chooser.title = "Fill a file dialog";
    chooser.description =
            "Operates an open file dialog (Open, Save, Export, image or PDF chooser - see ui_windows): sets the "
            "path and confirms it (or cancels). For Save dialogs the file name is set in the name field.";
    chooser.inputSchema =
            schema::object({{"window", schema::string("File dialog id (default: the open file dialog)")},
                            {"path", schema::string("File to choose / save to")},
                            {"accept", schema::withDefault(schema::boolean("Confirm (true) or cancel (false)"), true)}},
                           {});
    chooser.tier = Tier::Files;
    chooser.asyncHandler = [ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"window", "path", "accept"});
        GtkWidget* dlg = nullptr;
        if (args.has("window")) {
            dlg = resolveWidget(ctrl, args, "window", false);
        } else {
            for (const auto& w: api::ui::windows(ctrl->getWindow()->getWindow())) {
                if (w.kind == "file_chooser") {
                    dlg = w.window;
                }
            }
        }
        if (!dlg || !GTK_IS_FILE_CHOOSER(dlg)) {
            throw ToolError("No file dialog is open (open one first, e.g. ui_menu_select(\"File/Open\"))");
        }
        const bool accept = args.boolean("accept", true);
        auto* fc = GTK_FILE_CHOOSER(dlg);
        std::string wantedFile;
        if (accept) {
            const fs::path path = fs::absolute(pathFromUtf8(args.str("path")));
            if (gtk_file_chooser_get_action(fc) == GTK_FILE_CHOOSER_ACTION_SAVE) {
                gtk_file_chooser_set_current_folder(fc, path.parent_path().c_str());
                gtk_file_chooser_set_current_name(fc, path.filename().c_str());
            } else if (!gtk_file_chooser_set_filename(fc, path.c_str())) {
                throw ToolError("The dialog did not accept " + toUtf8(path));
            } else {
                wantedFile = path.string();
            }
        }
        auto press = [ctrl, dlg, accept, respond](bool) {
            // Click the visible confirm/cancel button like a user (the app's dialogs use their own response ids)
            static const std::vector<std::string> acceptLabels = {"open",   "save",     "export", "ok",    "select",
                                                                  "insert", "annotate", "apply",  "choose"};
            static const std::vector<std::string> cancelLabels = {"cancel", "close"};
            const auto& wanted = accept ? acceptLabels : cancelLabels;
            GtkWidget* button = nullptr;
            for (const auto& w: api::ui::inspect(dlg, -1, true)) {
                if (w.role == "button" && w.sensitive &&
                    std::find(wanted.begin(), wanted.end(), lower(w.label)) != wanted.end()) {
                    button = w.widget;  // the last match is the dialog's action button
                }
            }
            if (button) {
                gtk_button_clicked(GTK_BUTTON(button));
            } else if (GTK_IS_DIALOG(dlg)) {
                gtk_dialog_response(GTK_DIALOG(dlg), accept ? GTK_RESPONSE_ACCEPT : GTK_RESPONSE_CANCEL);
            }
            whenReady([] { return false; },
                      [ctrl, respond](bool) {
                          respond(ToolResult::structured({{"done", true}, {"windows", windowsJson(ctrl)}}));
                      },
                      250);
        };
        // Selecting a file happens after the folder has loaded: wait for it before confirming
        whenReady(
                [fc, wantedFile] {
                    if (wantedFile.empty()) {
                        return true;
                    }
                    gchar* f = gtk_file_chooser_get_filename(fc);
                    const bool ok = f && wantedFile == f;
                    g_free(f);
                    return ok;
                },
                press, 3000, 50);
    };
    server.getRegistry().addTool(std::move(chooser));

    ToolSpec wait;
    wait.name = "ui_wait_for_window";
    wait.title = "Wait for a window";
    wait.description = "Waits until a window whose title contains 'title' (or of 'kind': dialog, file_chooser, "
                       "menu) is open, e.g. after an action that opens a dialog. Returns it, or an error on timeout.";
    wait.inputSchema = schema::object(
            {{"title", schema::string("Part of the window title (case-insensitive)")},
             {"kind", schema::enumeration("Window kind", {"dialog", "file_chooser", "menu", "popup", "window"})},
             {"timeout_ms", schema::withDefault(schema::integer("Maximum wait"), 5000)}});
    wait.readOnly = true;
    wait.tier = Tier::Ui;
    wait.asyncHandler = [ctrl](const json& j, Responder respond) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"title", "kind", "timeout_ms"});
        const std::string title = lower(args.str("title", ""));
        const std::string kind = args.str("kind", "");
        if (title.empty() && kind.empty()) {
            throw ToolError("Give 'title' and/or 'kind'");
        }
        auto found = std::make_shared<json>();
        auto match = [ctrl, title, kind, found] {
            for (const auto& w: api::ui::windows(ctrl->getWindow()->getWindow())) {
                if ((kind.empty() || w.kind == kind) &&
                    (title.empty() || lower(w.title).find(title) != std::string::npos) && w.kind != "main") {
                    *found = {{"id", api::ui::idOf(w.window)}, {"title", w.title}, {"kind", w.kind}};
                    return true;
                }
            }
            return false;
        };
        whenReady(
                match,
                [found, respond](bool ok) {
                    respond(ok ? ToolResult::structured({{"window", *found}}) :
                                 ToolResult::error("No such window appeared within the timeout"));
                },
                static_cast<unsigned>(args.integer("timeout_ms", 5000, 0, 120000)), 50);
    };
    server.getRegistry().addTool(std::move(wait));
}

}  // namespace xoj::mcp::tools
