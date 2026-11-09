/*
 * xournalai (based on Xournal++)
 *
 * The modes of live notes, offered by a long press on the big Record button (the owner's choice, 2026-11-09;
 * docs/mcp/LIVE_MODES.md): each is a preset of what is drawn without being asked (annotate), what a spoken request
 * may do (requests) and whether there is a live-notes page with the transcript. The options of a recording (economy
 * or intense, a summary at the end, requests only after a trigger word, …) are big toggles in the same menu,
 * remembered between recordings in the app's own config folder. A short tap on Record stays a plain recording.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "util/PathUtil.h"  // for fs

#include "Json.h"

namespace xoj::mcp::live {

struct Mode {
    std::string id;         ///< text, voice, drawing, conspect, safe, konspekt
    std::string emoji;      ///< the tile's big sign
    std::string label;      ///< the tile's name (as the owner named the modes)
    std::string hint;       ///< one line under it
    std::string annotate;   ///< none | cards | visual
    std::string requests;   ///< off | notes | drawing | commands
    bool transcriptPage;    ///< a live-notes page with the transcript column (voice control has none)
    bool wakeWhileTalking;  ///< the serving session is woken for new speech (text: only at the end, if asked)
    std::string guide;      ///< what the serving session reads for it ("" = the live-annotation skill as written)
    bool available;         ///< built already (the menu shows the others as "soon")
};

/// The modes in the menu's order (the plain recording is the Record button's own tap, not one of them)
const std::vector<Mode>& modes();
const Mode* findMode(const std::string& id);

/// A toggle of the Record menu
struct Toggle {
    std::string id;     ///< intense, summary, trigger, questions, browser
    std::string label;  ///< when on
    std::string off;    ///< when off ("" = the same label, unticked)
    bool available;     ///< built already
};
const std::vector<Toggle>& toggles();

/// The options of the next recording (remembered)
struct Options {
    bool intense = false;  ///< ⚡ intense (no saving of the owner's limits), else 🌿 economy
    bool summary = false;  ///< 🧾 a summary page at the end
    bool trigger = false;  ///< 🔤 a request (or a voice command) only after the trigger word
    bool questions = false;
    bool browser = false;
    bool get(const std::string& id) const;
    void set(const std::string& id, bool on);
    /// For the wake-up line: "intense" or "economy", then the others that are on
    std::vector<std::string> names() const;
};

/// What the menu remembers: the last mode and the options (<config>/ai-live-modes.json)
struct State {
    std::string lastMode = "conspect";
    Options options;
    static fs::path file();
    static State load();
    void save() const;
};

/// Whether an option applies to a mode (its tile shows it as a badge)
bool applies(const std::string& toggle, const Mode& mode);

/// "mode «Конспект без команд» (safe: annotate=visual, requests=off; options: economy, summary; trigger words:
/// «клод», «claude»)": named in every wake-up line
std::string describe(const Mode& mode, const Options& options, const std::vector<std::string>& triggerWords);

}  // namespace xoj::mcp::live
