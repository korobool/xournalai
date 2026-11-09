#include "LiveModes.h"

#include <fstream>  // for ifstream, ofstream

#include <glib.h>  // for g_warning

#include "McpConfig.h"  // for McpConfig::path

namespace xoj::mcp::live {

const std::vector<Mode>& modes() {
    // (the order of the menu's grid, after the plain recording: three in a row)
    static const std::vector<Mode> list = {
            {"text", "📝", "Текст", "транскрипт на странице", "none", "off", true, false, "", true},
            {"voice", "🎙", "Голосовое управление", "команды голосом,\nна странице ничего", "none", "commands",
             false, true, "the voice-control skill", true},
            {"drawing", "✍", "Просьбы к рисунку", "голосом правлю\nсвой рисунок", "none", "drawing", true, true,
             "modes/drawing.md", true},
            {"conspect", "🧠", "Конспект + просьбы", "карточки, схемы,\nвыполняет просьбы", "cards", "notes", true,
             true, "", true},
            {"safe", "🛡", "Конспект без команд", "схемы и карточки,\nпросьбы не выполняет", "visual", "off", true,
             true, "modes/safe.md", true},
            {"konspekt", "🗒", "Ведение конспекта", "вёрстка и графики", "visual", "notes", true, true,
             "modes/konspekt.md", true},
    };
    return list;
}

const Mode* findMode(const std::string& id) {
    for (const auto& m: modes()) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

const std::vector<Toggle>& toggles() {
    static const std::vector<Toggle> list = {
            {"intense", "⚡ Интенсивно", "🌿 Экономно", true},
            {"summary", "🧾 Итог в конце", "", true},
            {"trigger", "🔤 Просьбы по слову", "", true},
            {"questions", "❓ Панель вопросов", "", false},
            {"browser", "🔎 Браузер-помощник", "", false},
    };
    return list;
}

bool Options::get(const std::string& id) const {
    return id == "intense" ? intense :
           id == "summary" ? summary :
           id == "trigger" ? trigger :
           id == "questions" ? questions :
           id == "browser" ? browser :
                             false;
}

void Options::set(const std::string& id, bool on) {
    (id == "intense" ? intense :
     id == "summary" ? summary :
     id == "trigger" ? trigger :
     id == "questions" ? questions :
                         browser) = on;
}

std::vector<std::string> Options::names() const {
    std::vector<std::string> out{intense ? "intense" : "economy"};
    for (const char* id: {"summary", "trigger", "questions", "browser"}) {
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
    if (j.contains("last_mode") && j["last_mode"].is_string() && findMode(j["last_mode"].get<std::string>())) {
        s.lastMode = j["last_mode"].get<std::string>();
    }
    if (j.contains("options") && j["options"].is_object()) {
        for (const auto& t: toggles()) {
            if (j["options"].contains(t.id) && j["options"][t.id].is_boolean()) {
                s.options.set(t.id, j["options"][t.id].get<bool>());
            }
        }
    }
    return s;
}

void State::save() const {
    json options = json::object();
    for (const auto& t: toggles()) {
        options[t.id] = this->options.get(t.id);
    }
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    std::ofstream out(file());
    out << json({{"last_mode", lastMode}, {"options", options}}).dump(2) << "\n";
    if (!out) {
        g_warning("live notes: could not write %s", file().string().c_str());
    }
}

bool applies(const std::string& toggle, const Mode& mode) {
    if (toggle == "summary") {
        return mode.id != "voice";  // (a summary of commands means nothing; the transcript's mode gets one)
    }
    if (mode.id == "text") {
        return false;  // (no agent while it records)
    }
    if (toggle == "trigger") {
        return mode.requests != "off";
    }
    return true;
}

std::string describe(const Mode& mode, const Options& options, const std::vector<std::string>& triggerWords) {
    std::string s = "mode «" + mode.label + "» (" + mode.id + ": annotate=" + mode.annotate + ", requests=" +
                    mode.requests + "; options:";
    bool first = true;
    for (const auto& n: options.names()) {
        if (n == "economy" || n == "intense" || applies(n, mode)) {
            s += (first ? " " : ", ") + n;
            first = false;
        }
    }
    if (options.trigger && applies("trigger", mode) && !triggerWords.empty()) {
        s += "; trigger words:";
        for (size_t i = 0; i < triggerWords.size(); i++) {
            s += (i ? ", «" : " «") + triggerWords[i] + "»";
        }
    }
    return s + ")";
}

}  // namespace xoj::mcp::live
