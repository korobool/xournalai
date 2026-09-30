/*
 * xournalai (based on Xournal++)
 *
 * The Ask popover: shown next to the area the user circled (or selected). It holds what was said (editable),
 * one-tap command icons, a hold-to-talk microphone button and Send. It does not grab the pen: drawing elsewhere
 * goes on, and the popover closes on send, Esc or its close button.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

namespace xoj::assistant {

class AskPopover final {
public:
    struct Command {
        const char* id;     ///< e.g. "improve" (sent to the serving session)
        const char* icon;   ///< themed icon name
        const char* label;  ///< tooltip
        const char* name;   ///< shown under the icon
    };
    static const std::vector<Command>& commands();

    /// `command` is "" for free text (Send)
    using Submit = std::function<void(const std::string& command, const std::string& text)>;
    using Mic = std::function<void(bool pressed)>;  ///< the microphone button is held (true) / released (false)

    AskPopover(GtkWidget* canvas, Submit submit, Mic mic);
    ~AskPopover();
    AskPopover(const AskPopover&) = delete;
    AskPopover& operator=(const AskPopover&) = delete;

    /// Shows the popover pointing at `rect` (canvas coordinates) with `text`
    void show(const GdkRectangle& rect, const std::string& text);
    void close();
    bool visible() const;
    std::string text() const;
    /// Appends dictated text (after a space)
    void appendText(const std::string& more);
    /// A short status under the text ("listening…", "transcribing…", an error); "" hides it
    void setStatus(const std::string& status);

private:
    void submit(const std::string& command);

    GtkWidget* popover = nullptr;  ///< owned via the canvas (a GtkPopover's relative_to)
    GtkWidget* entry = nullptr;
    GtkWidget* status = nullptr;
    Submit onSubmit;
    Mic onMic;
};

}  // namespace xoj::assistant
