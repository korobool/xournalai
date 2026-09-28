#include "Shapes.h"

#include <algorithm>  // for max
#include <cmath>      // for cos, sin, hypot, atan2, ceil
#include <stdexcept>  // for invalid_argument

#include "Pressure.h"  // for resample

namespace xoj::api::shapes {

namespace {
constexpr double PI = 3.14159265358979323846;

void requirePositive(double v, const char* what) {
    if (!(v > 0)) {
        throw std::invalid_argument(std::string(what) + " must be positive");
    }
}

int segmentsFor(double length, double spacing) { return std::max(8, static_cast<int>(std::ceil(length / spacing))); }
}  // namespace

Shape line(double x1, double y1, double x2, double y2, double spacing) {
    if (x1 == x2 && y1 == y2) {
        throw std::invalid_argument("A line needs two different points");
    }
    return {{resample({Point(x1, y1), Point(x2, y2)}, spacing)}, false};
}

Shape arrow(double x1, double y1, double x2, double y2, double headSize, bool both, double spacing) {
    if (x1 == x2 && y1 == y2) {
        throw std::invalid_argument("An arrow needs two different points");
    }
    requirePositive(headSize, "head_size");
    const double angle = std::atan2(y2 - y1, x2 - x1);
    const double spread = PI / 7;  // ~26 degrees on each side
    auto head = [&](double tx, double ty, double a) {
        return std::pair<Point, Point>{
                Point(tx - headSize * std::cos(a - spread), ty - headSize * std::sin(a - spread)),
                Point(tx - headSize * std::cos(a + spread), ty - headSize * std::sin(a + spread))};
    };
    std::vector<Point> path;
    if (both) {  // start head: wing, tip, wing, then the shaft
        auto [l, r] = head(x1, y1, angle + PI);
        path = {l, Point(x1, y1), r, Point(x1, y1)};
    } else {
        path = {Point(x1, y1)};
    }
    auto [l, r] = head(x2, y2, angle);
    path.insert(path.end(), {Point(x2, y2), l, Point(x2, y2), r});
    return {{resample(path, spacing)}, false};
}

Shape rectangle(double x, double y, double w, double h, double cornerRadius, double spacing) {
    requirePositive(w, "width");
    requirePositive(h, "height");
    const double r = std::clamp(cornerRadius, 0.0, std::min(w, h) / 2);
    std::vector<Point> path;
    if (r <= 0) {
        path = {Point(x, y), Point(x + w, y), Point(x + w, y + h), Point(x, y + h), Point(x, y)};
        return {{resample(path, spacing)}, true};
    }
    auto corner = [&](double cx, double cy, double fromDeg) {
        const int n = segmentsFor(PI * r / 2, spacing);
        for (int i = 0; i <= n; i++) {
            const double a = (fromDeg + 90.0 * i / n) * PI / 180;
            path.emplace_back(cx + r * std::cos(a), cy + r * std::sin(a));
        }
    };
    corner(x + w - r, y + r, -90);    // top right
    corner(x + w - r, y + h - r, 0);  // bottom right
    corner(x + r, y + h - r, 90);     // bottom left
    corner(x + r, y + r, 180);        // top left
    path.push_back(path.front());
    return {{resample(path, spacing)}, true};
}

Shape ellipse(double cx, double cy, double rx, double ry, double spacing) {
    requirePositive(rx, "rx");
    requirePositive(ry, "ry");
    const double circumference = PI * (3 * (rx + ry) - std::sqrt((3 * rx + ry) * (rx + 3 * ry)));
    const int n = segmentsFor(circumference, spacing);
    std::vector<Point> path;
    for (int i = 0; i <= n; i++) {
        const double a = 2 * PI * i / n - PI / 2;  // start at the top, like a hand-drawn circle
        path.emplace_back(cx + rx * std::cos(a), cy + ry * std::sin(a));
    }
    return {{path}, true};
}

Shape polygon(const std::vector<Point>& vertices, bool closed, double spacing) {
    if (vertices.size() < 2 || (closed && vertices.size() < 3)) {
        throw std::invalid_argument(closed ? "A polygon needs at least 3 points" :
                                             "A polyline needs at least 2 points");
    }
    std::vector<Point> path = vertices;
    if (closed) {
        path.push_back(vertices.front());
    }
    return {{resample(path, spacing)}, closed};
}

Shape bezier(const std::vector<Point>& cp, double spacing) {
    if (cp.size() < 4 || (cp.size() - 1) % 3 != 0) {
        throw std::invalid_argument("A bezier needs 3n+1 points: start, control1, control2, end, control1, ...");
    }
    std::vector<Point> path{cp.front()};
    for (size_t i = 0; i + 3 < cp.size(); i += 3) {
        const Point &p0 = cp[i], &c1 = cp[i + 1], &c2 = cp[i + 2], &p1 = cp[i + 3];
        const double approx = std::hypot(c1.x - p0.x, c1.y - p0.y) + std::hypot(c2.x - c1.x, c2.y - c1.y) +
                              std::hypot(p1.x - c2.x, p1.y - c2.y);
        const int n = segmentsFor(approx, spacing);
        for (int k = 1; k <= n; k++) {
            const double t = static_cast<double>(k) / n, u = 1 - t;
            path.emplace_back(u * u * u * p0.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * p1.x,
                              u * u * u * p0.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * p1.y);
        }
    }
    const bool closed = std::hypot(path.front().x - path.back().x, path.front().y - path.back().y) < 1e-6;
    return {{path}, closed};
}

Shape arc(double cx, double cy, double r, double startDeg, double endDeg, double spacing) {
    requirePositive(r, "r");
    if (startDeg == endDeg) {
        throw std::invalid_argument("An arc needs different start and end angles");
    }
    const double sweep = (endDeg - startDeg) * PI / 180;
    const int n = segmentsFor(std::abs(sweep) * r, spacing);
    std::vector<Point> path;
    for (int i = 0; i <= n; i++) {
        const double a = startDeg * PI / 180 + sweep * i / n;
        path.emplace_back(cx + r * std::cos(a), cy + r * std::sin(a));
    }
    return {{path}, false};
}

Shape coordinateSystem(double x, double y, double xLength, double yLength, double headSize, double spacing) {
    requirePositive(xLength, "x_length");
    requirePositive(yLength, "y_length");
    Shape s;
    s.strokes.push_back(arrow(x, y, x + xLength, y, headSize, false, spacing).strokes[0]);
    s.strokes.push_back(arrow(x, y, x, y - yLength, headSize, false, spacing).strokes[0]);
    return s;
}

}  // namespace xoj::api::shapes
