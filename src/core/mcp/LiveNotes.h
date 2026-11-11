/*
 * xournalai (based on Xournal++)
 *
 * Live notes (experimental): one recording transcribed while it goes on. The recorded sound (through the sample
 * tap) is cut into pieces at pauses (LiveChunker), each piece goes to the owner's speech recognizer
 * (assistant::Transcriber: any program behind docs/mcp/TRANSCRIBERS.md), and its text, with its time in the recording,
 * goes into a narrow column on the right of a live-notes page inserted after the page the owner was on (more pages
 * as it fills), or of the page he is on when he chose «📍 This page» (1.22.0; no column where the page has its own
 * content at the right edge). The rest of the page is the serving Claude's: it is woken when new text is there, reads
 * it with live_notes(op="read") and draws what is being talked about at its own pace (the live-annotation skill); one
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
#include "assistant/SpeakerTracker.h"   // for SpeakerTracker
#include "assistant/Transcriber.h"  // for Transcriber
#include "model/PageRef.h"              // for PageRef
#include "util/Rectangle.h"             // for Rectangle

#include <glib.h>  // for gint64

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
        /// (1.22.0, «📍 This page») start on the page the owner is on instead of a new one after it
        bool thisPage = false;
        /// (1.23.0, «👥 Speakers», experimental) who said what: voice fingerprints from the recognizer, told apart
        bool speakers = false;
        assistant::SpeakerTracker::Settings speakerSettings;
        bool voice = false;
        std::vector<std::string> triggerWords;  ///< (lower case) empty: no trigger word
        /// (📝 with 🔤, 1.20.0) woken only when a new phrase starts with a trigger word (the rest is context)
        bool wakeOnlyForTrigger = false;
        /// a piece pops up (next to the region the owner drew in while saying it, if he did: page, area)
        std::function<void(const std::string& text, bool command,
                           std::optional<std::pair<size_t, xoj::util::Rectangle<double>>> where)>
                heard;
        /// (1.21.0) the local English model on a piece's WAV, when the remote transcriber fails or isn't there
        std::function<void(const std::string& wav,
                           std::function<void(bool ok, const std::string& text, const std::string& error)> done)>
                fallback;
        /// what the owner drew between two moments (monotonic µs): page and area
        std::function<std::optional<std::pair<size_t, xoj::util::Rectangle<double>>>(gint64 from, gint64 to)> inkDuring;
    };
    using Wake = std::function<void(const std::string& description)>;  ///< an intent for the serving session
    using Changed = std::function<void()>;                              ///< (the status strip)

    LiveNotes(McpServer& server, Options options, std::string recording, fs::path audioFolder, size_t afterPage,
              Wake wake, Changed changed);
    ~LiveNotes();
    LiveNotes(const LiveNotes&) = delete;
    LiveNotes& operator=(const LiveNotes&) = delete;

    /// The switch «Speech commands» changed during the recording (1.20.0): what it is woken for and told
    void reconfigure(const std::string& modeLine, const std::string& guide, const std::string& rule,
                     bool wakeWhileTalking, std::vector<std::string> triggerWords, bool wakeOnlyForTrigger);
    const std::string& mode() const { return opt.mode; }

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
        bool fallbackTried = false;  ///< (the local model has been asked)
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
        /// (👥) its speech by speaker, in order (consecutive segments of one voice joined); empty: no speakers
        struct Turn {
            int64_t startMs = 0;  ///< in the recording
            int64_t endMs = 0;
            std::string speaker;  ///< "A", "B" … or "?"
            std::string text;
        };
        std::vector<Turn> turns;
        size_t turnsWritten = 0;  ///< (each turn is a line of its own in the column, in its speaker's colour)
    };
    /// (👥) the turns of an answered piece from its segments' fingerprints
    void labelTurns(Piece& p, const assistant::TranscriberResult& r);
    void heardPiece(Piece& p);  ///< 🎙: a command (woken with it) or context (kept for the next command)
    void onPiece(assistant::LiveChunk&& c);
    void onResult(const assistant::TranscriberResult& r);
    void pump();  ///< writes answered pieces into the column, in order, one at a time
    void pumpLater();
    void newPage(std::function<void()> then);
    /// «📍 This page»: the page the owner is on is the first live page (its title and column only where it is free)
    void startHere(std::function<void()> then);
    /// Whether the area of `page` holds nothing of its own (no element, no PDF or picture background)
    static bool emptyArea(const PageRef& page, const xoj::util::Rectangle<double>& area);
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
    gint64 startedUs = 0;  ///< when the recording started (piece times → the owner's strokes meanwhile)
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
    bool ownPage = false;   ///< (📍) the first live page is the owner's own page
    bool noColumn = false;  ///< (📍) his page had no room for the column: the transcript stays off the paper

    // (👥) who is speaking
    std::unique_ptr<assistant::SpeakerTracker> tracker;  ///< (null: «Speakers» is off)
    std::string speakersProblem;  ///< why the last pieces have no speakers ("" none)
};

}  // namespace xoj::mcp
