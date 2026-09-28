#include "config-features.h"

#ifdef ENABLE_MCP

#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "api/Pressure.h"

using namespace xoj::api;

namespace {
std::vector<Point> line(double x1, double y1, double x2, double y2) { return {Point(x1, y1), Point(x2, y2)}; }
}  // namespace

TEST(Pressure, hardwareMappingMatchesStylusFormula) {
    PressureSettings s{0.1, 1.5, true};
    EXPECT_DOUBLE_EQ(hardwareWidth(0.5, 2.0, s), 0.75 * 2.0);  // p * multiplier
    EXPECT_DOUBLE_EQ(hardwareWidth(0.01, 2.0, s), 0.1 * 2.0);  // clamped to the minimum pressure
    PressureSettings defaults;
    EXPECT_DOUBLE_EQ(hardwareWidth(1.0, 1.41, defaults), 1.41);
}

TEST(Pressure, resampleSpacing) {
    auto pts = resample(line(0, 0, 10, 0), 1.0);
    ASSERT_EQ(pts.size(), 11u);
    for (size_t i = 1; i < pts.size(); i++) {
        EXPECT_NEAR(pts[i].x - pts[i - 1].x, 1.0, 1e-9);
    }
    EXPECT_EQ(resample(line(0, 0, 0.5, 0), 1.0).size(), 2u);
}

TEST(Pressure, inkTapersAtBothEnds) {
    auto pts = resample(line(0, 0, 200, 0), 1.0);
    PressureProfile ink;
    auto p = profilePressures(pts, ink);
    const double middle = p[p.size() / 2];
    EXPECT_LT(p.front(), middle * 0.6);
    EXPECT_LT(p.back(), middle * 0.6);
    EXPECT_NEAR(middle, 0.75, 0.1);  // base level with a little variation
}

TEST(Pressure, constantAndMarkerAreFlat) {
    auto pts = resample(line(0, 0, 100, 0), 2.0);
    for (const char* preset: {"constant", "marker"}) {
        PressureProfile pr;
        pr.preset = preset;
        for (double v: profilePressures(pts, pr)) {
            EXPECT_DOUBLE_EQ(v, 1.0);
        }
    }
}

TEST(Pressure, brushSwellsInTheMiddle) {
    auto pts = resample(line(0, 0, 300, 0), 1.0);
    PressureProfile brush;
    brush.preset = "brush";
    brush.variation = 0;
    auto p = profilePressures(pts, brush);
    EXPECT_GT(p[p.size() / 2], p[p.size() / 10] + 0.1);
    EXPECT_GT(p[p.size() / 2], p[p.size() * 9 / 10] + 0.1);
}

TEST(Pressure, calligraphyDependsOnDirection) {
    PressureProfile cal;
    cal.preset = "calligraphy";
    cal.nibAngle = 0;  // horizontal nib: horizontal strokes thin, vertical strokes thick
    cal.variation = 0;
    cal.taperIn = 0;
    cal.taperOut = 0;
    auto h = profilePressures(resample(line(0, 0, 100, 0), 1.0), cal);
    auto v = profilePressures(resample(line(0, 0, 0, 100), 1.0), cal);
    EXPECT_LT(h[50], 0.35);
    EXPECT_GT(v[50], 0.9);
}

TEST(Pressure, speedAwareThinsFastSegments) {
    auto pts = resample(line(0, 0, 100, 0), 1.0);
    std::vector<double> times;
    double t = 0;
    for (size_t i = 0; i < pts.size(); i++) {
        t += i < 50 ? 10 : 1;  // slow first half, fast second half
        times.push_back(t);
    }
    PressureProfile pr;
    pr.preset = "constant";
    pr.base = 0.8;
    pr.min = 0.8;
    auto p = profilePressures(pts, pr, &times);
    EXPECT_GT(p[30], p[70]);
    pr.speedAware = false;
    auto flat = profilePressures(pts, pr, &times);
    EXPECT_DOUBLE_EQ(flat[30], flat[70]);
}

TEST(Pressure, seedsAreReproducible) {
    auto pts = resample(line(0, 0, 150, 30), 1.0);
    PressureProfile a, b;
    a.seed = b.seed = 7;
    EXPECT_EQ(profilePressures(pts, a), profilePressures(pts, b));
    b.seed = 8;
    EXPECT_NE(profilePressures(pts, a), profilePressures(pts, b));
}

TEST(Pressure, validationAndApply) {
    PressureProfile bad;
    bad.preset = "crayon";
    EXPECT_THROW(bad.validate(), std::invalid_argument);
    PressureProfile range;
    range.base = 1.5;
    EXPECT_THROW(range.validate(), std::invalid_argument);

    std::vector<Point> pts = {Point(0, 0), Point(1, 0)};
    applyPressures(pts, {0.5, 1.0}, 2.0, PressureSettings{});
    EXPECT_DOUBLE_EQ(pts[0].z, 1.0);
    EXPECT_DOUBLE_EQ(pts[1].z, 2.0);
    EXPECT_THROW(applyPressures(pts, {0.5}, 2.0, PressureSettings{}), std::invalid_argument);
}

TEST(Pressure, tremorKeepsEndsAndStaysSmall) {
    auto pts = resample(line(0, 0, 100, 0), 1.0);
    auto shaky = pts;
    addTremor(shaky, 0.5, 3);
    EXPECT_DOUBLE_EQ(shaky.front().x, pts.front().x);
    EXPECT_DOUBLE_EQ(shaky.back().y, pts.back().y);
    double maxDev = 0;
    for (const auto& p: shaky) {
        maxDev = std::max(maxDev, std::abs(p.y));
    }
    EXPECT_GT(maxDev, 0.05);
    EXPECT_LE(maxDev, 0.5 + 1e-9);
}

#endif  // ENABLE_MCP
