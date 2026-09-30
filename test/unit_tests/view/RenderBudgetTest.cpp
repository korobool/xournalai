#include <gtest/gtest.h>

#include "view/RenderBudget.h"

using xoj::view::fitsInOneBuffer;
using xoj::view::partialExtent;

namespace {
const Range A4(0, 0, 595, 842);
}

TEST(RenderBudget, WholePagesFitUpToTheBudget) {
    EXPECT_TRUE(fitsInOneBuffer(A4, 1.0));
    EXPECT_TRUE(fitsInOneBuffer(A4, 7.0));                        // the normal maximum
    EXPECT_FALSE(fitsInOneBuffer(A4, 30.0));                      // deep zoom
    EXPECT_FALSE(fitsInOneBuffer(Range(0, 0, 100, 20000), 1.0));  // a very tall page: too long a side
}

TEST(RenderBudget, PartialExtentCoversTheVisiblePartAndAMargin) {
    const double ppu = 40;  // 3000% at 96 dpi
    const Range visible(100, 200, 135, 220);
    const Range r = partialExtent(A4, visible, ppu);
    EXPECT_LE(r.minX, visible.minX);
    EXPECT_LE(r.minY, visible.minY);
    EXPECT_GE(r.maxX, visible.maxX);
    EXPECT_GE(r.maxY, visible.maxY);
    EXPECT_GT(r.getWidth(), visible.getWidth());  // a margin for scrolling
    EXPECT_TRUE(fitsInOneBuffer(r, ppu));
}

TEST(RenderBudget, PartialExtentStaysOnThePage) {
    const Range r = partialExtent(A4, Range(0, 0, 30, 20), 40);
    EXPECT_GE(r.minX, 0);
    EXPECT_GE(r.minY, 0);
    EXPECT_LE(r.maxX, 595);
    EXPECT_LE(r.maxY, 842);
}

TEST(RenderBudget, PartialExtentShrinksTheMarginToTheBudget) {
    const double ppu = 80;
    const Range big(100, 100, 150, 180);  // 4000 x 6400 px: fits, but not with a full margin
    const Range r = partialExtent(A4, big, ppu);
    EXPECT_TRUE(fitsInOneBuffer(r, ppu));
    EXPECT_LE(r.minX, big.minX);
    EXPECT_GE(r.maxY, big.maxY);
}

TEST(RenderBudget, NothingVisibleRendersTheTopLeftCorner) {
    const Range r = partialExtent(A4, Range(), 40);
    EXPECT_EQ(r.minX, 0);
    EXPECT_EQ(r.minY, 0);
    EXPECT_TRUE(fitsInOneBuffer(r, 40));
}
