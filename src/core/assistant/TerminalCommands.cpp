#include "TerminalCommands.h"

#include <algorithm>  // for find, remove
#include <fstream>    // for ifstream, ofstream

#include <glib.h>  // for g_get_home_dir

#include "mcp/Json.h"

namespace xoj::assistant::terminal_commands {

using xoj::mcp::json;

std::vector<SavedCommand> load(const std::filesystem::path& file) {
    std::vector<SavedCommand> out;
    std::ifstream in(file);
    if (!in) {
        return out;
    }
    const json j = json::parse(in, nullptr, false);
    if (!j.is_object() || !j.contains("commands") || !j["commands"].is_array()) {
        return out;
    }
    for (const auto& c: j["commands"]) {
        if (c.is_object()) {
            out.push_back({c.value("title", ""), c.value("command", ""), c.value("folder", "")});
        }
    }
    return out;
}

bool save(const std::filesystem::path& file, const std::vector<SavedCommand>& list) {
    json arr = json::array();
    for (const auto& c: list) {
        arr.push_back({{"title", c.title}, {"command", c.command}, {"folder", c.folder}});
    }
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    out << json{{"commands", arr}}.dump(2) << "\n";
    return static_cast<bool>(out);
}

std::vector<SavedCommand> remember(std::vector<SavedCommand> list, const SavedCommand& cmd) {
    list = forget(std::move(list), cmd);
    list.insert(list.begin(), cmd);
    if (list.size() > MAX_SAVED) {
        list.resize(MAX_SAVED);
    }
    return list;
}

std::vector<SavedCommand> forget(std::vector<SavedCommand> list, const SavedCommand& cmd) {
    list.erase(std::remove(list.begin(), list.end(), cmd), list.end());
    return list;
}

std::string tabTitle(const SavedCommand& cmd) {
    if (!cmd.title.empty()) {
        return cmd.title;
    }
    // The first word that isn't a VAR=value assignment, without its directory
    size_t i = 0;
    const std::string& s = cmd.command;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') {
            i++;
        }
        const size_t end = std::min(s.find(' ', i), s.size());
        std::string word = s.substr(i, end - i);
        i = end;
        if (word.empty() || word.find('=') != std::string::npos) {
            continue;
        }
        const auto slash = word.rfind('/');
        return slash == std::string::npos ? word : word.substr(slash + 1);
    }
    return "Shell";
}

std::string expandFolder(const std::string& folder) {
    if (folder == "~") {
        return g_get_home_dir();
    }
    if (folder.rfind("~/", 0) == 0) {
        return std::string(g_get_home_dir()) + folder.substr(1);
    }
    return folder;
}

}  // namespace xoj::assistant::terminal_commands
