// The "stream" kind of recognizer (docs/mcp/TRANSCRIBERS.md): one long-running process speaking JSON lines (hello,
// transcribe → result / error). At most `max_in_flight` pieces are sent at a time (1 unless its hello says more), so a
// piece's timeout counts its own recognition, never the wait behind others; a piece that times out means the
// recognizer may be stuck: it is ended, and a fresh one takes the rest. The process is a process group of its
// own: at the end its input is closed, and if it doesn't exit it is ended with what it started.

#include <algorithm>  // for clamp
#include <csignal>    // for SIGKILL, SIGTERM
#include <deque>    // for deque
#include <map>      // for map
#include <memory>   // for shared_ptr
#include <set>      // for set
#include <utility>  // for move
#include <vector>   // for vector

#include "TranscriberKinds.h"

namespace xoj::assistant {

namespace {
using json = nlohmann::json;

class StreamTranscriber final: public Transcriber {
public:
    StreamTranscriber(TranscriberSettings settings, TranscriberHooks hooks):
            settings(std::move(settings)), hooks(std::move(hooks)) {}

    ~StreamTranscriber() override {
        *token = nullptr;
        g_cancellable_cancel(cancel);
        g_object_unref(cancel);
        for (auto& [id, t]: inFlight) {
            g_source_remove(t);
        }
        for (guint t: {readyTimer, endTimer}) {
            if (t) {
                g_source_remove(t);
            }
        }
        if (proc) {
            if (in) {
                g_output_stream_close(in, nullptr, nullptr);
            }
            signalRecognizer(proc, SIGTERM);
        }
        dropProcess();
    }

    void submit(TranscriberPiece piece) override {
        pending.insert(piece.id);
        queue.push_back(std::move(piece));
        if (!proc) {
            start();
        }
        pump();
    }

    void finish() override {
        finished = true;
        closeIfDone();
    }

    size_t waiting() const override { return pending.size(); }
    std::string engine() const override { return engineName; }
    std::string voicesModel() const override { return voicesName; }

private:
    // ---- the process ----

    void start() {
        GError* error = nullptr;
        ready = false;
        closed = false;
        maxInFlight = 1;
        proc = spawnRecognizer(settings.command,
                               static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                                             G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                                             G_SUBPROCESS_FLAGS_STDERR_PIPE),
                               &error);
        if (!proc) {
            const std::string why = error ? error->message : "could not start the recognizer";
            g_clear_error(&error);
            failQueued(why);
            return;
        }
        in = G_OUTPUT_STREAM(g_object_ref(g_subprocess_get_stdin_pipe(proc)));
        out = g_data_input_stream_new(g_subprocess_get_stdout_pipe(proc));
        err = g_data_input_stream_new(g_subprocess_get_stderr_pipe(proc));
        readLine(out, true);
        readLine(err, false);
        struct Wait {
            std::shared_ptr<StreamTranscriber*> token;
            GSubprocess* proc;
        };
        g_subprocess_wait_async(
                proc, cancel,
                +[](GObject* src, GAsyncResult* res, gpointer d) {
                    std::unique_ptr<Wait> w(static_cast<Wait*>(d));
                    g_subprocess_wait_finish(G_SUBPROCESS(src), res, nullptr);
                    StreamTranscriber* self = *w->token;
                    if (self && self->proc == w->proc) {
                        self->onExit();
                    }
                },
                new Wait{token, proc});
        readyTimer = g_timeout_add_seconds(settings.readyTimeoutS, &StreamTranscriber::onReadyTimeout, this);
        status("starting the recognizer…");
    }

    void dropProcess() {
        g_clear_object(&in);
        g_clear_object(&out);
        g_clear_object(&err);
        g_clear_object(&proc);
        writes.clear();  // (each write owns its own copy: nothing in flight points here)
        writing = false;
    }

