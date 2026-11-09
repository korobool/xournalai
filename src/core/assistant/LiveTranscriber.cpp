#include "LiveTranscriber.h"

#include <csignal>  // for SIGTERM
#include <cstdio>   // for FILE, fopen
#include <memory>   // for unique_ptr
#include <utility>  // for move
#include <vector>   // for vector

#include "mcp/Json.h"  // for json

namespace xoj::assistant {

using json = nlohmann::json;

struct LiveTranscriber::State {
    LiveTranscriber* owner = nullptr;  ///< null once the owner is gone: pending callbacks only clean up
    GCancellable* cancel = g_cancellable_new();
    // stream
    GSubprocess* proc = nullptr;
    GOutputStream* in = nullptr;
    GDataInputStream* out = nullptr;
    GDataInputStream* err = nullptr;
    // file
    GSubprocess* fileProc = nullptr;
    std::string fileId;

    void dropStream() {
        g_clear_object(&in);
        g_clear_object(&out);
        g_clear_object(&err);
        g_clear_object(&proc);
    }
    ~State() {
        dropStream();
        g_clear_object(&fileProc);
        g_clear_object(&cancel);
    }
};

namespace {
using Hold = std::shared_ptr<LiveTranscriber::State>;

std::string shell() {
    const char* s = g_getenv("SHELL");
    return s && *s ? s : "/bin/bash";
}

/// Through the owner's login shell, like the terminal's tabs (the app started from the desktop lacks their PATH)
GSubprocess* spawn(const std::string& command, GSubprocessFlags flags, GError** error) {
    const std::string sh = shell();
    const char* argv[] = {sh.c_str(), "-l", "-c", command.c_str(), nullptr};
    return g_subprocess_newv(argv, flags, error);
}

std::string replaceAll(std::string s, const std::string& what, const std::string& with) {
    for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + with.size())) {
        s.replace(at, what.size(), with);
    }
    return s;
}

std::string shellQuoted(const std::string& s) {
    gchar* q = g_shell_quote(s.c_str());
    std::string out = q;
    g_free(q);
    return out;
}

std::string trimmed(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return {};
    }
    return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}
}  // namespace

LiveTranscriber::LiveTranscriber(TranscriberConfig config, OnResult onResult, OnStatus onStatus):
        config(std::move(config)),
        onResult(std::move(onResult)),
        onStatus(std::move(onStatus)),
        state(std::make_shared<State>()) {
    state->owner = this;
}

LiveTranscriber::~LiveTranscriber() {
    state->owner = nullptr;
    g_cancellable_cancel(state->cancel);
    for (auto& [id, timer]: inFlight) {
        g_source_remove(timer);
    }
    for (guint* t: {&helloTimer, &finishTimer}) {
        if (*t) {
            g_source_remove(*t);
        }
    }
    if (state->in) {
        g_output_stream_close(state->in, nullptr, nullptr);  // (a stream recognizer ends at the end of its input)
    }
    if (state->proc) {
        g_subprocess_send_signal(state->proc, SIGTERM);
    }
    if (state->fileProc) {
        g_subprocess_force_exit(state->fileProc);
    }
}

void LiveTranscriber::submit(Piece piece) {
    if (!config.valid()) {
        Result r;
        r.id = piece.id;
        r.error = "no speech recognizer set (assistant.live_notes.transcriber)";
        answer(std::move(r));
        return;
    }
    queue.push_back(std::move(piece));
    if (config.kind == "file") {
        runNextFile();
    } else {
        if (!state->proc) {
            startStream();
        }
        pumpStream();
    }
}

void LiveTranscriber::finish() {
    finished = true;
    if (!finishTimer) {
        finishTimer = g_timeout_add_seconds(TIMEOUT_S, +[](gpointer d) -> gboolean {
            auto* self = static_cast<LiveTranscriber*>(d);
            self->finishTimer = 0;
            self->failAll("no answer in time");
            return G_SOURCE_REMOVE;
        }, this);
    }
    closeIfDone();
}

