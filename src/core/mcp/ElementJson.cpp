#include "ElementJson.h"

#include "api/ElementIds.h"     // for ElementIds
#include "api/Geometry.h"       // for simplify, roundTo
#include "model/Image.h"        // for Image
#include "model/Link.h"         // for Link
#include "model/Stroke.h"       // for Stroke
#include "model/StrokeStyle.h"  // for formatStyle
#include "model/TexImage.h"     // for TexImage
#include "model/Text.h"         // for Text
#include "tools/ToolUtil.h"     // for colorToHex

#include "Registry.h"  // for ToolError

namespace xoj::mcp {

using api::roundTo;

Detail detailFromName(const std::string& name) {
    if (name == "bbox") {
        return Detail::Bbox;
    }
    if (name == "full") {
        return Detail::Full;
    }
    if (name == "simplified" || name.empty()) {
        return Detail::Simplified;
    }
    throw ToolError("detail must be one of: bbox, simplified, full");
}

const char* elementTypeName(const Element* e) {
    switch (e->getType()) {
        case ELEMENT_STROKE:
            return "stroke";
        case ELEMENT_TEXT:
            return "text";
        case ELEMENT_TEXIMAGE:
            return "latex";
        case ELEMENT_IMAGE:
            return "image";
        case ELEMENT_LINK:
            return "link";
    }
    return "unknown";
}

json bboxJson(double x, double y, double w, double h) {
    return json::array({roundTo(x), roundTo(y), roundTo(w), roundTo(h)});
}

namespace {
const char* strokeToolName(const Stroke* s) {
    switch (static_cast<StrokeTool::Value>(s->getToolType())) {
        case StrokeTool::HIGHLIGHTER:
            return "highlighter";
        case StrokeTool::ERASER:
            return "eraser";
        default:
            return "pen";
    }
}
}  // namespace

std::function<fs::path()>& audioFolderProvider() {
    static std::function<fs::path()> provider;
    return provider;
}

json elementToJson(const api::ElementLocation& loc, Detail detail, double tolerance, const std::string* id) {
    const Element* e = loc.element;
    const auto& bb = e->getBoundingBox();
    json out = {{"id", id ? *id : api::ElementIds::get().idOf(e)},
                {"type", elementTypeName(e)},
                {"layer", loc.layer},
                {"bbox", bboxJson(bb.x, bb.y, bb.width, bb.height)},
                {"color", tools::colorToHex(e->getColor())}};

    switch (e->getType()) {
        case ELEMENT_STROKE: {
            const auto* s = static_cast<const Stroke*>(e);
            out["tool"] = strokeToolName(s);
            out["point_count"] = s->getPointCount();
            if (detail == Detail::Bbox) {
                break;
            }
            out["width"] = roundTo(s->getWidth());
            out["pressure"] = s->hasPressure();
            out["line_style"] = StrokeStyle::formatStyle(s->getLineStyle());
            if (s->getFill() >= 0) {
                out["fill_opacity"] = roundTo(s->getFill() / 255.0);
            }
            const auto points =
                    detail == Detail::Full ? s->getPointVector() : api::simplify(s->getPointVector(), tolerance);
            json pts = json::array();
            const bool pressure = s->hasPressure();
            for (const auto& p: points) {
                if (pressure) {
                    pts.push_back({roundTo(p.x), roundTo(p.y), roundTo(p.z)});
                } else {
                    pts.push_back({roundTo(p.x), roundTo(p.y)});
                }
            }
            out["points"] = std::move(pts);
            break;
        }
        case ELEMENT_TEXT: {
            const auto* t = static_cast<const Text*>(e);
            out["text"] = t->getText();
            out["font"] = {{"name", t->getFontName()}, {"size", roundTo(t->getFontSize())}};
            break;
        }
        case ELEMENT_TEXIMAGE:
            out["latex"] = static_cast<const TexImage*>(e)->getText();
            break;
        case ELEMENT_LINK: {
            const auto* l = static_cast<const Link*>(e);
            out["text"] = l->getText();
            out["url"] = l->getUrl();
            break;
        }
        case ELEMENT_IMAGE:
            out.erase("color");
            break;
    }
    // Written during an audio recording: which one, and when in it (the transcript can be aligned with the ink)
    const AudioContent* audio =
            e->getType() == ELEMENT_STROKE ? static_cast<const AudioContent*>(static_cast<const Stroke*>(e)) :
            e->getType() == ELEMENT_TEXT   ? static_cast<const AudioContent*>(static_cast<const Text*>(e)) :
                                             nullptr;
    if (audio && !audio->getAudioFilename().empty()) {
        const fs::path folder = audioFolderProvider() ? audioFolderProvider()() : fs::path();
        out["audio"] = {
                {"file", (folder.empty() ? audio->getAudioFilename() : folder / audio->getAudioFilename()).string()},
                {"t", roundTo(static_cast<double>(audio->getTimestamp()) / 1000.0, 2)}};
    }
    return out;
}

}  // namespace xoj::mcp