    void onExit() {
        const bool wasReady = ready || restarting;
        restarting = false;
        ready = false;
        if (readyTimer) {
            g_source_remove(readyTimer);
            readyTimer = 0;
        }
        if (endTimer) {
            g_source_remove(endTimer);
            endTimer = 0;
        }
        dropProcess();
        const std::string why =
                "the recognizer ended" + (lastStderr.empty() ? std::string() : ": " + lastStderr);
        std::vector<std::string> lost;
        for (const auto& [id, t]: inFlight) {
            lost.push_back(id);
        }
        for (const auto& id: lost) {
            fail(id, why);
        }
        if (!queue.empty()) {
            if (wasReady) {
                start();  // (it had worked: a fresh one for the rest)
                pump();
            } else {
                failQueued(why + " (before it was ready)");
            }
        }
    }

    static gboolean onReadyTimeout(gpointer d) {
        auto* self = static_cast<StreamTranscriber*>(d);
        self->readyTimer = 0;
        if (!self->ready) {
            const std::string why = "the recognizer did not get ready in " +
                                    std::to_string(self->settings.readyTimeoutS) + " s" +
                                    (self->lastStderr.empty() ? std::string() : " (" + self->lastStderr + ")");
            self->failQueued(why);
            signalRecognizer(self->proc, SIGKILL);  // (its exit fails nothing more: nothing was sent)
        }
        return G_SOURCE_REMOVE;
    }

    void closeIfDone() {
        if (!finished || !pending.empty() || closed || !proc) {
            return;
        }
        closed = true;
        if (!writing && in) {
            g_output_stream_close(in, nullptr, nullptr);  // (it exits at the end of its input)
        }
        // …and if it doesn't, it is ended (a hung worker must not keep its GPU)
        endTimer = g_timeout_add_seconds(10, +[](gpointer d) -> gboolean {
            auto* self = static_cast<StreamTranscriber*>(d);
            self->endTimer = 0;
            signalRecognizer(self->proc, self->endSignal);
            if (self->endSignal == SIGTERM) {
                self->endSignal = SIGKILL;
                self->endTimer = g_timeout_add_seconds(5, +[](gpointer d) -> gboolean {
                    auto* self = static_cast<StreamTranscriber*>(d);
                    self->endTimer = 0;
                    signalRecognizer(self->proc, SIGKILL);
                    return G_SOURCE_REMOVE;
                }, self);
            }
            return G_SOURCE_REMOVE;
        }, this);
    }

    // ---- lines from it ----

    void readLine(GDataInputStream* s, bool isStdout) {
        struct Reader {
            std::shared_ptr<StreamTranscriber*> token;
            bool isStdout;
        };
        g_data_input_stream_read_line_async(
                s, G_PRIORITY_DEFAULT, cancel,
                +[](GObject* src, GAsyncResult* res, gpointer d) {
                    std::unique_ptr<Reader> r(static_cast<Reader*>(d));
                    gsize len = 0;
                    GError* error = nullptr;
                    gchar* line =
                            g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, &len, &error);
                    g_clear_error(&error);
                    StreamTranscriber* self = *r->token;
                    if (!line || !self) {  // (the end of the stream, or this is gone)
                        g_free(line);
                        return;
                    }
                    const std::string text = line;
                    g_free(line);
                    if (G_DATA_INPUT_STREAM(src) != (r->isStdout ? self->out : self->err)) {
                        return;  // (an earlier process)
                    }
                    r->isStdout ? self->onLine(text) : self->onStderr(text);
                    self->readLine(G_DATA_INPUT_STREAM(src), r->isStdout);
                },
                new Reader{token, isStdout});
    }

    void onStderr(const std::string& line) {
        lastStderr = line;
        g_message("speech recognizer: %s", line.c_str());
    }