void LiveTranscriber::answer(Result r) {
    if (auto it = inFlight.find(r.id); it != inFlight.end()) {
        g_source_remove(it->second);
        inFlight.erase(it);
    }
    if (onResult) {
        onResult(r);
    }
    closeIfDone();
}

void LiveTranscriber::failAll(const std::string& why) {
    std::vector<std::string> ids;
    for (const auto& [id, timer]: inFlight) {
        ids.push_back(id);
    }
    for (const auto& p: queue) {
        ids.push_back(p.id);
    }
    queue.clear();
    for (const auto& id: ids) {
        Result r;
        r.id = id;
        r.error = why;
        answer(std::move(r));
    }
    if (onStatus && !ids.empty()) {
        onStatus("problem: " + why);
    }
}

void LiveTranscriber::armTimeout(const std::string& id) {
    struct T {
        LiveTranscriber* self;
        std::string id;
    };
    inFlight[id] = g_timeout_add_seconds_full(
            G_PRIORITY_DEFAULT, TIMEOUT_S,
            +[](gpointer d) -> gboolean {
                auto* t = static_cast<T*>(d);
                t->self->inFlight.erase(t->id);  // (this source ends here)
                if (t->self->state->fileProc && t->self->state->fileId == t->id) {
                    g_subprocess_force_exit(t->self->state->fileProc);  // (its answer becomes the error)
                    return G_SOURCE_REMOVE;
                }
                Result r;
                r.id = t->id;
                r.error = "no answer in " + std::to_string(TIMEOUT_S) + " s";
                t->self->answer(std::move(r));
                return G_SOURCE_REMOVE;
            },
            new T{this, id}, +[](gpointer d) { delete static_cast<T*>(d); });
}

void LiveTranscriber::closeIfDone() {
    if (!finished || waiting() > 0 || closed) {
        return;
    }
    closed = true;
    if (finishTimer) {
        g_source_remove(finishTimer);
        finishTimer = 0;
    }
    if (state->in && !writing) {
        g_output_stream_close(state->in, nullptr, nullptr);  // (the recognizer exits)
    }
}

// ---- file: one run per piece ----

void LiveTranscriber::runNextFile() {
    if (state->fileProc || queue.empty()) {
        return;
    }
    Piece p = std::move(queue.front());
    queue.pop_front();
    std::string cmd = replaceAll(config.command, "{audio}", shellQuoted(p.audioPath));
    cmd = replaceAll(cmd, "{language}", shellQuoted(p.language));
    cmd = replaceAll(cmd, "{prompt}", shellQuoted(p.prompt));
    GError* error = nullptr;
    state->fileProc = spawn(cmd, static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                               G_SUBPROCESS_FLAGS_STDERR_PIPE),
                            &error);
    if (!state->fileProc) {
        Result r;
        r.id = p.id;
        r.error = error ? error->message : "could not start the recognizer";
        g_clear_error(&error);
        answer(std::move(r));
        runNextFile();
        return;
    }
    state->fileId = p.id;
    armTimeout(p.id);
    const gint64 started = g_get_monotonic_time();
    struct C {
        Hold hold;
        std::string id;
        gint64 started;
    };
    g_subprocess_communicate_utf8_async(
            state->fileProc, nullptr, state->cancel,
            +[](GObject* src, GAsyncResult* res, gpointer d) {
                std::unique_ptr<C> c(static_cast<C*>(d));
                gchar* out = nullptr;
                gchar* err = nullptr;
                GError* error = nullptr;
                const bool talked = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, &err, &error);
                LiveTranscriber* self = c->hold->owner;
                if (self) {
                    Result r;
                    r.id = c->id;
                    r.elapsed = static_cast<double>(g_get_monotonic_time() - c->started) / G_USEC_PER_SEC;
                    if (talked && g_subprocess_get_successful(G_SUBPROCESS(src))) {
                        r.ok = true;
                        r.text = trimmed(out ? out : "");
                    } else {
                        std::string e = trimmed(err ? err : "");
                        if (const auto nl = e.find_last_of('\n'); nl != std::string::npos) {
                            e = e.substr(nl + 1);  // (the last line says what went wrong)
                        }
                        r.error = !e.empty() ? e : error ? error->message : "the recognizer failed";
                    }
                    g_clear_object(&self->state->fileProc);
                    self->state->fileId.clear();
                    self->answer(std::move(r));
                    self->runNextFile();
                }
                g_free(out);
                g_free(err);
                g_clear_error(&error);
            },
            new C{state, p.id, started});
}

