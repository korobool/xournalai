#include "RenderApi.h"

#include <algorithm>     // for max, min
#include <cmath>         // for ceil, floor
#include <shared_mutex>  // for shared_lock
#include <stdexcept>     // for invalid_argument

#include <cairo-svg.h>
#include <cairo.h>

#include "model/Document.h"                   // for Document
#include "model/PageSnapshot.h"               // for snapshotPage
#include "model/XojPage.h"                    // for XojPage
#include "pdf/base/XojPdfPage.h"              // for XojPdfPage
#include "util/ElementRange.h"                // for LayerRangeVector
#include "view/DocumentView.h"                // for DocumentView
#include "view/background/BackgroundFlags.h"  // for BackgroundFlags

namespace xoj::api {

namespace {

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

void drawGrid(cairo_t* cr, const xoj::util::Rectangle<double>& region, double step, double scale) {
    cairo_save(cr);
    const double lineWidth = 0.6 / scale;
    cairo_set_line_width(cr, lineWidth);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 9.0 / scale);
    const double x0 = std::ceil(region.x / step) * step;
    const double y0 = std::ceil(region.y / step) * step;
    for (double x = x0; x <= region.x + region.width; x += step) {
        cairo_set_source_rgba(cr, 0.1, 0.5, 0.9, 0.35);
        cairo_move_to(cr, x, region.y);
        cairo_line_to(cr, x, region.y + region.height);
        cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0.1, 0.4, 0.8, 0.9);
        cairo_move_to(cr, x + 2 / scale, region.y + 10 / scale);
        cairo_show_text(cr, std::to_string(static_cast<int>(x)).c_str());
    }
    for (double y = y0; y <= region.y + region.height; y += step) {
        cairo_set_source_rgba(cr, 0.1, 0.5, 0.9, 0.35);
        cairo_move_to(cr, region.x, y);
        cairo_line_to(cr, region.x + region.width, y);
        cairo_stroke(cr);
        cairo_set_source_rgba(cr, 0.1, 0.4, 0.8, 0.9);
        cairo_move_to(cr, region.x + 2 / scale, y - 2 / scale);
        cairo_show_text(cr, std::to_string(static_cast<int>(y)).c_str());
    }
    cairo_restore(cr);
}

void drawHighlights(cairo_t* cr, const std::vector<Highlight>& highlights, Color color, double scale) {
    cairo_save(cr);
    cairo_set_source_rgba(cr, color.red / 255.0, color.green / 255.0, color.blue / 255.0, 0.9);
    cairo_set_line_width(cr, 1.5 / scale);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.0 / scale);
    const double pad = 3 / scale;
    for (const auto& h: highlights) {
        cairo_rectangle(cr, h.rect.x - pad, h.rect.y - pad, h.rect.width + 2 * pad, h.rect.height + 2 * pad);
        cairo_stroke(cr);
        if (!h.label.empty()) {
            cairo_move_to(cr, h.rect.x - pad, h.rect.y - pad - 2 / scale);
            cairo_show_text(cr, h.label.c_str());
        }
    }
    cairo_restore(cr);
}

/// The requested region clamped to the page (whole page if none); throws if it lies outside
xoj::util::Rectangle<double> clampRegion(const ConstPageRef& page, const RenderOptions& o) {
    const xoj::util::Rectangle<double> full(0, 0, page->getWidth(), page->getHeight());
    if (!o.region) {
        return full;
    }
    const double x1 = std::max(o.region->x, 0.0);
    const double y1 = std::max(o.region->y, 0.0);
    const double x2 = std::min(o.region->x + o.region->width, full.width);
    const double y2 = std::min(o.region->y + o.region->height, full.height);
    if (x2 - x1 < 1 || y2 - y1 < 1) {
        throw std::invalid_argument("The region lies outside the page (page size " +
                                    std::to_string(static_cast<int>(full.width)) + " x " +
                                    std::to_string(static_cast<int>(full.height)) + " points)");
    }
    return {x1, y1, x2 - x1, y2 - y1};
}