    void onLine(const std::string& line) {
        // Anything a recognizer writes is read defensively: a wrong type or a null never throws out of here
        json j = json::parse(line, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            g_message("speech recognizer (not a JSON object): %s", line.c_str());
            return;
        }
        const std::string type = jsonString(j, "type");
        if (type == "hello") {
            onHello(j);
        } else if (type == "result" || type == "error") {
            std::string id = jsonString(j, "id");
            if (id.empty() && j.contains("id") && j["id"].is_number()) {
                id = std::to_string(j["id"].get<long long>());
            }
            if (!inFlight.count(id)) {
                return;  // (timed out already, or never asked)
            }
            TranscriberResult r;
            r.id = id;
            r.ok = type == "result";
            r.text = jsonString(j, "text");
            r.language = jsonString(j, "language");
            r.languageProbability = jsonNumber(j, "language_probability", -1);
            r.elapsed = jsonNumber(j, "elapsed", -1);
            if (!r.ok) {
                r.error = jsonString(j, "message", "the recognizer failed");
            }
            if (j.contains("segments") && j["segments"].is_array()) {
                for (const auto& s: j["segments"]) {
                    if (!s.is_object()) {
                        continue;
                    }
                    TranscriberSegment seg;
                    seg.start = jsonNumber(s, "start", 0);
                    seg.end = jsonNumber(s, "end", seg.start);
                    seg.text = jsonString(s, "text");
                    seg.voice = voiceOf(s);
                    r.segments.push_back(std::move(seg));
                }
            }
            r.voice = voiceOf(j);
            r.voicesError = jsonString(j, "voices_error");
            answer(std::move(r));
        }
    }

    /// A fingerprint as the hello said (its length); anything else is none
    Voice voiceOf(const json& j) const {
        Voice v;
        if (voicesDim <= 0 || !j.contains("voice") || !j["voice"].is_array() ||
            j["voice"].size() != static_cast<size_t>(voicesDim)) {
            return v;
        }
        v.reserve(static_cast<size_t>(voicesDim));
        for (const auto& x: j["voice"]) {
            if (!x.is_number()) {
                return {};
            }
            v.push_back(x.get<float>());
        }
        return v;
    }

    void onHello(const json& j) {
        ready = true;
        engineName = jsonString(j, "engine");
        if (j.contains("voices") && j["voices"].is_object()) {  // (1.23.0: it can give voice fingerprints)
            voicesName = jsonString(j["voices"], "model");
            voicesDim = static_cast<int>(jsonNumber(j["voices"], "dim", 0));
            if (voicesName.empty() || voicesDim <= 0 || voicesDim > 4096) {
                voicesName.clear();
                voicesDim = 0;
            }
        }
        inlineAudio = jsonString(j, "audio", "path") == "inline";
        maxInFlight = static_cast<size_t>(std::clamp(jsonNumber(j, "max_in_flight", 1), 1.0, 16.0));
        const int protocol = static_cast<int>(jsonNumber(j, "protocol", 0));
        if (protocol != PROTOCOL) {
            g_message("speech recognizer: protocol %d; xournalai speaks %d (going on with it)", protocol, PROTOCOL);
        }
        if (readyTimer) {
            g_source_remove(readyTimer);
            readyTimer = 0;
        }
        status("ready: " + (engineName.empty() ? std::string("speech recognizer") : engineName));
        pump();
    }

    // ---- pieces ----

    void pump() {
        while (ready && in && !closed && inFlight.size() < maxInFlight && !queue.empty()) {
            TranscriberPiece p = std::move(queue.front());
            queue.pop_front();
            if (hooks.prepare) {
                hooks.prepare(p);  // (the hint and the prompt from the answers so far)
            }
            json req = {{"type", "transcribe"},          {"id", p.id},
                        {"audio_path", p.audioPath},       {"language", p.language},
                        {"language_hint", p.languageHint}, {"prompt", p.prompt}};
            if (p.voices && !voicesName.empty()) {
                req["voices"] = true;
            }
            if (inlineAudio) {
                gchar* data = nullptr;
                gsize size = 0;
                if (!g_file_get_contents(p.audioPath.c_str(), &data, &size, nullptr)) {
                    fail(p.id, "could not read " + p.audioPath);
                    continue;
                }
                gchar* b64 = g_base64_encode(reinterpret_cast<const guchar*>(data), size);
                req["audio_b64"] = b64;
                g_free(b64);
                g_free(data);
            }
            struct T {
                StreamTranscriber* self;
                std::string id;
            };
            inFlight[p.id] = g_timeout_add_seconds_full(
                    G_PRIORITY_DEFAULT, settings.timeoutS,
                    +[](gpointer d) -> gboolean {
                        auto* t = static_cast<T*>(d);
                        StreamTranscriber* self = t->self;
                        self->inFlight.erase(t->id);  // (this source ends here)
                        // It may be stuck (or still on this piece): ended first (nothing more is sent to it), the
                        // rest go to a fresh one (its exit starts it)
                        if (self->proc && !self->closed) {
                            self->restarting = true;
                            self->ready = false;
                            signalRecognizer(self->proc, SIGKILL);
                        }
                        self->fail(t->id, "no answer in " + std::to_string(self->settings.timeoutS) + " s");
                        return G_SOURCE_REMOVE;
                    },
                    new T{this, p.id}, +[](gpointer d) { delete static_cast<T*>(d); });
            write(req.dump() + "\n");
        }
    }

