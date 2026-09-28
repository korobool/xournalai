#include "Backup.h"

#include <algorithm>     // for sort
#include <shared_mutex>  // for shared_lock
#include <vector>        // for vector

#include <glib.h>  // for g_date_time

#include "control/Control.h"              // for Control
#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"               // for Document

namespace xoj::api {

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
        const fs::path file = dir / (stem + "-" + stamp + "-" + reason + ".xopp");
        g_free(stamp);

        SaveHandler saver;
        {
            std::shared_lock lock(*doc);
            saver.prepareSave(doc, file);
        }
        saver.saveTo(file);
        if (!saver.getErrorMessage().empty()) {
            g_warning("MCP backup failed: %s", saver.getErrorMessage().c_str());
            return std::nullopt;
        }

        // Keep only the newest backups
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
        return file;
    } catch (const std::exception& e) {
        g_warning("MCP backup failed: %s", e.what());
        return std::nullopt;
    }
}

}  // namespace xoj::api
