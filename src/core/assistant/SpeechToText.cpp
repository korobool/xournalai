#include "SpeechToText.h"

#include <utility>  // for move

#include <glib.h>
#include <glib/gstdio.h>  // for g_rename, g_remove

#include "mcp/Json.h"  // for json

namespace xoj::assistant {

namespace {
bool cancelled(GError* err) { return err && g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED); }
}  // namespace

SpeechToText::SpeechToText(std::string model): modelName(std::move(model)), cancellable(g_cancellable_new()) {}

SpeechToText::~SpeechToText() {
    *alive = false;                     // (a remote answer still on its way finds nobody)
    g_cancellable_cancel(cancellable);  // no callback touches this object afterwards
    if (proc) {
        send(R"({"cmd":"quit"})");
        g_subprocess_force_exit(proc);
    }
    if (downloader) {
        g_subprocess_force_exit(downloader);
        g_object_unref(downloader);
    }
    g_clear_object(&out);
    g_clear_object(&proc);
    g_object_unref(cancellable);
}

const char* SpeechToText::name(State s) {
    switch (s) {
        case State::Unavailable:
            return "unavailable";
        case State::Downloading:
            return "downloading";
        case State::Starting:
            return "starting";
        case State::Ready:
            return "ready";
        case State::Listening:
            return "listening";
        case State::Transcribing:
            return "transcribing";
    }
    return "?";
}

std::string SpeechToText::modelsDir() {
    if (const char* dir = g_getenv("XOURNALAI_STT_MODELS"); dir && *dir) {  // e.g. tests with their own data folder
        return dir;
    }
    gchar* d = g_build_filename(g_get_user_data_dir(), "xournalai", "models", nullptr);
    std::string s(d);
    g_free(d);
    return s;
}

std::string SpeechToText::modelPath() const {
    gchar* p = g_build_filename(modelsDir().c_str(), ("ggml-" + modelName + ".bin").c_str(), nullptr);
    std::string s(p);
    g_free(p);
    return s;
}

std::string SpeechToText::helperPath() {
    // Next to the app (installed together), else on the PATH
    if (gchar* self = g_file_read_link("/proc/self/exe", nullptr)) {
        gchar* dir = g_path_get_dirname(self);
        gchar* p = g_build_filename(dir, "xournalai-stt", nullptr);
        std::string s = g_file_test(p, G_FILE_TEST_IS_EXECUTABLE) ? p : "";
        g_free(p);
        g_free(dir);
        g_free(self);
        if (!s.empty()) {
            return s;
        }
    }
    gchar* p = g_find_program_in_path("xournalai-stt");
    std::string s = p ? p : "";
    g_free(p);
    return s;
}

void SpeechToText::log(const std::string& line) {
    gchar* dir = g_build_filename(g_get_user_cache_dir(), "xournalai", nullptr);
    g_mkdir_with_parents(dir, 0700);
    gchar* path = g_build_filename(dir, "speech.log", nullptr);
    if (FILE* f = g_fopen(path, "a")) {
        GDateTime* now = g_date_time_new_now_local();
        gchar* stamp = g_date_time_format(now, "%H:%M:%S");
        fprintf(f, "%s.%03d app  %s\n", stamp, g_date_time_get_microsecond(now) / 1000, line.c_str());
        g_free(stamp);
        g_date_time_unref(now);
        fclose(f);
    }
    g_free(path);
    g_free(dir);
}

void SpeechToText::set(State s) {
    if (current == s) {
        return;
    }
    log(std::string("speech ") + name(current) + " -> " + name(s));
    current = s;
    if (listener) {
        listener(s);
    }
}

void SpeechToText::settle() {
    if (!proc) {
        return;
    }
    const bool busy = !waiting.empty() || !saving.empty() || remotePending > 0;
    set(recordingNow ? State::Listening : (busy ? State::Transcribing : State::Ready));
}

void SpeechToText::warmUp() {
    if (proc || downloader) {
        return;
    }
    if (helperPath().empty()) {
        lastProblem = "the speech helper xournalai-stt is not installed";
        set(State::Unavailable);
        return;
    }
    if (!g_file_test(modelPath().c_str(), G_FILE_TEST_EXISTS)) {
        download();
        return;
    }
    spawn();
}

