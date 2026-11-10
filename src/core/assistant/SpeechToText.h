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
#include <functional>
#include <memory>  // for function
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

    /// The owner's remote transcriber, tried first (1.21.0; the owner: every tool that hears him the same way, the
    /// remote multilingual model when it is there, else the local English one). Given a recording as a WAV, it
    /// answers its text, or an error (not there, failed, too slow): then the same WAV goes to the local model.
    using Remote = std::function<void(const std::string& wav,
                                      std::function<void(const std::string& text, const std::string& error)> done)>;
    void setRemote(Remote r) { remote = std::move(r); }
    /// The local model on a WAV file (16 kHz mono 16-bit): the fallback of the other tools too
    void transcribeWav(const std::string& path, Done done);

    State state() const { return current; }
    static const char* name(State s);
    std::string model() const { return modelName; }
    std::string problem() const { return lastProblem; }  ///< why it is unavailable, or the last error
    /// Called when the state changes
    void setListener(std::function<void(State)> l) { listener = std::move(l); }
    /// Called ~20 times a second while recording from the microphone, with its level (RMS, about 0..0.3)
    void setLevelListener(std::function<void(float)> l) { levelListener = std::move(l); }

    static std::string modelsDir();
    /// Diagnostics: a line in ~/.cache/xournalai/speech.log (shared with the helper)
    static void log(const std::string& line);
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
    std::deque<Done> waiting;   ///< stop() calls waiting for their transcript, in order
    std::deque<Done> saving;    ///< stop() calls waiting for their recording as a WAV (the remote transcriber first)
    Remote remote;
    int remotePending = 0;
    int saved = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    void onSaved(const std::string& path, bool silent);
    bool recordingNow = false;  ///< between start() and stop() / cancel()
    /// Listening while recording, else Transcribing while transcripts are pending, else Ready
    void settle();
    std::function<void(State)> listener;
    std::function<void(float)> levelListener;
};

}  // namespace xoj::assistant
