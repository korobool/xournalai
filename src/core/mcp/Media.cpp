#include "Media.h"

#include <atomic>   // for atomic
#include <fstream>  // for ofstream

#include <glib.h>  // for g_base64_encode

#include "PathText.h"

namespace xoj::mcp {

std::string base64Encode(const std::string& bytes) {
    gchar* enc = g_base64_encode(reinterpret_cast<const guchar*>(bytes.data()), bytes.size());
    std::string out(enc);
    g_free(enc);
    return out;
}

std::string base64Decode(const std::string& text) {
    std::string input = text;
    if (auto comma = input.find(','); input.rfind("data:", 0) == 0 && comma != std::string::npos) {
        input = input.substr(comma + 1);  // accept data URLs
    }
    gsize len = 0;
    guchar* dec = g_base64_decode(input.c_str(), &len);
    std::string out(reinterpret_cast<const char*>(dec), len);
    g_free(dec);
    return out;
}

std::string encodePng(cairo_surface_t* surface) {
    std::string out;
    cairo_surface_flush(surface);
    cairo_surface_write_to_png_stream(
            surface,
            [](void* closure, const unsigned char* data, unsigned int length) {
                static_cast<std::string*>(closure)->append(reinterpret_cast<const char*>(data), length);
                return CAIRO_STATUS_SUCCESS;
            },
            &out);
    return out;
}

fs::path writeExportFile(const fs::path& dir, const std::string& stem, const std::string& ext,
                         const std::string& bytes) {
    static std::atomic<unsigned> counter{0};
    fs::create_directories(dir);
    GDateTime* now = g_date_time_new_now_local();
    gchar* stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    g_date_time_unref(now);
    fs::path file = dir / pathFromUtf8(stem + "-" + stamp + "-" + std::to_string(counter++) + "." + ext);
    g_free(stamp);
    std::ofstream out(file, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw ToolError("Could not write " + toUtf8(file));
    }
    return file;
}

void addPng(ToolResult& result, const std::string& pngBytes, const fs::path& file) {
    result.addImage(base64Encode(pngBytes), "image/png");
    result.addText("PNG saved to " + toUtf8(file));
}

}  // namespace xoj::mcp