void SpeechToText::download() {
    g_mkdir_with_parents(modelsDir().c_str(), 0700);
    const std::string url = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-" + modelName + ".bin";
    const std::string part = modelPath() + ".part";
    GError* err = nullptr;
    downloader = g_subprocess_new(
            static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE), &err,
            "curl", "-sfL", "-o", part.c_str(), url.c_str(), nullptr);
    if (!downloader) {
        lastProblem = std::string("could not download the speech model: ") + (err ? err->message : "no curl");
        g_clear_error(&err);
        set(State::Unavailable);
        return;
    }
    set(State::Downloading);
    g_subprocess_wait_check_async(
            downloader, cancellable,
            +[](GObject* src, GAsyncResult* res, gpointer self) {
                GError* err = nullptr;
                const bool ok = g_subprocess_wait_check_finish(G_SUBPROCESS(src), res, &err);
                if (cancelled(err)) {
                    g_error_free(err);
                    return;
                }
                auto* s = static_cast<SpeechToText*>(self);
                g_clear_object(&s->downloader);
                const std::string part = s->modelPath() + ".part";
                if (ok && g_rename(part.c_str(), s->modelPath().c_str()) == 0) {
                    s->spawn();
                } else {
                    g_remove(part.c_str());
                    s->lastProblem = "could not download the speech model (" + s->modelName + ")";
                    s->set(State::Unavailable);
                }
                g_clear_error(&err);
            },
            this);
}

void SpeechToText::spawn() {
    GError* err = nullptr;
    const std::string helper = helperPath();
    const std::string model = modelPath();
    proc = g_subprocess_new(
            static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE),
            &err, helper.c_str(), "--model", model.c_str(), nullptr);
    if (!proc) {
        lastProblem = std::string("could not start the speech helper: ") + (err ? err->message : "?");
        g_clear_error(&err);
        set(State::Unavailable);
        return;
    }
    in = g_subprocess_get_stdin_pipe(proc);
    out = g_data_input_stream_new(g_subprocess_get_stdout_pipe(proc));
    set(State::Starting);
    readNext();
    g_subprocess_wait_async(
            proc, cancellable,
            +[](GObject* src, GAsyncResult* res, gpointer self) {
                GError* err = nullptr;
                g_subprocess_wait_finish(G_SUBPROCESS(src), res, &err);
                if (cancelled(err)) {
                    g_error_free(err);
                    return;
                }
                g_clear_error(&err);
                static_cast<SpeechToText*>(self)->onExit();
            },
            this);
}

void SpeechToText::readNext() {
    g_data_input_stream_read_line_async(
            out, G_PRIORITY_DEFAULT, cancellable,
            +[](GObject* src, GAsyncResult* res, gpointer self) {
                GError* err = nullptr;
                gsize len = 0;
                gchar* line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(src), res, &len, &err);
                if (cancelled(err)) {
                    g_error_free(err);
                    return;
                }
                g_clear_error(&err);
                if (!line) {
                    return;  // the helper exited (onExit follows)
                }
                auto* s = static_cast<SpeechToText*>(self);
                s->onLine(line);
                g_free(line);
                if (s->out) {
                    s->readNext();
                }
            },
            this);
}

void SpeechToText::onLine(const std::string& line) {
    mcp::json j;
    try {
        j = mcp::json::parse(line);
    } catch (const std::exception&) {
        return;
    }
    const std::string event = j.value("event", "");
    if (event != "level") {
        log("helper: " + line);
    }
    if (event == "level") {
        if (levelListener && current == State::Listening) {
            levelListener(j.value("rms", 0.0f));
        }
        return;
    }
    if (event == "ready") {
        settle();  // (Listening if start() was already sent: it is queued)
    } else if (event == "saved") {
        onSaved(j.value("path", ""), j.value("silent", false));
    } else if (event == "listening") {
        settle();
    } else if (event == "text" || event == "error") {
        if (event == "error") {
            lastProblem = j.value("message", "error");
        }
        if (!waiting.empty() && (event == "text" || current == State::Transcribing)) {
            Done done = std::move(waiting.front());
            waiting.pop_front();
            settle();  // (not Ready if a new recording started meanwhile)
            if (done) {
                event == "text" ? done(j.value("text", ""), j.value("silent", true), "") : done("", true, lastProblem);
            }
        } else if (event == "error" && !saving.empty()) {
            Done done = std::move(saving.front());  // (its recording could not be kept)
            saving.pop_front();
            settle();
            if (done) {
                done("", true, lastProblem);
            }
        } else if (event == "error") {
            recordingNow = false;  // e.g. the microphone could not be opened; stop() will then find no speech
            settle();
        }
    } else if (event == "cancelled") {
        settle();
    }
}

