#include "Layout.h"

#include <algorithm>  // for sort, min, max
#include <cmath>      // for hypot
#include <numeric>    // for iota

#include "Geometry.h"  // for intersects

namespace xoj::api {

using Rect = xoj::util::Rectangle<double>;
using Pt = xoj::util::Point<double>;

namespace {

struct UnionFind {
    std::vector<size_t> parent;
    explicit UnionFind(size_t n): parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    size_t find(size_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }
    void unite(size_t a, size_t b) { parent[find(a)] = find(b); }
};

Rect expand(const Rect& r, double dx, double dy) { return {r.x - dx, r.y - dy, r.width + 2 * dx, r.height + 2 * dy}; }

Rect unite(const Rect& a, const Rect& b) {
    const double x1 = std::min(a.x, b.x), y1 = std::min(a.y, b.y);
    const double x2 = std::max(a.x + a.width, b.x + b.width), y2 = std::max(a.y + a.height, b.y + b.height);
    return {x1, y1, x2 - x1, y2 - y1};
}

bool contains(const Rect& r, const Pt& p) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

bool inside(const Rect& inner, const Rect& outer, double tol) {
    return inner.x >= outer.x - tol && inner.y >= outer.y - tol &&
           inner.x + inner.width <= outer.x + outer.width + tol &&
           inner.y + inner.height <= outer.y + outer.height + tol;
}

double quantile(std::vector<double> v, double q) {
    if (v.empty()) {
        return 0;
    }
    const auto k = static_cast<size_t>(q * static_cast<double>(v.size() - 1));
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}

/// Splits the strokes of a handwriting block into text lines by the vertical position of their centers
std::vector<Rect> textLines(const std::vector<LayoutItem>& items, const std::vector<size_t>& members, double h) {
    std::vector<size_t> sorted = members;
    auto cy = [&](size_t i) { return items[i].bbox.y + items[i].bbox.height / 2; };
    std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) { return cy(a) < cy(b); });
    std::vector<Rect> lines;
    double lineCenter = 0;
    size_t lineCount = 0;
    for (size_t i: sorted) {
        if (lineCount == 0 || cy(i) > lineCenter + 0.9 * h) {
            lines.push_back(items[i].bbox);
            lineCenter = cy(i);
            lineCount = 1;
        } else {
            lines.back() = unite(lines.back(), items[i].bbox);
            lineCenter = (lineCenter * static_cast<double>(lineCount) + cy(i)) / static_cast<double>(lineCount + 1);
            lineCount++;
        }
    }
    return lines;
}

/// Groups `members` (indices into items) into connected clusters of overlapping expanded bboxes
std::vector<std::vector<size_t>> cluster(const std::vector<LayoutItem>& items, const std::vector<size_t>& members,
                                         double dx, double dy) {
    UnionFind uf(members.size());
    std::vector<Rect> boxes;
    boxes.reserve(members.size());
    for (size_t m: members) {
        boxes.push_back(expand(items[m].bbox, dx, dy));
    }
    // Sweep over x to keep this fast for pages with thousands of strokes
    std::vector<size_t> order(members.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return boxes[a].x < boxes[b].x; });
    for (size_t i = 0; i < order.size(); i++) {
        const Rect& a = boxes[order[i]];
        for (size_t j = i + 1; j < order.size() && boxes[order[j]].x <= a.x + a.width; j++) {
            if (intersects(a, boxes[order[j]])) {
                uf.unite(order[i], order[j]);
            }
        }
    }
    std::vector<std::vector<size_t>> groups(members.size());
    for (size_t i = 0; i < members.size(); i++) {
        groups[uf.find(i)].push_back(members[i]);
    }
    std::vector<std::vector<size_t>> out;
    for (auto& g: groups) {
        if (!g.empty()) {
            std::sort(g.begin(), g.end());
            out.push_back(std::move(g));
        }
    }
    return out;
}

Rect bboxOf(const std::vector<LayoutItem>& items, const std::vector<size_t>& members) {
    Rect r = items[members.front()].bbox;
    for (size_t m: members) {
        r = unite(r, items[m].bbox);
    }
    return r;
}

/// Merges blocks of the same kind whose expanded bboxes overlap (repeats until stable)
void mergeTouching(std::vector<LayoutBlock>& blocks, const std::string& kind, double margin) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < blocks.size() && !changed; i++) {
            if (blocks[i].kind != kind) {
                continue;
            }
            for (size_t j = i + 1; j < blocks.size(); j++) {
                if (blocks[j].kind == kind && intersects(expand(blocks[i].bbox, margin, margin), blocks[j].bbox)) {
                    blocks[i].bbox = unite(blocks[i].bbox, blocks[j].bbox);
                    blocks[i].items.insert(blocks[i].items.end(), blocks[j].items.begin(), blocks[j].items.end());
                    blocks[i].lines.insert(blocks[i].lines.end(), blocks[j].lines.begin(), blocks[j].lines.end());
                    blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(j));
                    changed = true;
                    break;
                }
            }
        }
    }
}

}  // namespace

