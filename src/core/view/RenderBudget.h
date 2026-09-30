/*
 * xournalai (based on Xournal++)
 *
 * How big a single rendered buffer may get. Pages (and PDF backgrounds) are normally rendered whole at the current
 * zoom; beyond this budget (deep zoom) only the visible part is rendered, so memory stays flat at any zoom and no
 * surface exceeds cairo's size limit.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cairo.h>  // for cairo_t

#include "util/Range.h"  // for Range

namespace xoj::view {

constexpr double MAX_BUFFER_PIXELS = 32e6;  ///< ~128 MB of ARGB
constexpr double MAX_BUFFER_SIDE = 16000;

/// Whether `extent` (in page units) fits one buffer at `pixelsPerUnit` (zoom × DPI scaling)
inline bool fitsInOneBuffer(const Range& extent, double pixelsPerUnit) {
    const double w = extent.getWidth() * pixelsPerUnit;
    const double h = extent.getHeight() * pixelsPerUnit;
    return w * h <= MAX_BUFFER_PIXELS && w <= MAX_BUFFER_SIDE && h <= MAX_BUFFER_SIDE;
}

/**
 * The part of a page to render when the whole page does not fit one buffer: the visible part grown by a margin (up
 * to one visible size on each side, less if the budget requires), within the page. `visible` may be empty (the page
 * is not shown): then the page's top-left corner.
 */
inline Range partialExtent(const Range& page, Range visible, double pixelsPerUnit) {
    visible = visible.empty() ? Range() : visible.intersect(page);
    if (!visible.isValid() || visible.empty()) {
        const double side = 1000.0 / pixelsPerUnit;
        return Range(page.minX, page.minY, page.minX + side, page.minY + side).intersect(page);
    }
    for (double margin: {1.0, 0.5, 0.25, 0.0}) {
        Range r(visible.minX - margin * visible.getWidth(), visible.minY - margin * visible.getHeight(),
                visible.maxX + margin * visible.getWidth(), visible.maxY + margin * visible.getHeight());
        r = r.intersect(page);
        if (fitsInOneBuffer(r, pixelsPerUnit) || margin == 0.0) {
            return r;
        }
    }
    return visible;
}

/**
 * `box` limited to what `cr` can show (its clip, in user coordinates): a mask for a stroke need not be bigger than
 * that, which matters at deep zoom (a page-sized highlighter stroke would otherwise need a page-sized mask). Never
 * empty: at least one device pixel at `zoom`.
 */
inline Range clippedToTarget(cairo_t* cr, const Range& box, double zoom) {
    Range clip;
    cairo_clip_extents(cr, &clip.minX, &clip.minY, &clip.maxX, &clip.maxY);
    Range r = box.intersect(clip);
    if (!r.isValid() || r.empty() || r.getWidth() <= 0 || r.getHeight() <= 0) {
        return Range(clip.minX, clip.minY, clip.minX + 1 / zoom, clip.minY + 1 / zoom);
    }
    return r;
}

}  // namespace xoj::view
