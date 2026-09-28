#include "config-features.h"

#ifdef ENABLE_MCP

#include <cmath>
#include <memory>

#include <gtest/gtest.h>

#include "api/DocumentApi.h"
#include "api/Geometry.h"
#include "mcp/ElementJson.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/XojPage.h"

using namespace xoj;

TEST(ApiGeometry, simplifyKeepsShapeAndPressure) {
    std::vector<Point> line;
    for (int i = 0; i <= 100; i++) {
        line.emplace_back(i, 0.001 * (i % 2), 0.5 + i / 200.0);  // almost straight
    }
    auto s = api::simplify(line, 0.1);
    ASSERT_EQ(s.size(), 2u);
    EXPECT_DOUBLE_EQ(s.front().z, line.front().z);
    EXPECT_DOUBLE_EQ(s.back().z, line.back().z);

    std::vector<Point> corner = {{0, 0}, {5, 0}, {10, 0}, {10, 5}, {10, 10}};
    auto c = api::simplify(corner, 0.1);
    ASSERT_EQ(c.size(), 3u);  // the corner survives
    EXPECT_DOUBLE_EQ(c[1].x, 10);
    EXPECT_DOUBLE_EQ(c[1].y, 0);

    EXPECT_EQ(api::simplify(corner, 0).size(), corner.size());
    EXPECT_DOUBLE_EQ(api::roundTo(1.23456), 1.23);
}

TEST(ApiDocument, elementsOnPageFiltersByLayerAndRegion) {
    auto page = std::make_shared<XojPage>(200, 200);
    auto* layer2 = new Layer();
    page->getLayers().push_back(layer2);  // owned by the page from now on
    auto s1 = std::make_unique<Stroke>();
    s1->addPoint(Point(10, 10));
    s1->addPoint(Point(20, 20));
    auto s2 = std::make_unique<Stroke>();
    s2->addPoint(Point(150, 150));
    s2->addPoint(Point(160, 160));
    page->getLayers()[0]->addElement(std::move(s1));
    layer2->addElement(std::move(s2));

    EXPECT_EQ(api::elementsOnPage(page, 0, std::nullopt, std::nullopt).size(), 2u);
    auto onlyLayer2 = api::elementsOnPage(page, 0, 2, std::nullopt);
    ASSERT_EQ(onlyLayer2.size(), 1u);
    EXPECT_EQ(onlyLayer2[0].layer, 2u);
    auto inRegion = api::elementsOnPage(page, 0, std::nullopt, xoj::util::Rectangle<double>(0, 0, 50, 50));
    ASSERT_EQ(inRegion.size(), 1u);
    EXPECT_EQ(inRegion[0].layer, 1u);
}

TEST(ElementJson, strokeDetailLevels) {
    Stroke s;
    s.setWidth(2);
    s.setColor(Color(0xff, 0, 0));
    for (int i = 0; i <= 20; i++) {
        s.addPoint(Point(i, std::sin(i / 3.0) * 10, 1 + i / 20.0));
    }
    api::ElementLocation loc{0, 1, 0, &s};
    auto bbox = mcp::elementToJson(loc, mcp::Detail::Bbox, 0.5);
    EXPECT_EQ(bbox["type"], "stroke");
    EXPECT_EQ(bbox["color"], "#ff0000");
    EXPECT_EQ(bbox["point_count"], 21);
    EXPECT_FALSE(bbox.contains("points"));

    auto full = mcp::elementToJson(loc, mcp::Detail::Full, 0.5);
    EXPECT_EQ(full["points"].size(), 21u);
    EXPECT_EQ(full["points"][0].size(), 3u);  // pressure -> [x, y, w]
    EXPECT_EQ(full["pressure"], true);
    EXPECT_EQ(full["line_style"], "plain");

    auto simplified = mcp::elementToJson(loc, mcp::Detail::Simplified, 0.5);
    EXPECT_LT(simplified["points"].size(), 21u);
    EXPECT_EQ(simplified["id"], full["id"]);
}

TEST(ElementJson, textElement) {
    Text t;
    t.setText("Hello xournalai");
    api::ElementLocation loc{0, 2, 0, &t};
    auto j = mcp::elementToJson(loc, mcp::Detail::Bbox, 0);
    EXPECT_EQ(j["type"], "text");
    EXPECT_EQ(j["text"], "Hello xournalai");
    EXPECT_EQ(j["layer"], 2);
    EXPECT_TRUE(j["font"].contains("size"));
}

#endif  // ENABLE_MCP
