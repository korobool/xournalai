/*
 * xournalai (based on Xournal++)
 *
 * When a recording stops, the owner decides what it is before anything happens: spoken instructions for the
 * assistant, notes to keep (a transcript, never instructions), or just the audio. Big targets for a finger or a
 * stylus; an optional title or note goes along. Recordings that end while it is open wait their turn.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for int64_t
#include <deque>       // for deque
#include <functional>  // for function
#include <string>      // for string

#include <gtk/gtk.h>  // for GtkWidget

namespace xoj::assistant {

class RecordingChooser final {
public:
    enum class Choice { Instructions, Notes, Keep };
    static const char* name(Choice c);  ///< "instructions", "notes", "keep"

    struct Recording {
        std::string file;  ///< full path
        std::string name;  ///< the file name strokes refer to
        int64_t durationMs = 0;
        size_t page = 0;     ///< where most of its strokes are (0-based)
        size_t strokes = 0;  ///< written meanwhile
    };
    using Chosen = std::function<void(const Recording&, Choice, const std::string& note)>;

    RecordingChooser(GtkWidget* canvas, Chosen chosen);
    ~RecordingChooser();
    RecordingChooser(const RecordingChooser&) = delete;
    RecordingChooser& operator=(const RecordingChooser&) = delete;

    /// Asks about `r` (now, or after the ones already waiting)
    void offer(Recording r);
    size_t pending() const { return queue.size(); }
    bool visible() const;

private:
    void showFront(bool fresh);  ///< fresh: a new recording is in front (else only the count changed)
    void choose(Choice c);

    GtkWidget* canvas = nullptr;
    GtkWidget* popover = nullptr;  ///< owned via the canvas (a GtkPopover's relative_to)
    GtkWidget* title = nullptr;
    GtkWidget* details = nullptr;
    GtkWidget* note = nullptr;
    Chosen onChosen;
    std::deque<Recording> queue;
};

}  // namespace xoj::assistant
