/*
 * xournalai (based on Xournal++)
 *
 * (internal to Transcriber) The kinds of recognizers: one maker each. Adding a kind: a class in a file of its own
 * and a line in Transcriber::make — and the specification (docs/mcp/TRANSCRIBERS.md).
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <memory>  // for unique_ptr
#include <string>  // for string

#include <nlohmann/json.hpp>  // for json

#include "Transcriber.h"

namespace xoj::assistant {

/// Settings every kind understands (the specification's "Common settings")
struct TranscriberSettings {
    std::string command;
    guint timeoutS = 120;       ///< a piece without an answer this long after it was sent: failed
    guint readyTimeoutS = 120;  ///< stream: the hello must come within this
    static TranscriberSettings from(const nlohmann::json& config);
};

std::unique_ptr<Transcriber> makeFileTranscriber(const nlohmann::json& config, TranscriberHooks hooks);
std::unique_ptr<Transcriber> makeStreamTranscriber(const nlohmann::json& config, TranscriberHooks hooks);

/// A string field of a JSON object, or `fallback` (missing, null or another type: never throws)
std::string jsonString(const nlohmann::json& j, const char* key, const std::string& fallback = {});
/// A number field, or `fallback`
double jsonNumber(const nlohmann::json& j, const char* key, double fallback);

}  // namespace xoj::assistant