/// Draws backgrounds and layers of a page (in page coordinates) onto `cr`; `pdfPage`: its PDF background, if any
void drawContent(cairo_t* cr, const XojPdfPageSPtr& pdfPage, const ConstPageRef& page, const RenderOptions& o) {
    if (o.background && pdfPage) {
        pdfPage->render(cr);
    }

    xoj::view::BackgroundFlags flags;
    flags.showPDF = xoj::view::HIDE_PDF_BACKGROUND;  // drawn above
    flags.showImage = o.background ? xoj::view::SHOW_IMAGE_BACKGROUND : xoj::view::HIDE_IMAGE_BACKGROUND;
    flags.showRuling = o.background ? xoj::view::SHOW_RULING_BACKGROUND : xoj::view::HIDE_RULING_BACKGROUND;
    flags.forceBackgroundColor = xoj::view::DONT_FORCE_BACKGROUND_COLOR;

    DocumentView view;
    if (o.layers) {
        LayerRangeVector ranges;
        for (size_t l: *o.layers) {
            if (l < 1 || l > page->getLayerCount()) {
                throw std::invalid_argument("Layer " + std::to_string(l) + " does not exist on page " +
                                            std::to_string(o.page + 1));
            }
            ranges.emplace_back(l - 1, l - 1);
        }
        view.drawLayersOfPage(ranges, page, cr, true, flags);
    } else {
        view.drawPage(page, cr, true, flags);
    }
}

}  // namespace

RenderedImage renderPage(Document* doc, const RenderOptions& o) {
    // The document lock is held only to copy the page (see model/PageSnapshot.h): rendering and PNG encoding take
    // long on big pages, and the UI thread needs the lock to add the user's strokes
    std::shared_lock lock(*doc);
    if (o.page >= doc->getPageCount()) {
        throw std::invalid_argument("Page " + std::to_string(o.page + 1) + " does not exist");
    }
    ConstPageRef page = doc->getPage(o.page);
    const xoj::util::Rectangle<double> region = clampRegion(page, o);
    XojPdfPageSPtr pdfPage =
            page->getBackgroundType().isPdfPage() ? doc->getPdfPage(page->getPdfPageNr()) : XojPdfPageSPtr();
    const Range area(region.x, region.y, region.x + region.width, region.y + region.height);
    if (PageRef snapshot = xoj::model::snapshotPage(*page, &area)) {
        page = snapshot;
        lock.unlock();
    }
    if (o.dpi <= 0 || o.maxPixels < 16) {
        throw std::invalid_argument("dpi must be positive and max_px at least 16");
    }
    double scale = o.dpi / 72.0;
    scale = std::min(scale, o.maxPixels / std::max(region.width, region.height));

    RenderedImage out;
    out.scale = scale;
    out.region = region;
    out.widthPx = std::max(1, static_cast<int>(std::ceil(region.width * scale)));
    out.heightPx = std::max(1, static_cast<int>(std::ceil(region.height * scale)));

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, out.widthPx, out.heightPx);
    cairo_t* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);  // opaque white underlay: easier to read for vision models
    cairo_paint(cr);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -region.x, -region.y);

    try {
        drawContent(cr, pdfPage, page, o);
    } catch (...) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        throw;
    }

    if (o.grid) {
        drawGrid(cr, region, o.gridStep > 1 ? o.gridStep : 50, scale);
    }
    drawHighlights(cr, o.highlights, o.highlightColor, scale);

    cairo_destroy(cr);
    out.png = encodePng(surface);
    cairo_surface_destroy(surface);
    return out;
}

}  // namespace xoj::api

namespace xoj::api {

std::string renderSvg(Document* doc, const RenderOptions& o) {
    std::shared_lock lock(*doc);
    if (o.page >= doc->getPageCount()) {
        throw std::invalid_argument("Page " + std::to_string(o.page + 1) + " does not exist");
    }
    ConstPageRef page = doc->getPage(o.page);
    const auto region = clampRegion(page, o);
    std::string out;
    cairo_surface_t* surface = cairo_svg_surface_create_for_stream(
            [](void* closure, const unsigned char* data, unsigned int length) {
                static_cast<std::string*>(closure)->append(reinterpret_cast<const char*>(data), length);
                return CAIRO_STATUS_SUCCESS;
            },
            &out, region.width, region.height);
    cairo_t* cr = cairo_create(surface);
    cairo_translate(cr, -region.x, -region.y);
    try {
        drawContent(cr, page->getBackgroundType().isPdfPage() ? doc->getPdfPage(page->getPdfPageNr()) : nullptr, page,
                    o);
    } catch (...) {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        throw;
    }
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    return out;
}

}  // namespace xoj::api
