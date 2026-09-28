#include "config-features.h"

#ifdef ENABLE_MCP

#include <gtest/gtest.h>

#include "api/Layout.h"

using namespace xoj::api;
using Kind = LayoutItem::Kind;

namespace {
struct Builder {
    std::vector<LayoutItem> items;
    /// A handwriting-like stroke: small box, wiggly (path much longer than chord)
    size_t glyph(double x, double y, double w = 6, double h = 10) {
        LayoutItem it;
        it.index = items.size();
        it.kind = Kind::PenStroke;
        it.bbox = {x, y, w, h};
        it.first = {x, y + h};
        it.last = {x + w, y + h};
        it.length = 3 * (w + h);
        items.push_back(it);
        return it.index;
    }
    /// A straight stroke from (x1,y1) to (x2,y2)
    size_t line(double x1, double y1, double x2, double y2) {
        LayoutItem it;
        it.index = items.size();
        it.kind = Kind::PenStroke;
        it.bbox = {std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1) + 0.5, std::abs(y2 - y1) + 0.5};
        it.first = {x1, y1};
        it.last = {x2, y2};
        it.length = std::hypot(x2 - x1, y2 - y1);
        items.push_back(it);
        return it.index;
    }
    void word(double x, double y, int letters) {
        for (int i = 0; i < letters; i++) {
            glyph(x + i * 7, y);
        }
    }
};
}  // namespace

TEST(Layout, handwritingLinesFormAParagraph) {
    Builder b;
    b.word(50, 50, 6);
    b.word(100, 50, 5);  // same line, next word
    b.word(50, 66, 8);   // next line
    b.word(50, 300, 4);  // far below: a separate paragraph
    auto layout = analyzeLayout(b.items);
    ASSERT_EQ(layout.blocks.size(), 2u);
    EXPECT_EQ(layout.blocks[0].kind, "handwriting");
    EXPECT_EQ(layout.blocks[0].lines.size(), 2u);
    EXPECT_EQ(layout.blocks[0].items.size(), 19u);
    EXPECT_EQ(layout.blocks[1].items.size(), 4u);
    EXPECT_NEAR(layout.typicalHeight, 10, 0.01);
}

TEST(Layout, boxOfFourStrokesIsAFigureWithLabel) {
    Builder b;
    b.word(50, 50, 3);  // some text so the typical height is realistic
    // box 200..320 x 200..280 drawn with four separate straight strokes
    b.line(200, 200, 320, 200);
    b.line(320, 200, 320, 280);
    b.line(320, 280, 200, 280);
    b.line(200, 280, 200, 200);
    b.word(240, 235, 4);  // label inside the box
    auto layout = analyzeLayout(b.items);
    const LayoutBlock* figure = nullptr;
    const LayoutBlock* label = nullptr;
    for (const auto& blk: layout.blocks) {
        if (blk.kind == "figure") {
            figure = &blk;
        } else if (blk.bbox.x > 230) {
            label = &blk;
        }
    }
    ASSERT_NE(figure, nullptr);
    EXPECT_EQ(figure->items.size(), 4u);
    EXPECT_TRUE(layout.connectors.empty());
    ASSERT_NE(label, nullptr);
    ASSERT_TRUE(label->parent.has_value());
    EXPECT_EQ(layout.blocks[*label->parent].kind, "figure");
}

TEST(Layout, arrowBetweenTwoBlocksIsAConnector) {
    Builder b;
    b.word(50, 100, 5);   // left block
    b.word(400, 100, 5);  // right block
    const size_t arrow = b.line(90, 105, 395, 105);
    auto layout = analyzeLayout(b.items);
    ASSERT_EQ(layout.connectors.size(), 1u);
    const auto& c = layout.connectors[0];
    EXPECT_EQ(c.item, arrow);
    ASSERT_TRUE(c.fromBlock && c.toBlock);
    EXPECT_NE(*c.fromBlock, *c.toBlock);
    EXPECT_LT(layout.blocks[*c.fromBlock].bbox.x, layout.blocks[*c.toBlock].bbox.x);
}

TEST(Layout, otherKindsAndReadingOrder) {
    Builder b;
    LayoutItem text;
    text.index = b.items.size();
    text.kind = Kind::Text;
    text.bbox = {300, 20, 100, 14};
    b.items.push_back(text);
    b.word(20, 22, 4);  // same band as the text, but further left
    LayoutItem hl;
    hl.index = b.items.size();
    hl.kind = Kind::Highlighter;
    hl.bbox = {20, 200, 150, 12};
    b.items.push_back(hl);
    auto layout = analyzeLayout(b.items);
    ASSERT_EQ(layout.blocks.size(), 3u);
    EXPECT_EQ(layout.blocks[0].kind, "handwriting");  // left first within the band
    EXPECT_EQ(layout.blocks[1].kind, "typed_text");
    EXPECT_EQ(layout.blocks[2].kind, "highlight");
}

TEST(Layout, emptyInput) {
    auto layout = analyzeLayout({});
    EXPECT_TRUE(layout.blocks.empty());
    EXPECT_TRUE(layout.connectors.empty());
}

#endif  // ENABLE_MCP
