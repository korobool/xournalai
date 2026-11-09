/*
 * xournalai (based on Xournal++)
 *
 * A speech recognizer ("transcriber"), as live notes use it: pieces of a recording go in, their text comes back.
 * xournalai knows no particular recognizer: it talks to one through the interface specified in
 * docs/mcp/TRANSCRIBERS.md (keep both in step). Each kind of recognizer is a class of its own behind this one:
 *  - "file":   a command run once per piece (FileTranscriber.cpp);
 *  - "stream": a long-running process speaking JSON lines (StreamTranscriber.cpp).
 * A new kind (an HTTP API, …) is a new class and a line in Transcriber::make.
 *
 * Runs on the GLib main loop; results come back there. Every piece submitted is answered exactly once.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for int16_t
#include <functional>  // for function
#include <memory>      // for unique_ptr
#include <string>      // for string

#include <gio/gio.h>            // for GSubprocess
#include <nlohmann/json_fwd.hpp>  // for json

namespace xoj::assistant {

struct TranscriberPiece {
    std::string id;
    std::string audioPath;  ///< WAV, PCM 16-bit mono 16 kHz
    std::string language = "auto";
    std::string languageHint;  ///< filled just before sending (Hooks::prepare)
    std::string prompt;        ///< filled just before sending
};

struct TranscriberResult {
    std::string id;
    bool ok = false;
    std::string text;
    std::string language;
    double languageProbability = -1;
    double elapsed = -1;
    std::string error;
};

struct TranscriberHooks {
    std::function<void(const TranscriberResult&)> onResult;
    std::function<void(const std::string&)> onStatus;  ///< "ready: <engine>", "problem: …", "starting …"
    /// Fills a piece's language hint and prompt just before it is sent (from the answers so far)
    std::function<void(TranscriberPiece&)> prepare;
};

class Transcriber {
public:
    static constexpr int PROTOCOL = 1;  ///< docs/mcp/TRANSCRIBERS.md

    virtual ~Transcriber() = default;
    /// Queues a piece; it is answered exactly once (a result or an error)
    virtual void submit(TranscriberPiece piece) = 0;
    /// No more pieces: the recognizer is let go once every piece is answered
    virtual void finish() = 0;
    /// Pieces submitted and not answered yet
    virtual size_t waiting() const = 0;
    bool done() const { return finished && waiting() == 0; }
    virtual std::string engine() const { return {}; }

    /// The recognizer `config` names ({"kind": …, "command": …, …}: see the specification); null with `error` if it
    /// can't be used
    static std::unique_ptr<Transcriber> make(const nlohmann::json& config, TranscriberHooks hooks, std::string& error);

protected:
    bool finished = false;
};

/// Starts `command` through the owner's login shell (like the AI terminal's tabs: their PATH, ssh keys) in a process
/// group of its own, so that ending it ends what it started too (ssh, scp)
GSubprocess* spawnRecognizer(const std::string& command, GSubprocessFlags flags, GError** error);
/// Signals the process group of a recognizer started by spawnRecognizer
void signalRecognizer(GSubprocess* process, int signal);

/// Writes mono 16 kHz 16-bit samples as a WAV file
bool writeWav16k(const std::string& path, const int16_t* samples, size_t count);

}  // namespace xoj::assistant