    void write(std::string line) {
        writes.push_back(std::make_shared<std::string>(std::move(line)));
        writeNext();
    }

    void writeNext() {
        if (writing || !in || writes.empty()) {
            return;
        }
        writing = true;
        struct W {
            std::shared_ptr<StreamTranscriber*> token;
            std::shared_ptr<std::string> data;  ///< alive until the write is done, whatever happens meanwhile
            GOutputStream* stream;
        };
        auto* w = new W{token, writes.front(), in};
        writes.pop_front();
        g_output_stream_write_all_async(
                in, w->data->data(), w->data->size(), G_PRIORITY_DEFAULT, cancel,
                +[](GObject* src, GAsyncResult* res, gpointer d) {
                    std::unique_ptr<W> w(static_cast<W*>(d));
                    GError* error = nullptr;
                    g_output_stream_write_all_finish(G_OUTPUT_STREAM(src), res, nullptr, &error);
                    g_clear_error(&error);  // (a broken pipe: its exit tells the pieces)
                    StreamTranscriber* self = *w->token;
                    if (!self || self->in != w->stream) {
                        return;
                    }
                    self->writing = false;
                    if (!self->writes.empty()) {
                        self->writeNext();
                    } else if (self->closed) {
                        g_output_stream_close(self->in, nullptr, nullptr);
                    }
                },
                w);
    }

    void answer(TranscriberResult r) {
        if (auto it = inFlight.find(r.id); it != inFlight.end()) {
            g_source_remove(it->second);
            inFlight.erase(it);
        }
        if (!pending.erase(r.id)) {
            return;
        }
        if (hooks.onResult) {
            hooks.onResult(r);
        }
        pump();
        closeIfDone();
    }

    void fail(const std::string& id, const std::string& why) {
        TranscriberResult r;
        r.id = id;
        r.error = why;
        answer(std::move(r));
    }

    void failQueued(const std::string& why) {
        std::vector<std::string> ids;
        for (const auto& p: queue) {
            ids.push_back(p.id);
        }
        queue.clear();
        for (const auto& id: ids) {
            fail(id, why);
        }
        status("problem: " + why);
    }

    void status(const std::string& s) {
        if (hooks.onStatus) {
            hooks.onStatus(s);
        }
    }

    TranscriberSettings settings;
    TranscriberHooks hooks;
    std::shared_ptr<StreamTranscriber*> token = std::make_shared<StreamTranscriber*>(this);
    GCancellable* cancel = g_cancellable_new();
    GSubprocess* proc = nullptr;
    GOutputStream* in = nullptr;
    GDataInputStream* out = nullptr;
    GDataInputStream* err = nullptr;
    std::deque<std::shared_ptr<std::string>> writes;  ///< lines waiting for the pipe
    bool writing = false;
    bool ready = false;
    bool closed = false;
    bool restarting = false;  ///< ended after a timeout: a fresh one takes the rest
    bool inlineAudio = false;
    size_t maxInFlight = 1;
    int endSignal = SIGTERM;
    guint readyTimer = 0;
    guint endTimer = 0;
    std::deque<TranscriberPiece> queue;     ///< not sent yet
    std::map<std::string, guint> inFlight;  ///< sent: id → its timeout
    std::set<std::string> pending;          ///< submitted, not answered
    std::string engineName;
    std::string voicesName;  ///< (its hello) the model of its voice fingerprints; "" none
    int voicesDim = 0;
    std::string lastStderr;
};
}  // namespace

std::unique_ptr<Transcriber> makeStreamTranscriber(const json& config, TranscriberHooks hooks) {
    return std::make_unique<StreamTranscriber>(TranscriberSettings::from(config), std::move(hooks));
}

}  // namespace xoj::assistant
