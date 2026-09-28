#include "ToolUtil.h"

#include <algorithm>  // for transform
#include <cctype>     // for tolower
#include <cstdio>     // for snprintf
#include <map>        // for map

#include "control/Control.h"                   // for Control
#include "control/pagetype/PageTypeHandler.h"  // for PageTypeHandler
#include "model/Document.h"                    // for Document
#include "model/Layer.h"                       // for Layer
#include "model/XojPage.h"                     // for XojPage

namespace xoj::mcp::tools {

std::string colorToHex(Color c) {
    char buf[16];
    if (c.alpha == 0xff) {
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.red, c.green, c.blue);
    } else {
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", c.red, c.green, c.blue, c.alpha);
    }
    return buf;
}

namespace {
const std::map<std::string, uint32_t>& namedColors() {
    static const std::map<std::string, uint32_t> colors = {
            {"black", 0x000000},   {"white", 0xffffff},     {"red", 0xff0000},       {"green", 0x008000},
            {"lime", 0x00ff00},    {"blue", 0x0000ff},      {"yellow", 0xffff00},    {"orange", 0xffa500},
            {"purple", 0x800080},  {"violet", 0xee82ee},    {"magenta", 0xff00ff},   {"pink", 0xffc0cb},
            {"cyan", 0x00ffff},    {"teal", 0x008080},      {"navy", 0x000080},      {"brown", 0xa52a2a},
            {"gray", 0x808080},    {"grey", 0x808080},      {"lightgray", 0xd3d3d3}, {"darkgray", 0xa9a9a9},
            {"gold", 0xffd700},    {"silver", 0xc0c0c0},    {"maroon", 0x800000},    {"olive", 0x808000},
            {"indigo", 0x4b0082},  {"turquoise", 0x40e0d0}, {"salmon", 0xfa8072},    {"coral", 0xff7f50},
            {"skyblue", 0x87ceeb}, {"darkgreen", 0x006400}, {"darkblue", 0x00008b},  {"darkred", 0x8b0000},
            {"beige", 0xf5f5dc},   {"khaki", 0xf0e68c},     {"crimson", 0xdc143c},   {"forestgreen", 0x228b22}};
    return colors;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}
}  // namespace

Color parseColor(const json& value, const std::string& what) {
    if (value.is_number_integer()) {
        const auto v = value.get<int64_t>();
        if (v < 0 || v > 0xffffff) {
            throw ToolError("'" + what + "' as a number must be 0xRRGGBB (0..16777215)");
        }
        return Color(static_cast<uint32_t>(v) | 0xff000000U);
    }
    if (!value.is_string()) {
        throw ToolError("'" + what + "' must be a color string like \"#1f77b4\" or \"red\"");
    }
    std::string s = value.get<std::string>();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
    if (auto it = namedColors().find(s); it != namedColors().end()) {
        return Color(it->second | 0xff000000U);
    }
    if (s.rfind("rgb(", 0) == 0 && s.back() == ')') {
        int r = 0, g = 0, b = 0;
        if (std::sscanf(s.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) == 3 && r >= 0 && r < 256 && g >= 0 && g < 256 &&
            b >= 0 && b < 256) {
            return Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        }
    }
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    } else if (s.rfind("0x", 0) == 0) {
        s = s.substr(2);
    }
    if (s.size() == 3) {
        s = std::string{s[0], s[0], s[1], s[1], s[2], s[2]};
    }
    if (s.size() == 6 || s.size() == 8) {
        uint8_t bytes[4] = {0, 0, 0, 0xff};
        bool ok = true;
        for (size_t i = 0; i < s.size() / 2; i++) {
            const int hi = hexDigit(s[2 * i]);
            const int lo = hexDigit(s[2 * i + 1]);
            ok = ok && hi >= 0 && lo >= 0;
            bytes[i] = static_cast<uint8_t>(hi * 16 + lo);
        }
        if (ok) {
            return Color(bytes[0], bytes[1], bytes[2], bytes[3]);
        }
    }
    throw ToolError("Unknown color " + value.dump() + " for '" + what +
                    "'. Use \"#rrggbb\", \"#rrggbbaa\", \"rgb(r,g,b)\" or a name like \"red\"");
}

size_t resolvePageIndex(Control* ctrl, const Args& args, const std::string& key) {
    const size_t count = ctrl->getDocument()->getPageCount();
    if (!args.has(key) || (args.raw(key).is_string() && args.str(key) == "current")) {
        return ctrl->getCurrentPageNo();
    }
    const auto page = args.integer(key);
    if (page < 1 || static_cast<size_t>(page) > count) {
        throw ToolError("Page " + std::to_string(page) + " does not exist; the document has " + std::to_string(count) +
                        " page(s), numbered from 1");
    }
    return static_cast<size_t>(page - 1);
}

std::string layerDisplayName(const Layer* layer, size_t index) {
    return layer->hasName() ? layer->getName() : "Layer " + std::to_string(index);
}

json pageSummary(const PageRef& page, size_t index) {
    const PageType bg = page->getBackgroundType();
    Color bgColor = page->getBackgroundColor();
    bgColor.alpha = 0xff;  // background colors are stored without alpha
    json background = {{"type", PageTypeHandler::getStringForPageTypeFormat(bg.format)},
                       {"color", colorToHex(bgColor)}};
    if (!bg.config.empty()) {
        background["config"] = bg.config;
    }
    if (bg.isPdfPage()) {
        background["pdf_page"] = page->getPdfPageNr() + 1;
    }
    json layers = json::array();
    size_t layerIndex = 0;
    for (const Layer* l: page->getLayersView()) {
        ++layerIndex;
        layers.push_back({{"index", layerIndex},
                          {"name", layerDisplayName(l, layerIndex)},
                          {"visible", l->isVisible()},
                          {"elements", l->getElementsView().size()}});
    }
    return {{"page", index + 1},           {"width", page->getWidth()},
            {"height", page->getHeight()}, {"background", std::move(background)},
            {"layers", std::move(layers)}, {"current_layer", page->getSelectedLayerId()}};
}

void requireDocument(Control* ctrl) {
    if (!ctrl || !ctrl->getWindow() || !ctrl->getDocument()) {
        throw ToolError("The application is not ready (no main window or document)");
    }
}

}  // namespace xoj::mcp::tools
