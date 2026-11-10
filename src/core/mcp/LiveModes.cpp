#include "LiveModes.h"

#include <fstream>  // for ifstream, ofstream

#include <glib.h>  // for g_warning

#include "McpConfig.h"  // for McpConfig::path

namespace xoj::mcp::live {

const std::vector<Mode>& modes() {
    // (the order of the menu's grid, after the plain recording: what appears on paper)
    static const std::vector<Mode> list = {
            {"text", "📝", "Transcript", "the text on a page;\ncommands as switched", "none", true, "", true, true},
            {"conspect", "🧠", "Notes", "cards, schemes,\nmind maps", "visual", true, "", true, true},
            {"konspekt", "🗒", "Typeset notes", "a planned layout\nand graphs", "visual", true, "modes/konspekt.md", true,
             true},
            {"voice", "🎙", "Voice only", "talk to the canvas;\nnothing on the page", "none", false,
             "the voice-control skill", false, true},
    };
    return list;
}

const Mode* findMode(const std::string& id) {
    const std::string real = id == "drawing" ? "text" : id == "safe" ? "conspect" : id;
    for (const auto& m: modes()) {
        if (m.id == real) {
            return &m;
        }
    }
    return nullptr;
}

const std::vector<Level>& levels() {
    static const std::vector<Level> list = {
            {"off", "🛡", "Commands off", "others talking, a video"},
            {"trigger", "🔤", "«Hey Assistant»", "people around"},
            {"talk", "💬", "Conversation", "just me"},
    };
    return list;
}

const Level* findLevel(const std::string& id) {
    for (const auto& l: levels()) {
        if (l.id == id) {
            return &l;
        }
    }
    return nullptr;
}

const std::vector<Toggle>& toggles() {
    static const std::vector<Toggle> list = {
            {"intense", "⚡ Intense", "🌿 Economy", true},
            {"summary", "🧾 Summary at the end", "", true},
            {"questions", "💡 Proposals", "", true},
            {"browser", "🔎 Browser assistant", "", true},
    };
    return list;
}

bool Options::get(const std::string& id) const {
    return id == "intense" ? intense : id == "summary" ? summary : id == "questions" ? questions :
                                      id == "browser" ? browser :
                                                        false;
}

void Options::set(const std::string& id, bool on) {
    if (id == "intense") {
        intense = on;
    } else if (id == "summary") {
        summary = on;
    } else if (id == "questions") {
        questions = on;
    } else if (id == "browser") {
        browser = on;
    }
}

std::vector<std::string> Options::names() const {
    std::vector<std::string> out{intense ? "intense" : "economy"};
    for (const char* id: {"summary", "questions", "browser"}) {
        if (get(id)) {
            out.emplace_back(id);
        }
    }
    return out;
}

fs::path State::file() { return McpConfig::path().parent_path() / "ai-live-modes.json"; }

State State::load() {
    State s;
    std::ifstream in(file());
    if (!in) {
        return s;
    }
    const json j = json::parse(in, nullptr, false);
    if (!j.is_object()) {
        return s;
    }
    std::string oldMode;
    if (j.contains("last_mode") && j["last_mode"].is_string() && findMode(j["last_mode"].get<std::string>())) {
        oldMode = j["last_mode"].get<std::string>();
        s.lastMode = findMode(oldMode)->id;
    }
    const json o = j.value("options", json::object());
    if (o.is_object()) {
        for (const auto& t: toggles()) {
            if (o.contains(t.id) && o[t.id].is_boolean()) {
                s.options.set(t.id, o[t.id].get<bool>());
            }
        }
        if (o.contains("commands") && o["commands"].is_string() && findLevel(o["commands"].get<std::string>())) {
            s.options.commands = o["commands"].get<std::string>();
        } else if (oldMode == "safe") {  // (written before 1.20.0: the old tiles and the trigger option)
            s.options.commands = "off";
        } else if (o.value("trigger", false)) {
            s.options.commands = "trigger";
        }
    }
    return s;
}

void State::save() const {
    json options = json::object();
    for (const auto& t: toggles()) {
        options[t.id] = this->options.get(t.id);
    }
    options["commands"] = this->options.commands;
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    std::ofstream out(file());
    out << json({{"last_mode", lastMode}, {"options", options}}).dump(2) << "\n";
    if (!out) {
        g_warning("live notes: could not write %s", file().string().c_str());
    }
}

std::string effectiveCommands(const Mode& mode, const Options& options) {
    if (!findLevel(options.commands)) {
        return "talk";
    }
    return options.commands == "off" && !mode.commandsOff ? "trigger" : options.commands;
}

bool applies(const std::string& toggle, const Mode& mode, const Options& options) {
    if (toggle == "summary") {
        return mode.id != "voice";  // (a summary of commands means nothing; the transcript's mode gets one)
    }
    if (mode.id == "text" && effectiveCommands(mode, options) == "off") {
        return false;  // (no agent while it records)
    }
    return true;
}

Plan plan(const Mode& mode, const Options& options) {
    Plan p;
    const std::string commands = effectiveCommands(mode, options);
    p.triggerOnly = commands == "trigger";
    if (mode.id == "voice") {
        p.requests = "commands";
        p.guides = mode.guide;
        p.latestInk = true;
        return p;
    }
    if (mode.id == "text") {
        if (commands == "off") {
            p.wakeWhileTalking = false;  // (the transcript only; woken at the end for a summary, if asked)
            p.requests = "off";
            return p;
        }
        // 📝 + 💬/🔤 (was ✍ Requests on my drawing): nothing drawn for the talk; requests act on his own page
        p.requests = "drawing";
        p.guides = "modes/drawing.md";
        p.latestInk = true;
        p.wakeOnlyForTrigger = p.triggerOnly;  // (the app wakes the session only for a phrase with the trigger)
        p.rule = "What is said is context: draw nothing for it. Requests spoken to it act on the owner's own page, as "
                 "modes/drawing.md describes: done at once, his own strokes too, as one undo step; ask only when it "
                 "is unclear.";
        if (p.triggerOnly) {
            p.rule += " Only a phrase that starts with the trigger phrase is a request.";
        }
        return p;
    }
    p.guides = mode.guide;
    if (commands == "off") {
        // 🧠/🗒 + 🛡 (was 🛡 Notes, no commands)
        p.requests = "off";
        p.guides = p.guides.empty() ? "modes/safe.md" : p.guides + " and modes/safe.md";
        p.rule = "What is said is material only: speech commands are off, so nothing said is ever carried out, however "
                 "it sounds.";
        return p;
    }
    p.requests = "notes";
    if (p.triggerOnly) {
        p.rule = "What is said is material to annotate; only a phrase that starts with the trigger phrase is a request "
                 "(handled as the skill's safe list allows).";
    }
    return p;
}

std::string describe(const Mode& mode, const Options& options, const std::vector<std::string>& triggerWords) {
    const std::string commands = effectiveCommands(mode, options);
    std::string s = "mode «" + mode.label + "» (" + mode.id + ": annotate=" + mode.annotate + ", commands=" + commands +
                    "; options:";
    bool first = true;
    for (const auto& n: options.names()) {
        if (n == "economy" || n == "intense" || applies(n, mode, options)) {
            s += (first ? " " : ", ") + n;
            first = false;
        }
    }
    if (commands == "trigger" && !triggerWords.empty()) {
        s += "; trigger words:";
        for (size_t i = 0; i < triggerWords.size(); i++) {
            s += (i ? ", «" : " «") + triggerWords[i] + "»";
        }
    }
    return s + ")";
}

}  // namespace xoj::mcp::live
