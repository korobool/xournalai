/*
 * xournalai (based on Xournal++)
 *
 * The AI toolbar: a row under the main toolbars with the actions of the user's marker language
 * (*! **! *w! *r! *c!), Revise page, the Auto-improve toggle, Pause and the AI terminal.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <string>      // for string

#include <gtk/gtk.h>

namespace xoj::assistant {

class AiToolbar final {
public:
    /// kind: improve | illustrate | web | image | command | revise; text: the typed command (for "command")
    using ActionHandler = std::function<void(const std::string& kind, const std::string& text)>;

    /// Inserts the toolbar into `mainBox` at `position`
    AiToolbar(GtkWidget* mainBox, int position, ActionHandler handler);
    ~AiToolbar();
    AiToolbar(const AiToolbar&) = delete;
    AiToolbar& operator=(const AiToolbar&) = delete;

    void setVisible(bool visible);
    bool isVisible() const;
    /// Opens the one-line command prompt (the *c! button)
    void promptCommand();
    /// If set, "Command…" opens this instead of the typing prompt (the Ask popover, with dictation)
    void setCommandOpener(std::function<void()> opener) { commandOpener = std::move(opener); }

    /// The marker label of an action kind, e.g. "*!"
    static const char* marker(const std::string& kind);

private:
    GtkWidget* toolbar = nullptr;  ///< weak
    GtkToolItem* commandItem = nullptr;
    ActionHandler handler;
    std::function<void()> commandOpener;
};

}  // namespace xoj::assistant
