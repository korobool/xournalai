#include "Placement.h"

#include <algorithm>  // for max, min
#include <cmath>      // for ceil, floor, hypot
#include <limits>     // for numeric_limits
#include <stdexcept>  // for invalid_argument

#include "control/Control.h"  // for Control
#include "model/Document.h"   // for Document
#include "model/Layer.h"      // for Layer
#include "model/PageType.h"   // for PageType
#include "model/XojPage.h"    // for XojPage

namespace xoj::api {

std::optional<Rect> findFreeSpace(const std::vector<Rect>& occupied, double pageWidth, double pageHeight, double w,
                                  double h, double margin, std::optional<Rect> near, const std::string& side) {
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("width and height must be positive");
    }
    if (side != "any" && side != "right" && side != "left" && side != "above" && side != "below") {
        throw std::invalid_argument("side must be any, right, left, above or below");
    }
    constexpr double CELL = 4.0;
    const int cols = static_cast<int>(std::ceil(pageWidth / CELL));
    const int rows = static_cast<int>(std::ceil(pageHeight / CELL));
    if (cols <= 0 || rows <= 0) {
        return std::nullopt;
    }
    // Occupancy grid (content grown by margin) and its summed-area table
    std::vector<int> grid(static_cast<size_t>(cols * rows), 0);
    auto mark = [&](double x1, double y1, double x2, double y2) {
        const int c1 = std::max(0, static_cast<int>(std::floor(x1 / CELL)));
        const int r1 = std::max(0, static_cast<int>(std::floor(y1 / CELL)));
        const int c2 = std::min(cols - 1, static_cast<int>(std::floor(x2 / CELL)));
        const int r2 = std::min(rows - 1, static_cast<int>(std::floor(y2 / CELL)));
        for (int r = r1; r <= r2; r++) {
            for (int c = c1; c <= c2; c++) {
                grid[static_cast<size_t>(r * cols + c)] = 1;
            }
        }
    };
    for (const auto& o: occupied) {
        mark(o.x - margin, o.y - margin, o.x + o.width + margin, o.y + o.height + margin);
    }
    std::vector<int> sat(static_cast<size_t>((cols + 1) * (rows + 1)), 0);
    auto S = [&](int r, int c) -> int& { return sat[static_cast<size_t>(r * (cols + 1) + c)]; };
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            S(r + 1, c + 1) = grid[static_cast<size_t>(r * cols + c)] + S(r, c + 1) + S(r + 1, c) - S(r, c);
        }
    }
    const int cw = static_cast<int>(std::ceil(w / CELL));
    const int ch = static_cast<int>(std::ceil(h / CELL));
    const int minC = static_cast<int>(std::ceil(margin / CELL));
    const int maxC = cols - static_cast<int>(std::ceil(margin / CELL)) - cw;
    const int maxR = rows - static_cast<int>(std::ceil(margin / CELL)) - ch;

    std::optional<Rect> best;
    double bestScore = std::numeric_limits<double>::max();
    for (int r = minC; r <= maxR; r++) {
        for (int c = minC; c <= maxC; c++) {
            if (S(r + ch, c + cw) - S(r, c + cw) - S(r + ch, c) + S(r, c) != 0) {
                continue;
            }
            const Rect cand(c * CELL, r * CELL, w, h);
            if (!near) {
                return cand;  // reading order: first free spot
            }
            const Rect& n = *near;
            if ((side == "right" && cand.x < n.x + n.width) || (side == "left" && cand.x + w > n.x) ||
                (side == "below" && cand.y < n.y + n.height) || (side == "above" && cand.y + h > n.y)) {
                continue;
            }
            // Distance between the rectangles (0 if touching) + a small bias toward alignment with `near`
            const double dx = std::max({0.0, n.x - (cand.x + w), cand.x - (n.x + n.width)});
            const double dy = std::max({0.0, n.y - (cand.y + h), cand.y - (n.y + n.height)});
            const double align = (side == "right" || side == "left")  ? std::abs(cand.y - n.y) :
                                 (side == "below" || side == "above") ? std::abs(cand.x - n.x) :
                                                                        0;
            const double score = std::hypot(dx, dy) + 0.3 * align;
            if (score < bestScore) {
                bestScore = score;
                best = cand;
            }
        }
    }
    return best;
}

std::vector<Rect> occupiedAreas(const PageRef& page) {
    std::vector<Rect> out;
    for (const Layer* l: page->getLayersView()) {
        if (!l->isVisible()) {
            continue;
        }
        for (const Element* e: l->getElementsView()) {
            out.push_back(e->getBoundingBox());
        }
    }
    return out;
}

size_t appendPage(Control* control) {
    Document* doc = control->getDocument();
    PageRef last;
    size_t count = 0;
    {
        std::shared_lock lock(*doc);
        count = doc->getPageCount();
        last = doc->getPage(count - 1);
    }
    auto page = std::make_shared<XojPage>(last->getWidth(), last->getHeight());
    PageType type = last->getBackgroundType();
    if (type.isPdfPage() || type.isImagePage()) {
        type = PageType(PageTypeFormat::Plain);
    }
    page->setBackgroundType(type);
    page->setBackgroundColor(last->getBackgroundColor());
    control->insertPage(page, count, false);
    return count;
}

}  // namespace xoj::api
