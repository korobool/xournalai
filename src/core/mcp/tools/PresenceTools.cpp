// Tool: show_message

#include <gtk/gtk.h>

#include "control/Control.h"            // for Control
#include "control/zoom/ZoomControl.h"   // for ZoomControl
#include "gui/MainWindow.h"             // for MainWindow
#include "gui/PageView.h"               // for XojPageView
#include "gui/XournalView.h"            // for XournalView
#include "gui/widgets/XournalWidget.h"  // for GtkXournal
#include "mcp/McpServer.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"

namespace xoj::mcp::tools {

void registerPresenceTools(McpServer& server) {
    Control* ctrl = server.getControl();

    ToolSpec msg;
    msg.name = "show_message";
    msg.title = "Show a message on the canvas";
    msg.description =
            "Shows a short message to the user as a callout on the canvas, pointing at a spot of a page (x, y in "
            "page points; default: the top of the visible area), for 'seconds'. Use it to comment on what the user "
            "is drawing without leaving the app. The page is scrolled into view if needed.";
    msg.inputSchema = schema::object({{"text", schema::string("Message (short; use \\n for line breaks)")},
                                      {"page", schema::integer("Page (1-based); default: current page")},
                                      {"x", schema::number("Anchor x in page points")},
                                      {"y", schema::number("Anchor y in page points")},
                                      {"seconds", schema::withDefault(schema::number("How long it stays"), 6)}},
                                     {"text"});
    msg.tier = Tier::Ui;
    msg.handler = [ctrl](const json& j) {
        requireDocument(ctrl);
        Args args(j);
        args.rejectUnknown({"text", "page", "x", "y", "seconds"});
        const size_t page = resolvePageIndex(ctrl, args);
        XournalView* xv = ctrl->getWindow()->getXournal();
        GtkWidget* widget = xv->getWidget();
        GtkXournal* xw = GTK_XOURNAL(widget);
        if (ctrl->getCurrentPageNo() != page) {
            xv->scrollTo(page);
        }
        XojPageView* view = xv->getViewFor(page);
        if (!view) {
            throw ToolError("The page is not available");
        }
        const double zoom = ctrl->getZoomControl()->getZoom();
        const auto pos = view->getPixelPosition();
        const double hx = gtk_adjustment_get_value(xw->hadjustment), vy = gtk_adjustment_get_value(xw->vadjustment);
        GdkRectangle target;
        if (args.has("x") && args.has("y")) {
            target.x = static_cast<int>(pos.x + args.number("x") * zoom - hx);
            target.y = static_cast<int>(pos.y + args.number("y") * zoom - vy);
        } else {
            target.x = gtk_widget_get_allocated_width(widget) / 2;
            target.y = 40;
        }
        target.width = target.height = 1;

        GtkWidget* popover = gtk_popover_new(widget);
        gtk_popover_set_modal(GTK_POPOVER(popover), FALSE);
        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &target);
        gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_TOP);
        GtkWidget* label = gtk_label_new(nullptr);
        gchar* markup = g_markup_printf_escaped("<b>AI</b>  %s", args.str("text").c_str());
        gtk_label_set_markup(GTK_LABEL(label), markup);
        g_free(markup);
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(label), 50);
        g_object_set(label, "margin", 10, nullptr);
        gtk_container_add(GTK_CONTAINER(popover), label);
        gtk_widget_show_all(popover);
        gtk_popover_popup(GTK_POPOVER(popover));
        // Close and destroy after the timeout (the popover may be closed earlier by the user)
        g_object_ref(popover);
        const auto ms = static_cast<guint>(args.number("seconds", 6, 0.5, 120) * 1000);
        g_timeout_add(
                ms,
                [](gpointer p) -> gboolean {
                    auto* pop = GTK_WIDGET(p);
                    gtk_widget_destroy(pop);
                    g_object_unref(pop);
                    return G_SOURCE_REMOVE;
                },
                popover);
        return ToolResult::structured({{"shown", true}, {"page", page + 1}});
    };
    server.getRegistry().addTool(std::move(msg));
}

}  // namespace xoj::mcp::tools
