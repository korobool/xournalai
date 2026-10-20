#include "BlockingJob.h"

#include <glib.h>  // for g_idle_add

#include "control/Control.h"    // for Control
#include "control/jobs/Job.h"   // for JOB_TYPE_BLOCKING, JobType
#include "gui/KeyboardFocus.h"  // for textInputWithKeyboard
#include "gui/MainWindow.h"     // for MainWindow
#include "gui/XournalView.h"    // for XournalView
#include "util/Util.h"          // for execInUiThread
#include "util/glib_casts.h"    // for wrap_for_once_v

BlockingJob::BlockingJob(Control* control, const std::string& name, bool blockUi):
        control(control), blocksUi(blockUi) {
    MainWindow* win = control->getWindow();
    g_weak_ref_init(&keyboardOwner, win && blockUi ? xoj::gui::textInputWithKeyboard(win->getXournal()->getWidget()) :
                                                      nullptr);
    if (blockUi) {
        control->block(name);
    }
}

BlockingJob::~BlockingJob() {
    this->control = nullptr;
    g_weak_ref_clear(&keyboardOwner);
}

void BlockingJob::execute() {
    this->run();
    if (!this->blocksUi) {
        return;
    }

    // (a reference, given up on the UI thread)
    auto* owner = static_cast<GtkWidget*>(g_weak_ref_get(&keyboardOwner));
    Util::execInUiThread([control = this->control, owner]() {
        control->unblock();
        if (MainWindow* win = control->getWindow()) {
            GtkWidget* top = gtk_widget_get_toplevel(win->getXournal()->getWidget());
            // Back where the user typed, unless they have moved on meanwhile
            if (owner && GTK_IS_WINDOW(top) && !gtk_window_get_focus(GTK_WINDOW(top)) &&
                gtk_widget_get_toplevel(owner) == top && gtk_widget_is_visible(owner) &&
                gtk_widget_is_sensitive(owner)) {
                if (GTK_IS_ENTRY(owner)) {
                    gtk_entry_grab_focus_without_selecting(GTK_ENTRY(owner));  // (else the next key replaces all)
                } else {
                    gtk_widget_grab_focus(owner);
                }
            } else {
                win->getXournal()->requestFocus();
            }
        }
        if (owner) {
            g_object_unref(owner);
        }
    });
}

auto BlockingJob::getType() -> JobType { return JOB_TYPE_BLOCKING; }
