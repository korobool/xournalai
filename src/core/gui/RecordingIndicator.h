/*
 * xournalai (based on Xournal++)
 *
 * Shows in the status line that the audio recorder is on, so it is not forgotten: a pulsing red dot, the live
 * microphone level, the time recorded so far and a Stop button. Hidden while nothing records.
 * Ask's listening shows there too (teal voice bars, no Stop: releasing the pen button ends it), and so does the
 * processing of a recording the owner handed over (a travelling wave and its current step, e.g. "Notes:
 * transcribing…"). The recorder wins, then Ask, then processing.
 *
 * It lives in a slot of its own at the bottom of the window; the AI status line takes it in while it exists
 * (attachTo) and gives it back before it goes away (detach).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <array>       // for array
#include <cstdint>     // for int64_t
#include <functional>  // for function
#include <string>      // for string

#include <gtk/gtk.h>  // for GtkWidget

namespace xoj::gui {

class RecordingIndicator final {
public:
    struct Source {
        std::function<float()> level;  ///< the loudest sample (0..1) since the last call
        std::function<void()> stop;    ///< stops the recording (as the toolbar button does)
    };

    /// Packs its slot at the bottom of `mainBox`
    RecordingIndicator(GtkWidget* mainBox, Source source);
    ~RecordingIndicator();
    RecordingIndicator(const RecordingIndicator&) = delete;
    RecordingIndicator& operator=(const RecordingIndicator&) = delete;

    void setRecording(bool on);
    bool isRecording() const { return recording; }

    /// Ask listens (speech to text); its voice level (RMS) comes with pushVoiceLevel
    void setListening(bool on);
    bool isListening() const { return listening; }
    void pushVoiceLevel(float rms);

    /// A handed-over recording is being processed: its current step ("" when none is)
    void setProcessing(const std::string& text);
    const std::string& getProcessing() const { return processing; }

    /// Moves the indicator into `box` at `position` (e.g. the AI status line); detach() brings it back
    void attachTo(GtkBox* box, int position);
    void detach();

    GtkWidget* getWidget() const { return widget; }

private:
    enum class Mode { Off, Recorder, Ask, Processing };
    Mode mode() const {
        return recording ? Mode::Recorder : listening ? Mode::Ask : !processing.empty() ? Mode::Processing : Mode::Off;
    }
    void refresh();
    void push(float level);

    static gboolean onTick(gpointer self);
    static gboolean onDraw(GtkWidget* area, cairo_t* cr, gpointer self);
    void tick();
    void updateLabel();

    Source source;
    GtkWidget* slot = nullptr;  ///< its own place at the bottom of the window
    GtkWidget* widget = nullptr;
    GtkWidget* wave = nullptr;
    GtkWidget* label = nullptr;

    GtkWidget* stop = nullptr;

    bool recording = false;
    bool listening = false;
    std::string processing;
    int64_t processingSinceUs = 0;
    Mode shown = Mode::Off;
    int64_t recordingSinceUs = 0;
    int64_t listeningSinceUs = 0;
    int64_t startedUs = 0;  ///< of what is shown
    int shownSeconds = -1;
    guint timer = 0;

    static constexpr int BARS = 18;
    std::array<float, BARS> levels{};  ///< the latest last
};

}  // namespace xoj::gui
