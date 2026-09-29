#include "Markers.h"

#include <algorithm>  // for min, max, sort
#include <cmath>      // for hypot, abs

namespace xoj::assistant {

namespace {
struct Info {
    size_t index;
    double x1, y1, x2, y2;  ///< bbox
    double length;          ///< polyline length
    double w() const { return x2 - x1; }
    double h() const { return y2 - y1; }
    double cx() const { return (x1 + x2) / 2; }
    double cy() const { return (y1 + y2) / 2; }
    double size() const { return std::max(w(), h()); }
};

Info info(size_t i, const StrokeShape& s) {
    Info r{i, 1e18, 1e18, -1e18, -1e18, 0};
    for (size_t k = 0; k < s.points.size(); k++) {
        const Point& p = s.points[k];
        r.x1 = std::min(r.x1, p.x);
        r.y1 = std::min(r.y1, p.y);
        r.x2 = std::max(r.x2, p.x);
        r.y2 = std::max(r.y2, p.y);
        if (k > 0) {
            r.length += std::hypot(p.x - s.points[k - 1].x, p.y - s.points[k - 1].y);
        }
    }
    return r;
}

bool isDot(const Info& s) { return s.size() <= 4.0; }

bool isBar(const Info& s) {
    return s.h() >= 6 && s.h() <= 32 && s.w() <= 0.4 * s.h() + 2 && s.length <= 1.6 * s.h() + 3;
}

/// A short, fairly straight stroke (a line of an asterisk)
bool isShortLine(const Info& s) {
    const double diag = std::hypot(s.w(), s.h());
    return s.size() >= 3 && s.size() <= 24 && s.length <= 1.35 * diag + 2;
}

bool segmentsCross(const Point& a, const Point& b, const Point& c, const Point& d) {
    auto cross = [](const Point& o, const Point& p, const Point& q) {
        return (p.x - o.x) * (q.y - o.y) - (p.y - o.y) * (q.x - o.x);
    };
    const double d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

struct Group {
    std::vector<size_t> members;
    double x1 = 1e18, y1 = 1e18, x2 = -1e18, y2 = -1e18;
    void add(const Info& s) {
        members.push_back(s.index);
        x1 = std::min(x1, s.x1);
        y1 = std::min(y1, s.y1);
        x2 = std::max(x2, s.x2);
        y2 = std::max(y2, s.y2);
    }
    double cx() const { return (x1 + x2) / 2; }
    double cy() const { return (y1 + y2) / 2; }
};
}  // namespace

std::vector<MarkerCandidate> findMarkers(const std::vector<StrokeShape>& strokes) {
    std::vector<Info> infos;
    for (size_t i = 0; i < strokes.size(); i++) {
        if (strokes[i].points.size() >= 1) {
            infos.push_back(info(i, strokes[i]));
        }
    }
    std::vector<bool> used(strokes.size(), false);
    std::vector<size_t> pos(strokes.size(), 0);  // stroke index → position in infos
    for (size_t k = 0; k < infos.size(); k++) {
        pos[infos[k].index] = k;
    }

    // 1. Asterisks: 2-4 short lines whose middles are close and that cross each other
    std::vector<Group> stars;
    for (size_t a = 0; a < infos.size(); a++) {
        if (used[infos[a].index] || !isShortLine(infos[a]) || isBar(infos[a])) {
            continue;
        }
        Group g;
        g.add(infos[a]);
        const StrokeShape& sa = strokes[infos[a].index];
        for (size_t b = 0; b < infos.size() && g.members.size() < 4; b++) {
            if (b == a || used[infos[b].index] || !isShortLine(infos[b])) {
                continue;
            }
            if (std::hypot(infos[b].cx() - infos[a].cx(), infos[b].cy() - infos[a].cy()) > 7) {
                continue;
            }
            const StrokeShape& sb = strokes[infos[b].index];
            if (segmentsCross(sa.points.front(), sa.points.back(), sb.points.front(), sb.points.back())) {
                g.add(infos[b]);
            }
        }
        if (g.members.size() >= 2) {
            for (size_t m: g.members) {
                used[m] = true;
            }
            stars.push_back(g);
        }
    }

    // 2. "!": an upright bar with a dot just below it
    std::vector<Group> bangs;
    for (const Info& bar: infos) {
        if (used[bar.index] || !isBar(bar)) {
            continue;
        }
        for (const Info& dot: infos) {
            if (used[dot.index] || dot.index == bar.index || !isDot(dot)) {
                continue;
            }
            const double below = dot.cy() - bar.y2;
            if (below >= 0.5 && below <= 12 && std::abs(dot.cx() - bar.cx()) <= 5) {
                Group g;
                g.add(bar);
                g.add(dot);
                used[bar.index] = used[dot.index] = true;
                bangs.push_back(g);
                break;
            }
        }
    }

    // 3. A marker: asterisk(s), then (maybe a letter), then "!" on the same line, close to the right
    std::vector<MarkerCandidate> out;
    std::vector<bool> starTaken(stars.size(), false);
    for (const Group& bang: bangs) {
        std::vector<size_t> mine;
        for (size_t i = 0; i < stars.size(); i++) {
            const Group& s = stars[i];
            if (starTaken[i] || s.x2 > bang.x1 + 3 || bang.x1 - s.x2 > 60) {
                continue;
            }
            if (s.cy() < bang.y1 - 12 || s.cy() > bang.y2 + 4) {
                continue;  // not on the same line
            }
            mine.push_back(i);
        }
        if (mine.empty()) {
            continue;
        }
        Group marker = bang;
        for (size_t i: mine) {
            starTaken[i] = true;
            for (size_t m: stars[i].members) {
                marker.add(infos[pos[m]]);
            }
        }
        // strokes between the last asterisk and the "!" form the letter
        double lastStarRight = -1e18;
        for (size_t i: mine) {
            lastStarRight = std::max(lastStarRight, stars[i].x2);
        }
        bool letter = false;
        for (const Info& s: infos) {
            if (used[s.index] || s.size() > 26) {
                continue;
            }
            if (s.x1 >= lastStarRight - 2 && s.x2 <= bang.x1 + 2 && s.cy() >= bang.y1 - 12 && s.cy() <= bang.y2 + 4) {
                marker.add(s);
                used[s.index] = true;
                letter = true;
            }
        }
        MarkerCandidate c;
        c.kind = letter ? "*?!" : (mine.size() >= 2 ? "**!" : "*!");
        for (size_t m: marker.members) {
            c.ids.push_back(strokes[m].id);
        }
        c.area = {marker.x1, marker.y1, marker.x2 - marker.x1, marker.y2 - marker.y1};
        out.push_back(c);
    }
    return out;
}

}  // namespace xoj::assistant
