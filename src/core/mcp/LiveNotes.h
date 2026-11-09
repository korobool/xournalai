/*
 * xournalai (based on Xournal++)
 *
 * Live notes (experimental): one recording transcribed while it goes on. The recorded sound (through the sample
 * tap) is cut into pieces at pauses (LiveChunker), each piece goes to the owner's speech recognizer
 * (assistant::Transcriber: any program behind docs/mcp/TRANSCRIBERS.md), and its text, with its time in the recording,
 * goes into a narrow column on the right of a live-notes page inserted after the page the owner was on (more pages
 * as it fills). The rest of the page is the serving Claude's: it is woken when new text is there, reads it with
 * live_notes(op="read") and draws what is being talked about at its own pace (the live-annotation skill); one
 * wake-up waits until it has read. The recording itself is unchanged.
 *
 * The pieces stay in <audio folder>/live/<recording>/ (WAV), the whole text in transcripts/<recording>.live.txt.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>     // for int64_t
#include <functional>  // for function
#include <map>         // for map
#include <memory>      // for shared_ptr, unique_ptr
#include <optional>    // for optional
#include <string>      // for string
#include <vector>      // for vector

#include "assistant/LiveChunker.h"      // for LiveChunker
#include "assistant/Transcriber.h"  // for Transcriber
#include "model/PageRef.h"              // for PageRef

#include "Json.h"
#include "Registry.h"    // for ToolResult
#include "filesystem.h"  // for path

namespace xoj::mcp {

class McpServer;

class LiveNotes final {
public:
    struct Options {
        assistant::LiveChunker::Options chunker;
        json transcriber;  ///< the speech recognizer, as written in mcp.json (docs/mcp/TRANSCRIBERS.md)
        std::string language = "auto";
        double transcriptSize = 3.8;  ///< the column's font size (points): small, the page is for the drawing
        // The mode (LiveModes.h; since 1.17.0): named in every wake-up line
        std::string mode = "conspect";
        std::string modeLine;          ///< "mode «…» (id: annotate=…, requests=…; options: …)"
        std::string guide;             ///< what the serving session reads for the mode ("" = the skill as written)
        bool wakeWhileTalking = true;  ///< woken for new speech (📝 Текст: never while it records)
        bool summary = false;          ///< 🧾 a summary page at the end (📝 Текст: woken once, at the end, for it)
        std::string rule;  ///< what spoken requests may do in the mode (empty: the safe list on the live pages)
        /// More for each wake-up line (✍: where the owner drew last)
        std::function<std::string()> context;
        // 🎙 voice control (1.17.1): no page, nothing written on the document; each piece is a command (said while
        // the owner held the talk button, or starting with a trigger word) or context
        bool transcriptPage = true;
        bool voice = false;
        std::vector<std::string> triggerWords;  ///< (lower case) empty: no trigger word
        std::function<void(const std::string& text, bool command)> heard;  ///< a piece pops up
    };
    using Wake = std::function<void(const std::string& description)>;  ///< an intent for the serving session
    using Changed = std::function<void()>;                              ///< (the status strip)

    LiveNotes(McpServer& server, Options options, std::string recording, fs::path audioFolder, size_t afterPage,
              Wake wake, Changed changed);
    ~LiveNotes();
    LiveNotes(const LiveNotes&) = delete;
    LiveNotes& operator=(const LiveNotes&) = delete;

    /// What the sample tap feeds (on the recorder's thread)
    std::shared_ptr<assistant::LiveChunker> chunker() const { return pieceCutter; }
    /// ✂: the piece being spoken goes now
    void cut();
    /// 🎙: the talk button held (what is said meanwhile is a command; the piece before ends now) or let go (the
    /// command is sent now)
    void talk(bool down);
    bool talking() const { return talkingSince.has_value(); }
    /// The recorder stopped (the tap is already off)
    void recordingEnded();

    const std::string& recording() const { return name; }
    bool listening() const { return !ended; }
    bool done() const;  ///< ended, and every piece answered and written
    /// For live_notes(op="status")
    json status() const;
    /// For live_notes(op="read"): the pieces written since `since` (default: since the last read)
    json read(std::optional<int> since);
    /// One line for the status strip
    std::string statusLine() const;

private:
    struct Piece {
        int index = 0;
        int64_t startMs = 0;
        int64_t endMs = 0;
        std::string reason;
        std::string wav;
        bool answered = false;
        bool ok = false;
        bool written = false;
        std::string text;
        std::string language;
        std::string error;
        double elapsed = -1;
        bool command = false;     ///< 🎙: taken as a command
        std::string commandText;  ///< (without the trigger word)
    };
    void heardPiece(Piece& p);  ///< 🎙: a command (woken with it) or context (kept for the next command)
    void onPiece(assistant::LiveChunk&& c);
    void onResult(const assistant::TranscriberResult& r);
    void pump();  ///< writes answered pieces into the column, in order, one at a time
    void pumpLater();
    void newPage(std::function<void()> then);
    /// Writes text boxes (create_text's JSON) on `page`: not undoable by the user, origin "app"; `then(bottom)`
    void writeTexts(const PageRef& page, const json& texts, std::function<void(double bottom)> then);
    std::optional<size_t> pageNumber(size_t i) const;  ///< 1-based number of the i-th live page (if it still exists)
    json pagesJson() const;
    void maybeWake();
    void appendTranscript(const Piece& p);  ///< the transcript file grows with each piece (kept if the app exits)
    fs::path transcriptPath() const;
    int lastWritten() const { return nextToWrite - 1; }

    McpServer& server;
    Options opt;
    std::string name;
    fs::path audioFolder;
    fs::path folder;  ///< the pieces
    Wake wake;
    Changed changed;
    std::shared_ptr<assistant::LiveChunker> pieceCutter;
    std::unique_ptr<assistant::Transcriber> transcriber;  ///< (null: none usable; `problem` says why)
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    std::map<int, Piece> pieces;
    int nextToWrite = 1;
    int lastRead = 0;
    bool writing = false;
    bool ended = false;
    bool wakePending = false;
    bool endWoken = false;
    std::vector<std::pair<int64_t, int64_t>> talks;  ///< 🎙: the talk button's intervals (ms in the recording)
    std::optional<int64_t> talkingSince;
    std::string contextSince;  ///< 🎙: what was said since the last command
    int commands = 0;
    bool transcriptStarted = false;
    std::string pageProblem;
    std::string engine;
    std::string problem;
    std::string languageHint;
    std::string lastText;
    std::string startedAt;  ///< "HH:MM"

    // the page(s)
    size_t afterPage;  ///< 0-based: the page the owner was on
    std::vector<PageRef> pages;
    double pageW = 0;
    double pageH = 0;
    double colX = 0;
    double colW = 0;
    double y = 0;  ///< the next line in the column
    double columnTop = 0;  ///< where a column's first line goes
    double top = 0;  ///< where the free area starts (below the title)
};

}  // namespace xoj::mcp
