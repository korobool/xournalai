// Tool: audio_transcribe (the local, English-only fallback for recordings; the owner's remote transcriber comes first)

#include <fstream>  // for ofstream, ifstream
#include <memory>   // for unique_ptr
#include <sstream>  // for stringstream

#include <gio/gio.h>

#include "assistant/SpeechToText.h"     // for SpeechToText
#include "control/Control.h"            // for Control
#include "control/settings/Settings.h"  // for Settings
#include "mcp/McpServer.h"
#include "mcp/Schema.h"

#include "ToolUtil.h"
#include "Tools.h"
#include "config-features.h"  // for ENABLE_STT

namespace xoj::mcp::tools {

namespace {
std::string readFile(const fs::path& p) {
    std::ifstream in(p);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

std::string clock(double seconds) {
    const auto s = static_cast<long>(seconds);
    char b[16];
    snprintf(b, sizeof b, "%02ld:%02ld", s / 60, s % 60);
    return b;
}
}  // namespace

void registerAudioTools(McpServer& server) {
#ifdef ENABLE_STT
    Control* ctrl = server.getControl();
    McpServer* srv = &server;
    ToolSpec t;
    t.name = "audio_transcribe";
    t.title = "Transcribe a recording (local fallback)";
    t.description =
            "Transcribes an audio recording on this laptop (whisper.cpp, English only, timestamped segments). A "
            "fallback: use the user's remote transcriber first when it is available (long or non-English audio). "
            "Transcripts are kept in the recordings folder's transcripts/ (<name>.local.txt / .local.json); an "
            "existing transcript (also the remote one, <name>.txt) is returned unless force=true. Long recordings "
            "take about a tenth of their length.";
    t.inputSchema = schema::object({{"file", schema::string("The recording (e.g. from an audio_recorded event)")},
                                    {"force", schema::withDefault(schema::boolean("Transcribe again"), false)}},
                                   {"file"});
    t.tier = Tier::Files;
    t.asyncHandler = [ctrl, srv](const json& j, Responder respond) {
        Args args(j);
        args.rejectUnknown({"file", "force"});
        const fs::path file = args.str("file");
        if (!fs::exists(file)) {
            throw ToolError("No such recording: " + file.string());
        }
        fs::path folder = ctrl->getSettings()->getAudioFolder();
        if (folder.empty()) {
            folder = file.parent_path();
        }
        const fs::path dir = folder / "transcripts";
        const std::string stem = file.stem().string();
        const fs::path remoteTxt = dir / (stem + ".txt"), localTxt = dir / (stem + ".local.txt"),
                       localJson = dir / (stem + ".local.json");
        if (!args.boolean("force", false)) {
            if (fs::exists(remoteTxt)) {
                respond(ToolResult::structured({{"source", "remote transcript (already there)"},
                                                {"transcript_file", remoteTxt.string()},
                                                {"text", readFile(remoteTxt)}}));
                return;
            }
            if (fs::exists(localJson)) {
                json r = json::parse(readFile(localJson), nullptr, false);
                if (r.is_object()) {
                    r["source"] = "local transcript (already there)";
                    r["transcript_file"] = localTxt.string();
                    respond(ToolResult::structured(r));
                    return;
                }
            }
        }
        const std::string helper = assistant::SpeechToText::helperPath();
        const std::string model = (fs::path(assistant::SpeechToText::modelsDir()) /
                                   ("ggml-" + srv->getConfig().assistant.speechModel + ".bin"))
                                          .string();
        if (helper.empty() || !fs::exists(model)) {
            throw ToolError("The local transcriber is not available (xournalai-stt or its model " + model + ")");
        }
        // Its own helper process: a long recording must not hold up the user's Ask
        GError* err = nullptr;
        GSubprocess* proc = g_subprocess_new(
                static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                              G_SUBPROCESS_FLAGS_STDERR_SILENCE),
                &err, helper.c_str(), "--model", model.c_str(), nullptr);
        if (!proc) {
            std::string why = err ? err->message : "?";
            g_clear_error(&err);
            throw ToolError("Could not start the local transcriber: " + why);
        }
        const std::string input =
                json({{"cmd", "transcribe_file"}, {"path", file.string()}}).dump() + "\n" + R"({"cmd":"quit"})" + "\n";
        struct Ctx {
            Responder respond;
            fs::path dir, localTxt, localJson, file;
        };
        g_subprocess_communicate_utf8_async(
                proc, input.c_str(), nullptr,
                +[](GObject* src, GAsyncResult* res, gpointer data) {
                    std::unique_ptr<Ctx> c(static_cast<Ctx*>(data));
                    gchar* out = nullptr;
                    GError* err = nullptr;
                    g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, nullptr, &err);
                    g_object_unref(src);
                    std::string text = out ? out : "";
                    g_free(out);
                    g_clear_error(&err);
                    json result;
                    std::stringstream lines(text);
                    for (std::string line; std::getline(lines, line);) {
                        json e = json::parse(line, nullptr, false);
                        if (e.is_object() &&
                            (e.value("event", "") == "transcript" || e.value("event", "") == "error")) {
                            result = e;
                        }
                    }
                    if (!result.is_object() || result.value("event", "") != "transcript") {
                        c->respond(ToolResult::error("The local transcriber failed: " +
                                                     (result.is_object() ? result.value("message", "?") : text)));
                        return;
                    }
                    result.erase("event");
                    result["source"] = "local (whisper.cpp, English)";
                    result["file"] = c->file.string();
                    std::error_code ec;
                    fs::create_directories(c->dir, ec);
                    std::ofstream txt(c->localTxt);
                    for (const auto& s: result["segments"]) {
                        txt << "[" << clock(s.value("start", 0.0)) << "] " << s.value("text", "") << "\n";
                    }
                    std::ofstream(c->localJson) << result.dump(1) << "\n";
                    result["transcript_file"] = c->localTxt.string();
                    c->respond(ToolResult::structured(result));
                },
                new Ctx{std::move(respond), dir, localTxt, localJson, file});
    };
    server.getRegistry().addTool(std::move(t));
#else
    (void)server;
#endif
}

}  // namespace xoj::mcp::tools
