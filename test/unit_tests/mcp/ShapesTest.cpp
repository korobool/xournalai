#include "config-features.h"

#ifdef ENABLE_MCP

#include <cmath>
#include <stdexcept>

#include <gtest/gtest.h>

#include "api/Shapes.h"

using namespace xoj::api::shapes;

TEST(Shapes, lineAndArrow) {
    auto l = line(0, 0, 10, 0);
    ASSERT_EQ(l.strokes.size(), 1u);
    EXPECT_EQ(l.strokes[0].size(), 11u);  // 1 pt spacing
    auto a = arrow(0, 0, 100, 0, 10, false);
    const auto& p = a.strokes[0];
    EXPECT_DOUBLE_EQ(p.front().x, 0);
    double maxX = 0;
    for (const auto& q: p) {
        maxX = std::max(maxX, q.x);
    }
    EXPECT_DOUBLE_EQ(maxX, 100);  // tip
    EXPECT_LT(p.back().x, 100);   // ends on a wing
    EXPECT_THROW(line(1, 1, 1, 1), std::invalid_argument);
}

TEST(Shapes, rectangleIsClosedAndSized) {
    auto r = rectangle(10, 20, 100, 50);
    EXPECT_TRUE(r.closed);
    const auto& p = r.strokes[0];
    EXPECT_DOUBLE_EQ(p.front().x, p.back().x);
    EXPECT_DOUBLE_EQ(p.front().y, p.back().y);
    double maxX = 0, maxY = 0;
    for (const auto& q: p) {
        maxX = std::max(maxX, q.x);
        maxY = std::max(maxY, q.y);
    }
    EXPECT_DOUBLE_EQ(maxX, 110);
    EXPECT_DOUBLE_EQ(maxY, 70);
    auto rounded = rectangle(0, 0, 40, 40, 10);
    EXPECT_GT(rounded.strokes[0].size(), r.strokes[0].size() / 3);
    EXPECT_THROW(rectangle(0, 0, -1, 5), std::invalid_argument);
}

TEST(Shapes, ellipseArcBezier) {
    auto e = ellipse(50, 50, 20, 10);
    for (const auto& q: e.strokes[0]) {
        const double v = std::pow((q.x - 50) / 20, 2) + std::pow((q.y - 50) / 10, 2);
        EXPECT_NEAR(v, 1.0, 1e-9);
    }
    auto a = arc(0, 0, 10, 0, 90);
    EXPECT_NEAR(a.strokes[0].front().x, 10, 1e-9);
    EXPECT_NEAR(a.strokes[0].back().y, 10, 1e-9);  // clockwise on the page: +90 deg points down
    auto b = bezier({Point(0, 0), Point(0, 10), Point(10, 10), Point(10, 0)});
    EXPECT_NEAR(b.strokes[0].back().x, 10, 1e-9);
    EXPECT_FALSE(b.closed);
    EXPECT_THROW(bezier({Point(0, 0), Point(1, 1)}), std::invalid_argument);
}

TEST(Shapes, polygonAndAxes) {
    auto t = polygon({Point(0, 0), Point(10, 0), Point(5, 8)}, true);
    EXPECT_TRUE(t.closed);
    EXPECT_THROW(polygon({Point(0, 0), Point(1, 0)}, true), std::invalid_argument);
    auto axes = coordinateSystem(0, 100, 50, 40, 5);
    ASSERT_EQ(axes.strokes.size(), 2u);
    double minY = 1e9;
    for (const auto& q: axes.strokes[1]) {
        minY = std::min(minY, q.y);
    }
    EXPECT_DOUBLE_EQ(minY, 60);  // y axis goes up
}

#endif  // ENABLE_MCP
