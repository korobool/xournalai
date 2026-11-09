// The "file" kind of recognizer (docs/mcp/TRANSCRIBERS.md): a command run once per piece, one at a time; its stdout
// is the text. Each run is a process group of its own: a run that takes too long is ended with what it started
// (ssh, scp), and the next piece goes on.

#include <csignal>  // for SIGKILL, SIGTERM
#include <deque>    // for deque
#include <memory>   // for shared_ptr
#include <set>      // for set
#include <utility>  // for move

#include "TranscriberKinds.h"

namespace xoj::assistant {

namespace {
using json = nlohmann::json;

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
    return b == std::string::npos ? std::string() : s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

class FileTranscriber final: public Transcriber {
public:
    FileTranscriber(TranscriberSettings settings, TranscriberHooks hooks):
            settings(std::move(settings)), hooks(std::move(hooks)), token(std::make_shared<FileTranscriber*>(this)) {}

    ~FileTranscriber() override {
        *token = nullptr;
        if (timer) {
            g_source_remove(timer);
        }
        if (running) {
            signalRecognizer(running, SIGTERM);
            g_object_unref(running);
        }
    }

    void submit(TranscriberPiece piece) override {
        pending.insert(piece.id);
        queue.push_back(std::move(piece));
        runNext();
    }

    void finish() override { finished = true; }  // (each run ends by itself: nothing to let go)
    size_t waiting() const override { return pending.size(); }

private:
    void answer(TranscriberResult r) {
        if (!pending.erase(r.id)) {
            return;  // (answered already: a run that ended after its timeout)
        }
        if (hooks.onResult) {
            hooks.onResult(r);
        }
    }

    void runNext() {
        if (running || queue.empty()) {
            return;
        }
        TranscriberPiece p = std::move(queue.front());
        queue.pop_front();
        if (hooks.prepare) {
            hooks.prepare(p);
        }
        std::string cmd = replaceAll(settings.command, "{audio}", shellQuoted(p.audioPath));
        cmd = replaceAll(cmd, "{language}", shellQuoted(p.language));
        cmd = replaceAll(cmd, "{language_hint}", shellQuoted(p.languageHint));
        cmd = replaceAll(cmd, "{prompt}", shellQuoted(p.prompt));
        cmd = replaceAll(cmd, "{id}", shellQuoted(p.id));
        GError* error = nullptr;
        GSubprocess* proc = spawnRecognizer(
                cmd, static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE),
                &error);
        if (!proc) {
            TranscriberResult r;
            r.id = p.id;
            r.error = error ? error->message : "could not start the recognizer";
            g_clear_error(&error);
            answer(std::move(r));
            runNext();
            return;
        }
        running = proc;
        runningId = p.id;
        struct Run {
            std::shared_ptr<FileTranscriber*> token;
            std::string id;
            gint64 started;
        };
        timer = g_timeout_add_seconds(settings.timeoutS, &FileTranscriber::onTimeout, this);
        g_subprocess_communicate_utf8_async(
                proc, nullptr, nullptr,
                +[](GObject* src, GAsyncResult* res, gpointer d) {
                    std::unique_ptr<Run> run(static_cast<Run*>(d));
                    gchar* out = nullptr;
                    gchar* err = nullptr;
                    GError* error = nullptr;
                    const bool talked =
                            g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, &err, &error);
                    if (FileTranscriber* self = *run->token) {
                        self->onRunEnded(G_SUBPROCESS(src), run->id, run->started, talked, out, err, error);
                    }
                    g_free(out);
                    g_free(err);
                    g_clear_error(&error);
                },
                new Run{token, p.id, g_get_monotonic_time()});
    }

    void onRunEnded(GSubprocess* proc, const std::string& id, gint64 started, bool talked, const char* out,
                    const char* err, GError* error) {
        TranscriberResult r;
        r.id = id;
        r.elapsed = static_cast<double>(g_get_monotonic_time() - started) / G_USEC_PER_SEC;
        if (talked && g_subprocess_get_successful(proc)) {
            r.ok = true;
            r.text = trimmed(out ? out : "");
        } else {
            std::string e = trimmed(err ? err : "");
            if (const auto nl = e.find_last_of('\n'); nl != std::string::npos) {
                e = e.substr(nl + 1);  // (the last line says what went wrong)
            }
            r.error = !e.empty() ? e : error ? error->message : "the recognizer failed";
        }
        if (proc == running) {
            if (timer) {
                g_source_remove(timer);
                timer = 0;
            }
            g_object_unref(running);
            running = nullptr;
        }
        answer(std::move(r));
        runNext();
    }

    static gboolean onTimeout(gpointer d) {
        auto* self = static_cast<FileTranscriber*>(d);
        self->timer = 0;
        // Ended with what it started; its answer is this error (whatever the run says later is ignored)
        signalRecognizer(self->running, SIGKILL);
        g_object_unref(self->running);
        self->running = nullptr;
        TranscriberResult r;
        r.id = self->runningId;
        r.error = "no answer in " + std::to_string(self->settings.timeoutS) + " s";
        self->answer(std::move(r));
        self->runNext();
        return G_SOURCE_REMOVE;
    }

    TranscriberSettings settings;
    TranscriberHooks hooks;
    std::shared_ptr<FileTranscriber*> token;  ///< null once this is gone (async callbacks check it)
    std::deque<TranscriberPiece> queue;       ///< not started yet
    std::set<std::string> pending;            ///< submitted, not answered
    GSubprocess* running = nullptr;
    std::string runningId;
    guint timer = 0;
};
}  // namespace

std::unique_ptr<Transcriber> makeFileTranscriber(const json& config, TranscriberHooks hooks) {
    return std::make_unique<FileTranscriber>(TranscriberSettings::from(config), std::move(hooks));
}

}  // namespace xoj::assistant
