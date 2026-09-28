#include "Pressure.h"

#include <algorithm>  // for clamp, max, min
#include <cmath>      // for sin, cos, atan2, hypot, pow
#include <random>     // for mt19937
#include <stdexcept>  // for invalid_argument

namespace xoj::api {

namespace {
constexpr double PI = 3.14159265358979323846;

/// Smooth pseudo-random signal in [-1, 1] along the arc length s (sum of a few sines with seeded phases)
struct SmoothNoise {
    double phase[3];
    double freq[3];
    explicit SmoothNoise(unsigned seed) {
        std::mt19937 rng(seed * 2654435761U + 17U);
        std::uniform_real_distribution<double> ph(0, 2 * PI);
        std::uniform_real_distribution<double> fr(0.8, 1.25);
        const double base[3] = {1.0 / 23.0, 1.0 / 11.0, 1.0 / 5.3};  // wavelengths in points: ~145, ~70, ~33
        for (int i = 0; i < 3; i++) {
            phase[i] = ph(rng);
            freq[i] = base[i] * fr(rng);
        }
    }
    double operator()(double s) const {
        return (0.55 * std::sin(freq[0] * s + phase[0]) + 0.3 * std::sin(freq[1] * s + phase[1]) +
                0.15 * std::sin(freq[2] * s + phase[2]));
    }
};

struct PresetDefaults {
    double base, min, taperIn, taperOut, variation;
};

PresetDefaults defaultsFor(const std::string& preset) {
    if (preset == "constant" || preset == "marker") {
        return {1.0, 1.0, 0, 0, 0};
    }
    if (preset == "brush") {
        return {0.95, 0.15, 14, 22, 0.12};
    }
    if (preset == "pencil") {
        return {0.45, 0.3, 3, 5, 0.06};
    }
    if (preset == "calligraphy") {
        return {1.0, 0.25, 3, 4, 0.04};
    }
    return {0.75, 0.3, 6, 9, 0.08};  // ink
}

double smoothstep(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3 - 2 * t);
}
}  // namespace

double hardwareWidth(double pressure, double toolWidth, const PressureSettings& settings) {
    return std::max(settings.minimumPressure, pressure * settings.multiplier) * toolWidth;
}

const std::vector<std::string>& PressureProfile::presets() {
    static const std::vector<std::string> p = {"constant", "ink", "brush", "pencil", "calligraphy", "marker"};
    return p;
}

void PressureProfile::validate() const {
    if (std::find(presets().begin(), presets().end(), preset) == presets().end()) {
        throw std::invalid_argument("Unknown pressure profile '" + preset +
                                    "' (constant, ink, brush, pencil, calligraphy, marker)");
    }
    auto unit = [](const std::optional<double>& v, const char* name) {
        if (v && (*v < 0 || *v > 1)) {
            throw std::invalid_argument(std::string("Profile '") + name + "' must be between 0 and 1");
        }
    };
    unit(base, "base");
    unit(min, "min");
    unit(variation, "variation");
    if ((taperIn && *taperIn < 0) || (taperOut && *taperOut < 0)) {
        throw std::invalid_argument("Profile tapers must not be negative");
    }
}

