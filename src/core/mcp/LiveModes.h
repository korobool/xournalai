/*
 * xournalai (based on Xournal++)
 *
 * The Record menu (a long press on the big Record button; the owner's choice of 2026-11-09, reworked 2026-11-10:
 * docs/mcp/LIVE_MODES.md) asks two separate questions:
 *  - its tiles: what appears on paper while he talks (📝 Transcript, 🧠 Notes, 🗒 Typeset notes, 🎙 Voice only);
 *  - one switch: may his speech steer things (🛡 off · 🔤 only «Hey Assistant» · 💬 conversation).
 * Before, each of six tiles answered both at once (✍ = 📝 + 💬, 🛡 = 🧠 + 🛡) and the trigger phrase was a third level
 * of the same question as a separate option. The options of a recording (economy or intense, a summary at the end, the
 * question panel's proposals, the research browser) are big toggles in the same menu; all of it is remembered between
 * recordings in the app's own config folder, and the switch can be changed during a recording. A short tap on Record
 * stays a plain recording, and «Hold to talk» is a command of its own whatever is chosen here.
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
    std::string id;        ///< text, voice, conspect, konspekt (old ids: drawing = text + 💬, safe = conspect + 🛡)
    std::string emoji;     ///< the tile's big sign
    std::string label;     ///< the tile's name
    std::string hint;      ///< one line under it
    std::string annotate;  ///< what it draws by itself: none | visual
    bool transcriptPage;   ///< a live-notes page with the transcript column (voice control has none)
    std::string guide;     ///< its own module ("" = the live-annotation skill as written)
    bool commandsOff;      ///< whether 🛡 (no commands) makes sense for it (🎙 Voice only is nothing but commands)
    bool available;        ///< built already (the menu shows the others as "soon")
    std::string explain;   ///< (1.22.0) what it does, in a few lines: the tooltip, or a long press's strip
};

/// The tiles in the menu's order (the plain recording is the Record button's own tap, not one of them)
const std::vector<Mode>& modes();
/// A mode by its id; the old ids too (drawing → text, safe → conspect)
const Mode* findMode(const std::string& id);

/// The switch «Speech commands»: off | trigger | talk
struct Level {
    std::string id;
    std::string emoji;      ///< 🛡 🔤 💬
    std::string label;      ///< what it does
    std::string situation;  ///< when to choose it (the owner thinks of who is talking)
    std::string explain;    ///< (1.22.0) what it does
};
const std::vector<Level>& levels();
const Level* findLevel(const std::string& id);

/// A toggle of the Record menu
struct Toggle {
    std::string id;     ///< this_page, intense, summary, questions, browser, speakers
    std::string label;  ///< when on
    std::string off;    ///< when off ("" = the same label, unticked)
    bool available;     ///< built already
    std::string hint;     ///< (a switch) a second line when on ("" none)
    std::string offHint;  ///< (a switch) a second line when off
    std::string explain;  ///< (1.22.0) what it does
};
const std::vector<Toggle>& toggles();

/// The options of the next recording (remembered)
struct Options {
    bool thisPage = false;  ///< (1.22.0) 📍 start on the page he is on, else 📄 a new page after it
    bool intense = false;  ///< ⚡ intense (no saving of the owner's limits), else 🌿 economy
    bool summary = false;  ///< 🧾 a summary page at the end
    bool questions = false;
    bool browser = false;
    bool speakers = false;  ///< (1.23.0, experimental) 👥 who said what in the transcript
    std::string commands = "talk";  ///< the switch: off | trigger | talk
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
    /// (an old file: last_mode safe → conspect with 🛡, drawing → text with 💬; options.trigger → 🔤)
    static State load();
    void save() const;
};

/// The commands level a mode really gets (🎙 never 🛡: then 🔤)
std::string effectiveCommands(const Mode& mode, const Options& options);
/// Whether an option applies to a mode (its tile shows it as a badge): 📝 with 🛡 has no agent while it records; 🎙
/// has no page
bool applies(const std::string& toggle, const Mode& mode, const Options& options);

/// What a recording of `mode` with `options` does: woken while he talks, for which requests, with which guides
struct Plan {
    bool wakeWhileTalking = true;
    std::string requests;  ///< off | notes | drawing (on his own page) | commands (🎙)
    std::string guides;    ///< what the serving session reads besides the skill ("" none)
    std::string rule;      ///< how to treat what is said ("" = the skill's default)
    bool latestInk = false;          ///< «this» is his latest ink (told in the wake-up line)
    bool triggerOnly = false;        ///< only phrases starting with the trigger phrase are requests
    bool wakeOnlyForTrigger = false;  ///< (📝 with 🔤) the app wakes the session only for such a phrase
};
Plan plan(const Mode& mode, const Options& options);

/// "mode «Notes» (conspect: annotate=visual, commands=off; options: economy, summary; trigger words: «hey
/// assistant», …)": named in every wake-up line
std::string describe(const Mode& mode, const Options& options, const std::vector<std::string>& triggerWords);

}  // namespace xoj::mcp::live
