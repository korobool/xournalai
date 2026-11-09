/*
 * xournalai (based on Xournal++)
 *
 * Live notes (experimental): one recording transcribed while it goes on. The recorded sound (through the sample
 * tap) is cut into pieces at pauses (LiveChunker), each piece goes to the owner's speech recognizer
 * (LiveTranscriber: any program behind docs/mcp/TRANSCRIBERS.md), and its text, with its time in the recording,
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
#include "assistant/LiveTranscriber.h"  // for LiveTranscriber
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
        assistant::TranscriberConfig transcriber;
        std::string language = "auto";
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
    };
    void onPiece(assistant::LiveChunk&& c);
    void onResult(const assistant::LiveTranscriber::Result& r);
    void pump();  ///< writes answered pieces into the column, in order, one at a time
    void newPage(std::function<void()> then);
    void call(const std::string& tool, json args, std::function<void(const ToolResult&)> then);
    std::optional<size_t> pageNumber(size_t i) const;  ///< 1-based number of the i-th live page (if it still exists)
    json pagesJson() const;
    void maybeWake();
    void writeTranscriptFile();
    int lastWritten() const { return nextToWrite - 1; }

    McpServer& server;
    Options opt;
    std::string name;
    fs::path audioFolder;
    fs::path folder;  ///< the pieces
    Wake wake;
    Changed changed;
    std::shared_ptr<assistant::LiveChunker> pieceCutter;
    std::unique_ptr<assistant::LiveTranscriber> transcriber;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);

    std::map<int, Piece> pieces;
    int nextToWrite = 1;
    int lastRead = 0;
    bool writing = false;
    bool ended = false;
    bool wakePending = false;
    bool endWoken = false;
    bool fileWritten = false;
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
    double top = 0;  ///< where the free area starts (below the title)
};

}  // namespace xoj::mcp