std::vector<Point> resample(const std::vector<Point>& points, double spacing) {
    if (points.size() < 2 || spacing <= 0) {
        return points;
    }
    std::vector<Point> out;
    out.reserve(points.size() * 2);
    out.push_back(points.front());
    for (size_t i = 1; i < points.size(); i++) {
        const Point& a = points[i - 1];
        const Point& b = points[i];
        const double d = std::hypot(b.x - a.x, b.y - a.y);
        const auto steps = static_cast<int>(std::ceil(d / spacing));
        for (int k = 1; k < steps; k++) {
            const double t = static_cast<double>(k) / steps;
            out.emplace_back(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
        }
        out.push_back(b);
    }
    return out;
}

std::vector<double> profilePressures(const std::vector<Point>& points, const PressureProfile& profile,
                                     const std::vector<double>* times) {
    profile.validate();
    const size_t n = points.size();
    std::vector<double> p(n, 1.0);
    if (n == 0) {
        return p;
    }
    const PresetDefaults d = defaultsFor(profile.preset);
    const double base = profile.base.value_or(d.base);
    const double lo = std::min(profile.min.value_or(d.min), base);
    const double variation = profile.variation.value_or(d.variation);

    // Arc length
    std::vector<double> s(n, 0.0);
    for (size_t i = 1; i < n; i++) {
        s[i] = s[i - 1] + std::hypot(points[i].x - points[i - 1].x, points[i].y - points[i - 1].y);
    }
    const double length = s.back();
    // Tapers never take more than 40% of the stroke each
    const double taperIn = std::min(profile.taperIn.value_or(d.taperIn), 0.4 * length);
    const double taperOut = std::min(profile.taperOut.value_or(d.taperOut), 0.4 * length);
    const SmoothNoise noise(profile.seed);

    // Speed factors from timestamps
    std::vector<double> speedFactor(n, 1.0);
    if (profile.speedAware && times && times->size() == n && n > 2) {
        std::vector<double> speed(n, 0.0);
        for (size_t i = 1; i < n; i++) {
            const double dt = std::max((*times)[i] - (*times)[i - 1], 1e-3);
            speed[i] = (s[i] - s[i - 1]) / dt;
        }
        speed[0] = speed[1];
        std::vector<double> sorted = speed;
        std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(n / 2), sorted.end());
        const double median = std::max(sorted[n / 2], 1e-6);
        for (size_t i = 0; i < n; i++) {
            speedFactor[i] = std::clamp(1.15 - 0.3 * (speed[i] / median - 1), 0.6, 1.2);
        }
    }

    const double nib = profile.nibAngle * PI / 180.0;
    for (size_t i = 0; i < n; i++) {
        double v = base;
        if (profile.preset == "brush" && length > 0) {  // swell in the middle of the stroke
            v = lo + (base - lo) * std::pow(std::sin(PI * std::clamp(s[i] / length, 0.0, 1.0)), 0.6);
        } else if (profile.preset == "calligraphy") {  // broad nib: thick across the nib, thin along it
            const size_t a = i == 0 ? 0 : i - 1, b = i + 1 < n ? i + 1 : n - 1;
            const double dir = std::atan2(points[b].y - points[a].y, points[b].x - points[a].x);
            v = lo + (base - lo) * std::abs(std::sin(dir - nib));
        }
        if (taperIn > 0 && s[i] < taperIn) {
            v = lo + (v - lo) * smoothstep(s[i] / taperIn);
        }
        if (taperOut > 0 && length - s[i] < taperOut) {
            v = lo + (v - lo) * smoothstep((length - s[i]) / taperOut);
        }
        v *= 1 + variation * noise(s[i]);
        v *= speedFactor[i];
        p[i] = std::clamp(v, 0.02, 1.0);
    }
    return p;
}

void applyPressures(std::vector<Point>& points, const std::vector<double>& pressures, double toolWidth,
                    const PressureSettings& settings) {
    if (pressures.size() != points.size()) {
        throw std::invalid_argument("pressure list must have one value per point");
    }
    for (size_t i = 0; i < points.size(); i++) {
        points[i].z = hardwareWidth(pressures[i], toolWidth, settings);
    }
}

void addTremor(std::vector<Point>& points, double amplitude, unsigned seed) {
    if (points.size() < 3 || amplitude <= 0) {
        return;
    }
    const SmoothNoise nx(seed * 3 + 1), ny(seed * 3 + 2);
    double s = 0;
    for (size_t i = 1; i + 1 < points.size(); i++) {
        s += std::hypot(points[i].x - points[i - 1].x, points[i].y - points[i - 1].y);
        // Faster wobble than the pressure noise: scale the arc length up
        points[i].x += amplitude * nx(s * 4);
        points[i].y += amplitude * ny(s * 4);
    }
}

}  // namespace xoj::api
