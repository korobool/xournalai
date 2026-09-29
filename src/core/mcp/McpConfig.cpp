#include "McpConfig.h"

#include <fstream>  // for ifstream, ofstream
#include <random>   // for random_device

#include <glib.h>  // for g_warning

#include "util/PathUtil.h"  // for getConfigFile

#include "Json.h"
#include "PathText.h"

namespace xoj::mcp {

McpConfig::McpConfig(): tiers{Tier::Read, Tier::Draw, Tier::Ui, Tier::Files} {
    exportDir = fs::path(g_get_user_data_dir()) / "xournalpp" / "mcp-exports";
    backupDir = fs::path(g_get_user_data_dir()) / "xournalpp" / "mcp-backups";
}

McpConfig::Overrides& McpConfig::overrides() {
    static Overrides o;
    return o;
}

fs::path McpConfig::path() { return Util::getConfigFile("mcp.json"); }

std::string McpConfig::url() const { return "http://127.0.0.1:" + std::to_string(port) + "/mcp"; }

std::string McpConfig::generateToken() {
    std::random_device rd;
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 24; i++) {
        const auto b = static_cast<unsigned>(rd()) & 0xffU;
        out += hex[b >> 4];
        out += hex[b & 0xfU];
    }
    return out;
}

McpConfig McpConfig::load() {
    McpConfig cfg;
    bool dirty = true;
    try {
        std::ifstream in(path());
        if (in) {
            json j = json::parse(in);
            cfg.enabled = j.value("enabled", cfg.enabled);
            cfg.port = static_cast<uint16_t>(j.value("port", static_cast<int>(cfg.port)));
            cfg.token = j.value("token", "");
            cfg.defaultLayer = j.value("default_layer", cfg.defaultLayer);
            cfg.animate = j.value("animate", cfg.animate);
            cfg.agentsChooseLayer = j.value("agents_choose_layer", cfg.agentsChooseLayer);
            cfg.backups = j.value("backups", cfg.backups);
            if (j.contains("assistant") && j["assistant"].is_object()) {
                const auto& a = j["assistant"];
                cfg.assistant.autostart = a.value("autostart", cfg.assistant.autostart);
                cfg.assistant.agent = a.value("agent", cfg.assistant.agent);
                cfg.assistant.permissionMode = a.value("permission_mode", cfg.assistant.permissionMode);
                cfg.assistant.command = a.value("command", cfg.assistant.command);
                cfg.assistant.autoImprove = a.value("auto_improve", cfg.assistant.autoImprove);
                if (a.contains("rules") && a["rules"].is_array()) {
                    cfg.assistant.rules.clear();
                    for (const auto& r: a["rules"]) {
                        if (r.is_string()) {
                            cfg.assistant.rules.push_back(r.get<std::string>());
                        }
                    }
                }
                cfg.assistant.wakeIdleMs = a.value("wake_idle_ms", cfg.assistant.wakeIdleMs);
                cfg.assistant.watchdogS = a.value("watchdog_s", cfg.assistant.watchdogS);
                cfg.assistant.maxParallel = a.value("max_parallel", cfg.assistant.maxParallel);
            }
            if (j.contains("backup_dir") && j["backup_dir"].is_string() &&
                !j["backup_dir"].get<std::string>().empty()) {
                cfg.backupDir = pathFromUtf8(j["backup_dir"].get<std::string>());
            }
            if (j.contains("export_dir") && j["export_dir"].is_string() &&
                !j["export_dir"].get<std::string>().empty()) {
                cfg.exportDir = pathFromUtf8(j["export_dir"].get<std::string>());
            }
            if (j.contains("permissions") && j["permissions"].is_object()) {
                cfg.tiers.clear();
                for (const auto& [name, granted]: j["permissions"].items()) {
                    auto t = tierFromName(name);
                    if (t && granted.is_boolean() && granted.get<bool>()) {
                        cfg.tiers.insert(*t);
                    }
                }
            }
            dirty = cfg.token.empty();
        }
    } catch (const std::exception& e) {
        g_warning("Could not read %s: %s (using defaults)", toUtf8(path()).c_str(), e.what());
    }
    if (cfg.token.empty()) {
        cfg.token = generateToken();
    }
    if (dirty) {
        cfg.save();
    }
    // Command line overrides are for this run only and are never written to the file
    if (overrides().enabled) {
        cfg.enabled = *overrides().enabled;
    }
    if (overrides().port) {
        cfg.port = *overrides().port;
    }
    return cfg;
}

void McpConfig::save() const {
    json permissions = json::object();
    for (Tier t: {Tier::Read, Tier::Draw, Tier::Ui, Tier::Files, Tier::Destructive}) {
        permissions[tierName(t)] = allows(t);
    }
    json j = {{"enabled", enabled},
              {"port", port},
              {"token", token},
              {"permissions", permissions},
              {"default_layer", defaultLayer},
              {"animate", animate},
              {"agents_choose_layer", agentsChooseLayer},
              {"export_dir", toUtf8(exportDir)},
              {"backups", backups},
              {"assistant",
               {{"autostart", assistant.autostart},
                {"agent", assistant.agent},
                {"permission_mode", assistant.permissionMode},
                {"command", assistant.command},
                {"auto_improve", assistant.autoImprove},
                {"rules", assistant.rules},
                {"wake_idle_ms", assistant.wakeIdleMs},
                {"watchdog_s", assistant.watchdogS},
                {"max_parallel", assistant.maxParallel}}},
              {"backup_dir", toUtf8(backupDir)},
              {"_help",
               {{"url", url()},
                {"header", "Authorization: Bearer " + token},
                {"claude_code", "claude mcp add --transport http xournalai " + url() +
                                        " --header \"Authorization: Bearer " + token + "\""},
                {"stdio", "xournalpp --mcp-stdio  (for clients that only launch stdio servers)"},
                {"permissions", "read: inspect/render; draw: create/edit content; ui: menus, dialogs, tools, view; "
                                "files: open/save/export/import; destructive: discard unsaved changes, overwrite "
                                "files, close without saving"}}}};
    try {
        fs::create_directories(path().parent_path());
        {
            std::ofstream out(path());
            out << j.dump(2) << "\n";
        }
        fs::permissions(path(), fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace);
    } catch (const std::exception& e) {
        g_warning("Could not write %s: %s", toUtf8(path()).c_str(), e.what());
    }
}

}  // namespace xoj::mcp
