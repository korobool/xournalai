/*
 * xournalai (based on Xournal++)
 *
 * The question panel (1.17.2, the owner's wish): agents ask the owner, he answers with a tap. In the AI terminal it is
 * the strip on the left (the terminal in the middle, the big keys on the right), under the left thumb when he holds
 * the tablet; when the terminal is closed, the same card shows at the bottom left of the canvas. It shows who asks,
 * the question and only the asker's options as big buttons ("Yes, draw it", "This one", "That one"; a yes/no
 * question: Yes, No), in the interface's language. Questions wait in a queue; each has a safe default that applies when nobody answers in time. Answers
 * come only from a tap (speech is the untrusted channel): the trusted way to confirm what an agent may do.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include <gtk/gtk.h>

namespace xoj::assistant {

class QuestionPanel final {
public:
    struct Option {
        std::string label;  ///< what the button says ("Yes, draw it")
        std::string value;  ///< what the agent gets back (its label, unless given)
    };
    struct Question {
        int id = 0;
        std::string who;   ///< "Claude", "⚙ T4"
        std::string text;  ///< the question
        std::vector<Option> options;  ///< empty: a yes/no question (Yes → "yes", No → "no")
        std::string fallback;         ///< the answer when nobody answers in time (safe: "no", "skip")
        gint64 deadlineUs = 0;
    };
    /// (id, value, by: "owner" | "timeout" | "cancelled")
    using Answered = std::function<void(int id, const std::string& value, const std::string& by)>;

    explicit QuestionPanel(Answered onAnswer);
    ~QuestionPanel();
    QuestionPanel(const QuestionPanel&) = delete;
    QuestionPanel& operator=(const QuestionPanel&) = delete;

    /// Queues a question (shown when the ones before are answered); returns its id
    int ask(Question q);
    /// Withdraws a question (no longer relevant): answered "cancelled"
    bool cancel(int id, const std::string& why = "cancelled");
    /// The owner's tap (or the views' buttons): answers the question shown now
    void answer(int id, const std::string& value);
    const std::vector<Question>& pending() const { return queue; }

    /// The strip for the terminal, and the card for the canvas: two views of the same queue
    GtkWidget* dockView();
    GtkWidget* floatView();
    /// Whether the card on the canvas should show (questions wait, and the terminal is not shown)
    void setDockShown(bool shown);

private:
    struct View {
        GtkWidget* root = nullptr;  ///< weak
        GtkWidget* who = nullptr;
        GtkWidget* text = nullptr;
        GtkWidget* options = nullptr;
        GtkWidget* left = nullptr;  ///< the countdown to the default
        int shownId = -1;  ///< the question its option buttons are for (rebuilt only when it changes)
    };
    GtkWidget* build(View& v, bool floating);
    void refresh();
    void refresh(View& v);
    void tick();  ///< once a second while questions wait: the countdown, and defaults when time is up
    void finish(int id, const std::string& value, const std::string& by);

    Answered onAnswer;
    std::vector<Question> queue;
    int nextId = 1;
    View dock;
    View floating;
    bool dockShown = false;
    guint timer = 0;
};

}  // namespace xoj::assistant
