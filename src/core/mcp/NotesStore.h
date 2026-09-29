/*
 * xournalai (based on Xournal++)
 *
 * Notes memory: what the agent learned about the document (transcripts, summaries, meanings), kept next to it
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string
#include <vector>    // for vector

#include "model/DocumentListener.h"  // for DocumentListener
#include "model/PageRef.h"           // for PageRef
#include "util/Rectangle.h"          // for Rectangle

#include "filesystem.h"  // for path

class Control;

namespace xoj::mcp {

struct Note {
    PageRef page;  ///< null: about the whole document. Pages are tracked by identity (moves, inserts are fine)
    std::optional<xoj::util::Rectangle<double>> region;  ///< area on the page (page points)
    std::string kind;                                    ///< transcript | summary | meaning | note
    std::string text;
    std::string updated;  ///< ISO 8601 time of the last change
};

/**
 * @brief Notes about the open document, stored in a sidecar file "<document>.ai-notes.json" next to it (the .xopp
 * format stays unchanged, so upstream Xournal++ opens the files without warnings). Notes of an unsaved document are
 * kept in memory and written once it has a file name. Main thread only.
 */
class NotesStore: public DocumentListener {
public:
    explicit NotesStore(Control* control);
    ~NotesStore() override;
    NotesStore(const NotesStore&) = delete;
    NotesStore& operator=(const NotesStore&) = delete;

    static constexpr size_t MAX_NOTES = 5000;
    static constexpr size_t MAX_TEXT = 200000;

    /// Follows the document: loads the notes of a newly opened file, writes pending notes after the first save
    void sync();

    /// Adds or replaces the note with the same page, region and kind; empty text deletes it. Returns true if
    /// something changed.
    bool set(Note note);
    /// All notes of existing pages and the document
    std::vector<Note> all() const;

    /// Where the notes are stored (empty while the document has no file name)
    fs::path file() const;
    static fs::path sidecarFor(const fs::path& document);

    void documentChanged(DocumentChangeType type) override;

private:
    void load();
    void save() const;

    Control* control;
    fs::path documentPath;
    bool replaced = false;
    std::vector<Note> notes;
};

}  // namespace xoj::mcp
