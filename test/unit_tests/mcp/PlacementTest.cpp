#include "config-features.h"

#ifdef ENABLE_MCP

#include <stdexcept>

#include <gtest/gtest.h>

#include "api/Placement.h"

using namespace xoj::api;

TEST(Placement, readingOrderOnEmptyPage) {
    auto r = findFreeSpace({}, 600, 800, 100, 50, 10);
    ASSERT_TRUE(r);
    EXPECT_NEAR(r->x, 12, 0.01);  // first grid cell after the margin
    EXPECT_NEAR(r->y, 12, 0.01);
}

TEST(Placement, avoidsContentAndPageEdges) {
    std::vector<Rect> occupied = {{0, 0, 600, 300}};
    auto r = findFreeSpace(occupied, 600, 800, 200, 100, 10);
    ASSERT_TRUE(r);
    EXPECT_GE(r->y, 310);
    EXPECT_FALSE(findFreeSpace(occupied, 600, 800, 700, 100, 10).has_value());  // wider than the page
    std::vector<Rect> full = {{0, 0, 600, 800}};
    EXPECT_FALSE(findFreeSpace(full, 600, 800, 10, 10, 5).has_value());
}

TEST(Placement, nearElementOnASide) {
    std::vector<Rect> occupied = {{100, 100, 80, 40}};
    Rect near(100, 100, 80, 40);
    auto right = findFreeSpace(occupied, 600, 800, 50, 30, 8, near, "right");
    ASSERT_TRUE(right);
    EXPECT_GE(right->x, 188);
    EXPECT_LE(right->x, 200);
    EXPECT_NEAR(right->y, 100, 4);  // aligned with the element
    auto below = findFreeSpace(occupied, 600, 800, 50, 30, 8, near, "below");
    ASSERT_TRUE(below);
    EXPECT_GE(below->y, 148);
    EXPECT_NEAR(below->x, 100, 4);
    EXPECT_THROW(findFreeSpace(occupied, 600, 800, 50, 30, 8, near, "diagonal"), std::invalid_argument);
}

#endif  // ENABLE_MCP