// ---- stream: one process, JSON lines ----

namespace {
struct LineReader {
    Hold hold;
    bool isStdout;
};

void readNextLine(GDataInputStream* s, LineReader* r);

void onLine(GObject* src, GAsyncResult* res, gpointer d) {
    auto* r = static_cast<LineReader*>(d);
    gsize len = 0;
    GError* error = nullptr;
    gchar* line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, &len, &error);
    LiveTranscriber* self = r->hold->owner;
    if (!line || !self) {  // (end of the stream, an error, or the owner is gone)
        g_clear_error(&error);
        g_free(line);
        delete r;
        return;
    }
    self->lineFrom(r->isStdout, line);
    g_free(line);
    readNextLine(G_DATA_INPUT_STREAM(src), r);
}

void readNextLine(GDataInputStream* s, LineReader* r) {
    g_data_input_stream_read_line_async(s, G_PRIORITY_DEFAULT, r->hold->cancel, onLine, r);
}
}  // namespace

void LiveTranscriber::lineFrom(bool isStdout, const std::string& line) {
    if (isStdout) {
        onStreamLine(line);
    } else {
        lastStderr = line;
        g_message("live transcriber: %s", line.c_str());
    }
}

void LiveTranscriber::startStream() {
    GError* error = nullptr;
    state->dropStream();
    ready = false;
    closed = false;
    state->proc = spawn(config.command,
                        static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                      G_SUBPROCESS_FLAGS_STDERR_PIPE),
                        &error);
    if (!state->proc) {
        const std::string why = error ? error->message : "could not start the recognizer";
        g_clear_error(&error);
        failAll(why);
        return;
    }
    state->in = G_OUTPUT_STREAM(g_object_ref(g_subprocess_get_stdin_pipe(state->proc)));
    state->out = g_data_input_stream_new(g_subprocess_get_stdout_pipe(state->proc));
    state->err = g_data_input_stream_new(g_subprocess_get_stderr_pipe(state->proc));
    readNextLine(state->out, new LineReader{state, true});
    readNextLine(state->err, new LineReader{state, false});
    g_subprocess_wait_async(
            state->proc, state->cancel,
            +[](GObject* src, GAsyncResult* res, gpointer d) {
                std::unique_ptr<Hold> hold(static_cast<Hold*>(d));
                g_subprocess_wait_finish(G_SUBPROCESS(src), res, nullptr);
                LiveTranscriber* self = (*hold)->owner;
                if (!self || (*hold)->proc != G_SUBPROCESS(src)) {
                    return;  // (gone, or an earlier process)
                }
                const bool wasReady = self->ready;
                self->ready = false;
                (*hold)->dropStream();
                self->writes.clear();
                self->writing = false;
                if (self->helloTimer) {
                    g_source_remove(self->helloTimer);
                    self->helloTimer = 0;
                }
                // the pieces it had: failed; the ones still queued go to a fresh process (if it had worked at all)
                std::vector<std::string> lost;
                for (const auto& [id, timer]: self->inFlight) {
                    lost.push_back(id);
                }
                const std::string why = "the recognizer ended" +
                                        (self->lastStderr.empty() ? std::string() : ": " + self->lastStderr);
                for (const auto& id: lost) {
                    Result r;
                    r.id = id;
                    r.error = why;
                    self->answer(std::move(r));
                }
                if (!self->queue.empty()) {
                    if (wasReady) {
                        self->startStream();
                    } else {
                        self->failAll(why);
                    }
                }
            },
            new Hold(state));
    helloTimer = g_timeout_add_seconds(TIMEOUT_S, +[](gpointer d) -> gboolean {
        auto* self = static_cast<LiveTranscriber*>(d);
        self->helloTimer = 0;
        if (!self->ready) {
            self->failAll("the recognizer did not get ready in " + std::to_string(TIMEOUT_S) + " s" +
                          (self->lastStderr.empty() ? std::string() : " (" + self->lastStderr + ")"));
            if (self->state->proc) {
                g_subprocess_send_signal(self->state->proc, SIGTERM);
            }
        }
        return G_SOURCE_REMOVE;
    }, this);
    if (onStatus) {
        onStatus("starting the recognizer…");
    }
}

