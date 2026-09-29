/*
 * xournalai (based on Xournal++)
 *
 * Images and files produced by tools: PNG encoding, base64, and the export folder
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string

#include <cairo.h>  // for cairo_surface_t

#include "Registry.h"  // for ToolResult
#include "filesystem.h"

namespace xoj::mcp {

std::string base64Encode(const std::string& bytes);
std::string base64Decode(const std::string& text);

/// Encodes an image surface as PNG bytes
std::string encodePng(cairo_surface_t* surface);

/**
 * @brief Writes `bytes` to a new file in `dir` named "<stem>-<timestamp>.<ext>" (creating the directory).
 * @return the full path
 */
fs::path writeExportFile(const fs::path& dir, const std::string& stem, const std::string& ext,
                         const std::string& bytes);

/**
 * @brief Adds a PNG to a tool result: inline image content (for clients and models with vision) plus the path of
 * the same image on disk (for clients that can't show inline images).
 */
void addPng(ToolResult& result, const std::string& pngBytes, const fs::path& file);

}  // namespace xoj::mcp