Layout analyzeLayout(const std::vector<LayoutItem>& items) {
    Layout layout;

    // Typical handwriting scale: 70th percentile of pen stroke heights, ignoring dots and very large strokes
    std::vector<double> heights;
    for (const auto& it: items) {
        if (it.kind == LayoutItem::Kind::PenStroke && it.bbox.height > 1.5 && it.bbox.height < 60) {
            heights.push_back(it.bbox.height);
        }
    }
    const double h = std::clamp(heights.empty() ? 10.0 : quantile(heights, 0.7), 3.0, 40.0);
    layout.typicalHeight = h;

    // 1. Long, nearly straight pen strokes are connector candidates
    std::vector<size_t> pen, candidates;
    const double minConnector = std::max(4 * h, 36.0);
    for (const auto& it: items) {
        if (it.kind != LayoutItem::Kind::PenStroke) {
            continue;
        }
        const double chord = std::hypot(it.last.x - it.first.x, it.last.y - it.first.y);
        const bool straight = it.length > 0 && chord / it.length >= 0.92;
        (straight && chord >= minConnector ? candidates : pen).push_back(it.index);
    }
    // Candidates whose ends meet another candidate's end form a polyline/shape (e.g. a box drawn side by side)
    std::vector<size_t> connectorCandidates;
    for (size_t a: candidates) {
        bool joined = false;
        for (size_t b: candidates) {
            if (a == b) {
                continue;
            }
            for (const Pt& pa: {items[a].first, items[a].last}) {
                for (const Pt& pb: {items[b].first, items[b].last}) {
                    joined = joined || std::hypot(pa.x - pb.x, pa.y - pb.y) < h;
                }
            }
        }
        (joined ? pen : connectorCandidates).push_back(a);
    }

    // Dots (i-dots, punctuation) don't take part in clustering: they are attached to a block afterwards
    std::vector<size_t> dots;
    std::vector<size_t> strokes;
    for (size_t i: pen) {
        (items[i].bbox.width < 0.35 * h && items[i].bbox.height < 0.35 * h ? dots : strokes).push_back(i);
    }

    // 2. Cluster pen strokes into text lines and shape parts. A cluster made mostly of letter-sized strokes is
    //    handwriting (however tall it is); one with a notable share of large strokes is a figure.
    std::vector<LayoutBlock> blocks;
    for (auto& group: cluster(items, strokes, 0.9 * h, 0.25 * h)) {
        LayoutBlock b;
        b.bbox = bboxOf(items, group);
        size_t large = 0;
        for (size_t i: group) {
            large += std::max(items[i].bbox.width, items[i].bbox.height) > 5 * h ? 1 : 0;
        }
        const bool flat = b.bbox.height <= 2.6 * h;
        b.kind = flat || (large * 5 < group.size() && group.size() >= 3) ? "handwriting" : "figure";
        b.items = std::move(group);
        blocks.push_back(std::move(b));
    }

    // 3. Handwriting lines stacked closely with horizontal overlap form paragraphs
    std::sort(blocks.begin(), blocks.end(), [](const auto& a, const auto& b) { return a.bbox.y < b.bbox.y; });
    bool merged = true;
    while (merged) {
        merged = false;
        for (size_t i = 0; i < blocks.size() && !merged; i++) {
            for (size_t j = 0; j < blocks.size(); j++) {
                if (i == j || blocks[i].kind != "handwriting" || blocks[j].kind != "handwriting") {
                    continue;
                }
                const Rect& a = blocks[i].bbox;
                const Rect& c = blocks[j].bbox;
                const double gap = c.y - (a.y + a.height);
                const bool xOverlap = c.x <= a.x + a.width && a.x <= c.x + c.width;
                if (gap >= -0.5 * h && gap <= 1.2 * h && xOverlap && c.y >= a.y) {
                    blocks[i].bbox = unite(a, c);
                    blocks[i].items.insert(blocks[i].items.end(), blocks[j].items.begin(), blocks[j].items.end());
                    blocks.erase(blocks.begin() + static_cast<std::ptrdiff_t>(j));
                    merged = true;
                    break;
                }
            }
        }
    }

    // 4. Figure parts close to each other are one figure
    mergeTouching(blocks, "figure", 1.0 * h);

    for (size_t d: dots) {
        const Pt c{items[d].bbox.x + items[d].bbox.width / 2, items[d].bbox.y + items[d].bbox.height / 2};
        LayoutBlock* home = nullptr;
        for (auto& b: blocks) {
            if (contains(expand(b.bbox, 1.2 * h, 1.2 * h), c) &&
                (!home || b.bbox.width * b.bbox.height < home->bbox.width * home->bbox.height)) {
                home = &b;
            }
        }
        if (home) {
            home->items.push_back(d);
            home->bbox = unite(home->bbox, items[d].bbox);
        } else {
            LayoutBlock b;
            b.kind = "handwriting";
            b.bbox = items[d].bbox;
            b.items = {d};
            blocks.push_back(std::move(b));
        }
    }

    // 5. Connectors: attach endpoints to nearby blocks; unattached candidates are figure strokes
    auto nearestBlock = [&](const Pt& p) -> std::optional<size_t> {
        std::optional<size_t> best;
        double bestArea = 0;
        for (size_t i = 0; i < blocks.size(); i++) {
            if (contains(expand(blocks[i].bbox, 1.5 * h, 1.5 * h), p)) {
                const double area = blocks[i].bbox.width * blocks[i].bbox.height;
                if (!best || area < bestArea) {  // prefer the tightest block (a label inside a box)
                    best = i;
                    bestArea = area;
                }
            }
        }
        return best;
    };
    std::vector<std::pair<size_t, std::pair<std::optional<size_t>, std::optional<size_t>>>> attached;
    for (size_t c: connectorCandidates) {
        auto from = nearestBlock(items[c].first);
        auto to = nearestBlock(items[c].last);
        if ((from || to) && from != to) {
            attached.push_back({c, {from, to}});
        } else {
            LayoutBlock b;
            b.kind = "figure";
            b.bbox = items[c].bbox;
            b.items = {c};
            blocks.push_back(std::move(b));
        }
    }
    const size_t beforeMerge = blocks.size();
    mergeTouching(blocks, "figure", 1.0 * h);
    if (blocks.size() != beforeMerge) {  // indices shifted: re-attach connectors
        for (auto& [c, ends]: attached) {
            ends = {nearestBlock(items[c].first), nearestBlock(items[c].last)};
        }
    }

    // 6. Other content kinds
    std::vector<size_t> highlighter;
    for (const auto& it: items) {
        switch (it.kind) {
            case LayoutItem::Kind::Highlighter:
                highlighter.push_back(it.index);
                break;
            case LayoutItem::Kind::Text:
            case LayoutItem::Kind::Latex:
            case LayoutItem::Kind::Image:
            case LayoutItem::Kind::Link: {
                LayoutBlock b;
                b.kind = it.kind == LayoutItem::Kind::Text  ? "typed_text" :
                         it.kind == LayoutItem::Kind::Latex ? "latex" :
                         it.kind == LayoutItem::Kind::Image ? "image" :
                                                              "link";
                b.bbox = it.bbox;
                b.items = {it.index};
                blocks.push_back(std::move(b));
                break;
            }
            default:
                break;
        }
    }
    for (auto& group: cluster(items, highlighter, 0.5 * h, 0.5 * h)) {
        LayoutBlock b;
        b.kind = "highlight";
        b.bbox = bboxOf(items, group);
        b.items = std::move(group);
        blocks.push_back(std::move(b));
    }

    // 7. Reading order: bands of vertically overlapping blocks, top to bottom, each left to right
    std::vector<size_t> order(blocks.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return blocks[a].bbox.y < blocks[b].bbox.y; });
    std::vector<size_t> reading;
    for (size_t i = 0; i < order.size();) {
        const double bandBottom = blocks[order[i]].bbox.y + blocks[order[i]].bbox.height;
        size_t j = i;
        while (j < order.size() && blocks[order[j]].bbox.y + blocks[order[j]].bbox.height / 2 < bandBottom) {
            j++;
        }
        j = std::max(j, i + 1);
        std::vector<size_t> band(order.begin() + static_cast<std::ptrdiff_t>(i),
                                 order.begin() + static_cast<std::ptrdiff_t>(j));
        std::sort(band.begin(), band.end(), [&](size_t a, size_t b) { return blocks[a].bbox.x < blocks[b].bbox.x; });
        reading.insert(reading.end(), band.begin(), band.end());
        i = j;
    }
    std::vector<size_t> newIndex(blocks.size());
    for (size_t k = 0; k < reading.size(); k++) {
        newIndex[reading[k]] = k;
        auto b = blocks[reading[k]];
        std::sort(b.items.begin(), b.items.end());
        if (b.kind == "handwriting") {
            b.lines = textLines(items, b.items, h);
        }
        layout.blocks.push_back(std::move(b));
    }

    // 8. Labels inside figures; connectors with final block indices
    for (size_t i = 0; i < layout.blocks.size(); i++) {
        if (layout.blocks[i].kind == "figure") {
            continue;
        }
        for (size_t f = 0; f < layout.blocks.size(); f++) {
            if (layout.blocks[f].kind == "figure" && inside(layout.blocks[i].bbox, layout.blocks[f].bbox, 0.5 * h)) {
                layout.blocks[i].parent = f;
                break;
            }
        }
    }
    for (const auto& [c, ends]: attached) {
        LayoutConnector con;
        con.item = c;
        con.from = items[c].first;
        con.to = items[c].last;
        if (ends.first) {
            con.fromBlock = newIndex[*ends.first];
        }
        if (ends.second) {
            con.toBlock = newIndex[*ends.second];
        }
        layout.connectors.push_back(con);
    }
    return layout;
}

}  // namespace xoj::api
