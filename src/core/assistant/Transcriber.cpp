#include "Transcriber.h"

#include <algorithm>  // for clamp
#include <csignal>    // for kill
#include <cstdio>     // for FILE, fopen
#include <cstdlib>    // for strtol

#include <unistd.h>  // for setsid

#include "TranscriberKinds.h"

namespace xoj::assistant {

using json = nlohmann::json;

std::string jsonString(const json& j, const char* key, const std::string& fallback) {
    if (j.is_object()) {
        if (auto it = j.find(key); it != j.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }
    return fallback;
}

double jsonNumber(const json& j, const char* key, double fallback) {
    if (j.is_object()) {
        if (auto it = j.find(key); it != j.end() && it->is_number()) {
            return it->get<double>();
        }
    }
    return fallback;
}

TranscriberSettings TranscriberSettings::from(const json& config) {
    TranscriberSettings s;
    s.command = jsonString(config, "command");
    s.timeoutS = static_cast<guint>(std::clamp(jsonNumber(config, "timeout_s", s.timeoutS), 1.0, 3600.0));
    s.readyTimeoutS =
            static_cast<guint>(std::clamp(jsonNumber(config, "ready_timeout_s", s.readyTimeoutS), 1.0, 3600.0));
    return s;
}

std::unique_ptr<Transcriber> Transcriber::make(const json& config, TranscriberHooks hooks, std::string& error) {
    const std::string kind = jsonString(config, "kind");
    if (jsonString(config, "command").empty()) {
        error = "no speech recognizer set (assistant.live_notes.transcriber in mcp.json)";
        return nullptr;
    }
    if (kind == "file") {
        return makeFileTranscriber(config, std::move(hooks));
    }
    if (kind == "stream") {
        return makeStreamTranscriber(config, std::move(hooks));
    }
    error = "unknown speech recognizer kind \"" + kind + "\" (file or stream)";
    return nullptr;
}

GSubprocess* spawnRecognizer(const std::string& command, GSubprocessFlags flags, GError** error) {
    const char* s = g_getenv("SHELL");
    const std::string sh = s && *s ? s : "/bin/bash";
    GSubprocessLauncher* launcher = g_subprocess_launcher_new(flags);
    g_subprocess_launcher_set_child_setup(launcher, +[](gpointer) { setsid(); }, nullptr, nullptr);
    GSubprocess* p = g_subprocess_launcher_spawn(launcher, error, sh.c_str(), "-l", "-c", command.c_str(), nullptr);
    g_object_unref(launcher);
    return p;
}

void signalRecognizer(GSubprocess* process, int signal) {
    const char* id = process ? g_subprocess_get_identifier(process) : nullptr;  // (null once it has exited)
    if (!id) {
        return;
    }
    const pid_t pid = static_cast<pid_t>(strtol(id, nullptr, 10));
    if (pid > 1) {
        kill(-pid, signal);  // the group: the shell and what it started
    }
}

bool writeWav16k(const std::string& path, const int16_t* samples, size_t count) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    auto u32 = [f](uint32_t v) {
        const unsigned char b[4] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8),
                                    static_cast<unsigned char>(v >> 16), static_cast<unsigned char>(v >> 24)};
        fwrite(b, 1, 4, f);
    };
    auto u16 = [f](uint16_t v) {
        const unsigned char b[2] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8)};
        fwrite(b, 1, 2, f);
    };
    const auto bytes = static_cast<uint32_t>(count * 2);
    fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);  // PCM
    u16(1);  // mono
    u32(16000);
    u32(16000 * 2);
    u16(2);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(bytes);
    for (size_t i = 0; i < count; i++) {  // (little-endian, whatever the machine)
        u16(static_cast<uint16_t>(samples[i]));
    }
    return fclose(f) == 0;
}

}  // namespace xoj::assistant
