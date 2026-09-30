/*
 * xournalai (based on Xournal++)
 *
 * The app's side of local speech to text: runs the xournalai-stt helper (whisper.cpp, see src/stt) and talks to it
 * with JSON lines. The helper is started early and kept running, so the model is loaded before the user presses the
 * pen button; if it exits it is started again on the next use. A missing model is downloaded once (with curl) into
 * ~/.local/share/xournalai/models.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <deque>       // for deque
#include <functional>  // for function
#include <string>      // for string

#include <gio/gio.h>

namespace xoj::assistant {

class SpeechToText final {
public:
    enum class State { Unavailable, Downloading, Starting, Ready, Listening, Transcribing };

    /// `model`: e.g. "base.en" (file ggml-base.en.bin)
    explicit SpeechToText(std::string model);
    ~SpeechToText();
    SpeechToText(const SpeechToText&) = delete;
    SpeechToText& operator=(const SpeechToText&) = delete;

    /// Starts the helper (and downloads the model if needed); harmless if already running
    void warmUp();
    /// Begins recording from the microphone
    void start();
    /// Ends the recording; `done(text, silent)` gets the transcript ("" and silent=true if nothing was said) or
    /// `error` is set
    using Done = std::function<void(const std::string& text, bool silent, const std::string& error)>;
    void stop(Done done);
    /// Ends the recording without a transcript
    void cancel();

    State state() const { return current; }
    static const char* name(State s);
    std::string model() const { return modelName; }
    std::string problem() const { return lastProblem; }  ///< why it is unavailable, or the last error
    /// Called when the state changes
    void setListener(std::function<void(State)> l) { listener = std::move(l); }

    static std::string modelsDir();
    std::string modelPath() const;
    static std::string helperPath();

private:
    void set(State s);
    void spawn();
    void download();
    void send(const std::string& line);
    void readNext();
    void onLine(const std::string& line);
    void onExit();

    std::string modelName;
    State current = State::Unavailable;
    std::string lastProblem;
    GSubprocess* proc = nullptr;
    GOutputStream* in = nullptr;
    GDataInputStream* out = nullptr;
    GCancellable* cancellable = nullptr;
    GSubprocess* downloader = nullptr;
    std::deque<Done> waiting;  ///< stop() calls waiting for their transcript, in order
    std::function<void(State)> listener;
};

}  // namespace xoj::assistant
