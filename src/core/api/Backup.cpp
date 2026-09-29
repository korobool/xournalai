#include "Backup.h"

#include <algorithm>           // for sort
#include <condition_variable>  // for condition_variable
#include <memory>              // for shared_ptr
#include <mutex>               // for mutex, lock_guard
#include <shared_mutex>        // for shared_lock
#include <thread>              // for thread
#include <vector>              // for vector

#include <glib.h>  // for g_date_time

#include "control/Control.h"              // for Control
#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"               // for Document

namespace xoj::api {

namespace {
/// Backups being written in the background
struct Writers {
    std::mutex m;
    std::condition_variable done;
    size_t active = 0;
    unsigned sequence = 0;
};
Writers& writers() {
    static Writers w;
    return w;
}

/// Keeps only the newest backups
void prune(const fs::path& dir, size_t keep) {
    try {
        std::vector<fs::directory_entry> entries;
        for (const auto& e: fs::directory_iterator(dir)) {
            if (e.is_regular_file() && e.path().extension() == ".xopp") {
                entries.push_back(e);
            }
        }
        if (entries.size() > keep) {
            std::sort(entries.begin(), entries.end(),
                      [](const auto& a, const auto& b) { return a.last_write_time() > b.last_write_time(); });
            for (size_t i = keep; i < entries.size(); i++) {
                std::error_code ec;
                fs::remove(entries[i].path(), ec);
            }
        }
    } catch (const std::exception& e) {
        g_warning("MCP backup cleanup failed: %s", e.what());
    }
}
}  // namespace

void waitForBackups() {
    Writers& w = writers();
    std::unique_lock lock(w.m);
    w.done.wait(lock, [&w] { return w.active == 0; });
}

std::optional<fs::path> backupDocument(Control* control, const fs::path& dir, const std::string& reason, size_t keep) {
    try {
        fs::create_directories(dir);
        Document* doc = control->getDocument();
        std::string stem = "untitled";
        {
            std::shared_lock lock(*doc);
            if (!doc->getFilepath().empty()) {
                stem = doc->getFilepath().stem().string();
            }
        }
        GDateTime* now = g_date_time_new_now_local();
        gchar* stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
        g_date_time_unref(now);
        Writers& w = writers();
        unsigned seq = 0;
        {
            std::lock_guard guard(w.m);
            seq = ++w.sequence;
        }
        const fs::path file = dir / (stem + "-" + stamp + "-" + std::to_string(seq) + "-" + reason + ".xopp");
        g_free(stamp);

        auto saver = std::make_shared<SaveHandler>();
        {
            std::shared_lock lock(*doc);
            saver->prepareSave(doc, file);  // a copy of the document as XML; writing it needs no document access
        }
        {
            std::lock_guard guard(w.m);
            w.active++;
        }
        std::thread([saver, file, dir, keep, &w]() {
            const fs::path part = fs::path(file).concat(".part");  // complete files only ever appear as .xopp
            saver->saveTo(part);
            std::error_code ec;
            if (saver->getErrorMessage().empty() && (fs::rename(part, file, ec), !ec)) {
                prune(dir, keep);
            } else {
                fs::remove(part, ec);
                g_warning("MCP backup failed: %s", saver->getErrorMessage().c_str());
            }
            std::lock_guard guard(w.m);
            w.active--;
            w.done.notify_all();
        }).detach();
        return file;
    } catch (const std::exception& e) {
        g_warning("MCP backup failed: %s", e.what());
        return std::nullopt;
    }
}

}  // namespace xoj::api
