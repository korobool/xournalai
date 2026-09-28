#include "Geometry.h"

#include <cmath>  // for hypot, round, pow

namespace xoj::api {

namespace {
double distanceToSegment(const Point& p, const Point& a, const Point& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 == 0.0) {
        return std::hypot(p.x - a.x, p.y - a.y);
    }
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

void rdp(const std::vector<Point>& pts, size_t first, size_t last, double tolerance, std::vector<bool>& keep) {
    // Iterative to avoid deep recursion on very long strokes
    std::vector<std::pair<size_t, size_t>> stack{{first, last}};
    while (!stack.empty()) {
        auto [a, b] = stack.back();
        stack.pop_back();
        if (b <= a + 1) {
            continue;
        }
        double maxDist = -1;
        size_t index = a;
        for (size_t i = a + 1; i < b; i++) {
            const double d = distanceToSegment(pts[i], pts[a], pts[b]);
            if (d > maxDist) {
                maxDist = d;
                index = i;
            }
        }
        if (maxDist > tolerance) {
            keep[index] = true;
            stack.emplace_back(a, index);
            stack.emplace_back(index, b);
        }
    }
}
}  // namespace

std::vector<Point> simplify(const std::vector<Point>& points, double tolerance) {
    if (tolerance <= 0 || points.size() < 3) {
        return points;
    }
    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;
    rdp(points, 0, points.size() - 1, tolerance, keep);
    std::vector<Point> out;
    for (size_t i = 0; i < points.size(); i++) {
        if (keep[i]) {
            out.push_back(points[i]);
        }
    }
    return out;
}

bool intersects(const xoj::util::Rectangle<double>& a, const xoj::util::Rectangle<double>& b) {
    return a.x <= b.x + b.width && b.x <= a.x + a.width && a.y <= b.y + b.height && b.y <= a.y + a.height;
}

double roundTo(double v, int decimals) {
    const double f = std::pow(10.0, decimals);
    return std::round(v * f) / f;
}

}  // namespace xoj::api