void SpeechToText::onExit() {
    log("helper exited");
    g_clear_object(&out);
    in = nullptr;
    g_clear_object(&proc);
    auto pending = std::move(waiting);
    waiting.clear();
    for (auto& d: saving) {
        pending.push_back(std::move(d));
    }
    saving.clear();
    recordingNow = false;
    lastProblem = "the speech helper stopped";
    set(State::Unavailable);
    for (auto& done: pending) {
        if (done) {
            done("", true, lastProblem);
        }
    }
}

void SpeechToText::send(const std::string& line) {
    if (!in) {
        return;
    }
    const std::string text = line + "\n";
    g_output_stream_write_all(in, text.data(), text.size(), nullptr, nullptr, nullptr);
    g_output_stream_flush(in, nullptr, nullptr);
}

void SpeechToText::start() {
    warmUp();
    if (!proc) {
        return;  // unavailable or downloading: stop() will say so
    }
    send(R"({"cmd":"start"})");  // queued by the helper if it is still loading the model
    recordingNow = true;
    settle();
}

void SpeechToText::stop(Done done) {
    if (!proc) {
        if (done) {
            done("", true,
                 current == State::Downloading ? "the speech model is still downloading" :
                                                 "speech is not available: " + lastProblem);
        }
        return;
    }
    recordingNow = false;
    if (remote) {
        // The recording as a WAV first: the remote transcriber tries it, the local model if it can't
        const std::string dir = std::string(g_get_user_cache_dir()) + "/xournalai/speech";
        g_mkdir_with_parents(dir.c_str(), 0700);
        const std::string path = dir + "/ask-" + std::to_string(++saved % 50) + ".wav";
        saving.push_back(std::move(done));
        send(mcp::json({{"cmd", "stop"}, {"save", path}}).dump());
    } else {
        waiting.push_back(std::move(done));
        send(R"({"cmd":"stop"})");
    }
    settle();
}

void SpeechToText::onSaved(const std::string& path, bool silent) {
    if (saving.empty()) {
        return;
    }
    Done done = std::move(saving.front());
    saving.pop_front();
    if (silent) {  // (holding the button without speaking changes nothing, as before)
        g_remove(path.c_str());
        settle();
        if (done) {
            done("", true, "");
        }
        return;
    }
    if (!remote) {
        transcribeWav(path, [path, done](const std::string& t, bool s, const std::string& e) {
            g_remove(path.c_str());
            if (done) {
                done(t, s, e);
            }
        });
        return;
    }
    remotePending++;
    settle();
    std::weak_ptr<bool> token = alive;
    remote(path, [this, token, path, done](const std::string& text, const std::string& error) {
        if (!token.lock()) {
            return;
        }
        remotePending--;
        if (error.empty()) {
            log("remote: \"" + text + "\"");
            g_remove(path.c_str());
            settle();
            if (done) {
                text.empty() ? done("", true, "") : done(text, false, "");
            }
            return;
        }
        log("remote transcriber: " + error + " (the local model instead)");
        transcribeWav(path, [path, done](const std::string& t, bool s, const std::string& e) {
            g_remove(path.c_str());
            if (done) {
                done(t, s, e);
            }
        });
    });
}

void SpeechToText::transcribeWav(const std::string& path, Done done) {
    warmUp();
    if (!proc) {
        if (done) {
            done("", true, "speech is not available: " + lastProblem);
        }
        return;
    }
    waiting.push_back(std::move(done));
    send(mcp::json({{"cmd", "transcribe"}, {"wav", path}}).dump());
    settle();
}

void SpeechToText::cancel() {
    if (proc && recordingNow) {
        send(R"({"cmd":"cancel"})");
        recordingNow = false;
        settle();
    }
}

}  // namespace xoj::assistant
