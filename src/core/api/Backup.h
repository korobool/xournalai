/*
 * Xournal++ (xournalai)
 *
 * Safety backups of the open document before risky agent operations
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <optional>  // for optional
#include <string>    // for string

#include "filesystem.h"

class Control;

namespace xoj::api {

/**
 * @brief Saves a copy of the current document (including unsaved changes) to `dir` as
 * "<name>-<timestamp>-<reason>.xopp" without touching the document or its file. Keeps the newest `keep` backups.
 * The document is captured right away (so it is safe to change it next); the file is written in the background.
 * @return the backup path, or std::nullopt if capturing failed (never throws)
 */
std::optional<fs::path> backupDocument(Control* control, const fs::path& dir, const std::string& reason,
                                       size_t keep = 30);

/// Waits until all backups are written (before the application quits)
void waitForBackups();

}  // namespace xoj::api
