/*
 * xournalai (based on Xournal++)
 *
 * Live notes: sends the pieces of a recording to a speech recognizer and brings back their text. The recognizer is
 * any program behind the interface in docs/mcp/TRANSCRIBERS.md (the specification: keep both in step):
 *  - "file": a command run once per piece ({audio}, {language}, {prompt} replaced), its stdout the text;
 *  - "stream": a long-running process speaking JSON lines (hello, transcribe → result / error).
 * Nothing here knows which recognizer it is, or where it runs.
 *
 * Runs on the GLib main loop; results come back there.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <deque>       // for deque
#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for shared_ptr
#include <string>      // for string
#include <vector>      // for vector

#include <gio/gio.h>  // for GSubprocess, GCancellable

namespace xoj::assistant {

struct TranscriberConfig {
    std::string kind;     ///< "file" or "stream"
    std::string command;  ///< run through the login shell
    bool valid() const { return (kind == "file" || kind == "stream") && !command.empty(); }
};

class LiveTranscriber final {
public:
    static constexpr int PROTOCOL = 1;          ///< docs/mcp/TRANSCRIBERS.md
    static constexpr guint TIMEOUT_S = 120;     ///< per piece; also the wait for the hello

    struct Piece {
        std::string id;
        std::string audioPath;  ///< WAV, PCM 16-bit mono 16 kHz
        std::string language = "auto";
        std::string languageHint;
        std::string prompt;
    };
    struct Result {
        std::string id;
        bool ok = false;
        std::string text;
        std::string language;
        double languageProbability = -1;
        double elapsed = -1;
        std::string error;
    };
    using OnResult = std::function<void(const Result&)>;
    using OnStatus = std::function<void(const std::string&)>;  ///< "ready: <engine>", problems

    LiveTranscriber(TranscriberConfig config, OnResult onResult, OnStatus onStatus = {});
    ~LiveTranscriber();
    LiveTranscriber(const LiveTranscriber&) = delete;
    LiveTranscriber& operator=(const LiveTranscriber&) = delete;

    void submit(Piece piece);
    /// No more pieces: the recognizer is let go once every answer is in (or after TIMEOUT_S)
    void finish();
    /// Pieces sent or queued without an answer yet
    size_t waiting() const { return queue.size() + inFlight.size(); }
    bool done() const { return finished && waiting() == 0; }
    std::string engine() const { return engineName; }

    struct State;  ///< (internal) outlives this object while async calls are pending
    void lineFrom(bool isStdout, const std::string& line);  ///< (internal) a line the recognizer wrote

private:
    void startStream();
    void pumpStream();
    void writeLine(const std::string& line);
    void writeNext();
    void onStreamLine(const std::string& line);
    void runNextFile();
    void answer(Result r);
    void failAll(const std::string& why);
    void armTimeout(const std::string& id);
    void closeIfDone();

    TranscriberConfig config;
    OnResult onResult;
    OnStatus onStatus;
    std::shared_ptr<State> state;
    std::deque<Piece> queue;                 ///< not sent yet
    std::map<std::string, guint> inFlight;   ///< sent: id → its timeout source
    std::deque<std::string> writes;          ///< lines waiting for the pipe
    bool writing = false;
    bool ready = false;     ///< stream: the hello came
    bool inlineAudio = false;
    bool finished = false;
    bool closed = false;
    guint helloTimer = 0;
    guint finishTimer = 0;
    std::string engineName;
    std::string lastStderr;
};

/// Writes mono 16 kHz 16-bit samples as a WAV file
bool writeWav16k(const std::string& path, const int16_t* samples, size_t count);

}  // namespace xoj::assistant
