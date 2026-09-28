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
 * @return the backup path, or std::nullopt if saving failed (never throws)
 */
std::optional<fs::path> backupDocument(Control* control, const fs::path& dir, const std::string& reason,
                                       size_t keep = 30);

}  // namespace xoj::api
