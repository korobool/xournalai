#include "config-features.h"

#ifdef ENABLE_MCP

#include <stdexcept>

#include <gtest/gtest.h>

#include "api/SvgImport.h"

using namespace xoj::api::svg;

namespace {
const char* SVG = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100" width="200" height="100">
  <rect id="box" x="10" y="10" width="80" height="40" stroke="#ff0000" fill="none" stroke-width="2"/>
  <circle id="dot" cx="150" cy="50" r="30" fill="#0000ff"/>
  <path id="curve" d="M10 90 C 50 60, 100 120, 190 80" stroke="black" fill="none" stroke-dasharray="4 2"/>
  <g transform="translate(100,0)"><line x1="0" y1="95" x2="50" y2="95" stroke="#00ff00"/></g>
  <text x="20" y="35" font-size="10" fill="#333333">Label &amp; more</text>
</svg>)SVG";
}

TEST(SvgImport, shapesColorsAndText) {
    Placement p;
    auto r = convert(SVG, p);
    ASSERT_EQ(r.texts.size(), 1u);
    EXPECT_EQ(r.texts[0].text, "Label & more");
    EXPECT_NEAR(r.texts[0].y, 27, 1e-9);  // baseline 35 - 0.8 * 10
    // rect (outline), circle (fill only), curve (dashed), line
    ASSERT_EQ(r.strokes.size(), 4u);
    EXPECT_EQ(r.strokes[0].color, Color(0xff, 0, 0));
    EXPECT_DOUBLE_EQ(r.strokes[0].width, 2);
    EXPECT_FALSE(r.strokes[0].fill.has_value());
    EXPECT_TRUE(r.strokes[1].fill.has_value());
    EXPECT_EQ(r.strokes[1].color, Color(0, 0, 0xff));
    EXPECT_TRUE(r.strokes[2].dashed);
    EXPECT_NEAR(r.strokes[3].points.front().x, 100, 1e-6);  // group transform applied
    // closed rectangle
    EXPECT_NEAR(r.strokes[0].points.front().x, r.strokes[0].points.back().x, 1e-6);
    EXPECT_NEAR(r.bounds.width, 200, 1e-9);
}

TEST(SvgImport, fitIntoTarget) {
    Placement p;
    p.target = xoj::util::Rectangle<double>(300, 400, 100, 100);
    auto r = convert(SVG, p);
    EXPECT_NEAR(r.bounds.width, 100, 1e-6);  // 200 wide → scaled by 0.5
    EXPECT_NEAR(r.bounds.x, 300, 1e-6);
    EXPECT_NEAR(r.bounds.y, 425, 1e-6);  // centered vertically (height 50 in 100)
    for (const auto& s: r.strokes) {
        for (const auto& pt: s.points) {
            EXPECT_GE(pt.x, 299);
            EXPECT_LE(pt.x, 401);
        }
    }
    EXPECT_NEAR(r.texts[0].size, 5, 1e-9);
}

TEST(SvgImport, errors) {
    EXPECT_THROW(convert("hello", Placement{}), std::invalid_argument);
    EXPECT_THROW(convert("<svg xmlns='http://www.w3.org/2000/svg'></svg>", Placement{}), std::invalid_argument);
}

#endif  // ENABLE_MCP