void LiveTranscriber::onStreamLine(const std::string& line) {
    json j;
    try {
        j = json::parse(line);
    } catch (const std::exception&) {
        g_message("live transcriber (not JSON): %s", line.c_str());
        return;
    }
    if (!j.is_object()) {
        return;
    }
    const std::string type = j.value("type", "");
    if (type == "hello") {
        ready = true;
        engineName = j.value("engine", "");
        inlineAudio = j.value("audio", "path") == "inline";
        if (helloTimer) {
            g_source_remove(helloTimer);
            helloTimer = 0;
        }
        if (j.value("protocol", 0) != PROTOCOL) {
            g_message("live transcriber: protocol %d, this app speaks %d", j.value("protocol", 0), PROTOCOL);
        }
        if (onStatus) {
            onStatus("ready: " + (engineName.empty() ? std::string("recognizer") : engineName));
        }
        pumpStream();
    } else if (type == "result" || type == "error") {
        Result r;
        r.id = j.contains("id") && j["id"].is_string() ? j["id"].get<std::string>() :
               j.contains("id")                         ? j["id"].dump() :
                                                          "";
        if (!inFlight.count(r.id)) {
            return;  // (timed out already, or unknown)
        }
        r.ok = type == "result";
        r.text = trimmed(j.value("text", ""));
        r.language = j.value("language", "");
        r.languageProbability = j.value("language_probability", -1.0);
        r.elapsed = j.value("elapsed", -1.0);
        r.error = type == "error" ? j.value("message", "failed") : "";
        answer(std::move(r));
    }
}

void LiveTranscriber::pumpStream() {
    if (!ready || !state->in) {
        return;
    }
    while (!queue.empty()) {
        Piece p = std::move(queue.front());
        queue.pop_front();
        json req = {{"type", "transcribe"},    {"id", p.id},
                    {"audio_path", p.audioPath}, {"language", p.language},
                    {"language_hint", p.languageHint}, {"prompt", p.prompt}};
        if (inlineAudio) {
            gchar* data = nullptr;
            gsize size = 0;
            if (!g_file_get_contents(p.audioPath.c_str(), &data, &size, nullptr)) {
                Result r;
                r.id = p.id;
                r.error = "could not read " + p.audioPath;
                answer(std::move(r));
                continue;
            }
            gchar* b64 = g_base64_encode(reinterpret_cast<const guchar*>(data), size);
            req["audio_b64"] = b64;
            g_free(b64);
            g_free(data);
        }
        armTimeout(p.id);
        writeLine(req.dump());
    }
}

void LiveTranscriber::writeLine(const std::string& line) {
    writes.push_back(line + "\n");
    writeNext();
}

void LiveTranscriber::writeNext() {
    if (writing || !state->in || writes.empty()) {
        return;
    }
    writing = true;
    g_output_stream_write_all_async(
            state->in, writes.front().data(), writes.front().size(), G_PRIORITY_DEFAULT, state->cancel,
            +[](GObject* src, GAsyncResult* res, gpointer d) {
                std::unique_ptr<Hold> hold(static_cast<Hold*>(d));
                GError* error = nullptr;
                g_output_stream_write_all_finish(G_OUTPUT_STREAM(src), res, nullptr, &error);
                LiveTranscriber* self = (*hold)->owner;
                g_clear_error(&error);  // (a broken pipe: the process's end tells the pieces)
                if (!self || (*hold)->in != G_OUTPUT_STREAM(src)) {
                    return;
                }
                self->writing = false;
                self->writes.pop_front();  // (written: the buffer was alive until now)
                if (!self->writes.empty()) {
                    self->writeNext();
                } else if (self->closed) {
                    g_output_stream_close((*hold)->in, nullptr, nullptr);
                }
            },
            new Hold(state));
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
