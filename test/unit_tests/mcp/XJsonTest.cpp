#include "config-features.h"

#ifdef ENABLE_MCP

#include <cairo.h>
#include <gtest/gtest.h>

#include "mcp/Media.h"
#include "mcp/Registry.h"
#include "mcp/XJson.h"
#include "model/Image.h"
#include "model/Link.h"
#include "model/Stroke.h"
#include "model/StrokeStyle.h"
#include "model/Text.h"

using namespace xoj::mcp;

namespace {
std::vector<ElementPtr> roundTrip(const std::vector<const Element*>& in, std::vector<std::string>& warnings) {
    json doc = xjson::serialize(in, {{"page", 1}});
    EXPECT_EQ(doc["format"], xjson::FORMAT);
    // Through text, like a real exchange with an agent
    return xjson::parse(json::parse(doc.dump()), warnings);
}

std::string tinyPng() {
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 10);
    cairo_t* cr = cairo_create(s);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    auto png = encodePng(s);
    cairo_surface_destroy(s);
    return png;
}
}  // namespace

TEST(XJson, strokeRoundTripIsLossless) {
    Stroke s;
    s.setWidth(2.5);
    s.setColor(Color(0x12, 0x34, 0x56, 0x80));
    s.setLineStyle(StrokeStyle::parseStyle("dash"));
    s.setFill(128);
    s.setStrokeCapStyle(StrokeCapStyle::BUTT);
    s.addPoint(Point(10.125, 20.5, 1.25));
    s.addPoint(Point(30.75, 40.0625, 2.5));
    s.addPoint(Point(50, 60, 0.5));
    std::vector<std::string> w;
    auto out = roundTrip({&s}, w);
    ASSERT_EQ(out.size(), 1u);
    auto* r = dynamic_cast<Stroke*>(out[0].get());
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->getColor(), s.getColor());
    EXPECT_DOUBLE_EQ(r->getWidth(), 2.5);
    EXPECT_EQ(StrokeStyle::formatStyle(r->getLineStyle()), "dash");
    EXPECT_EQ(r->getFill(), 128);
    EXPECT_EQ(r->getStrokeCapStyle(), StrokeCapStyle::BUTT);
    ASSERT_EQ(r->getPointCount(), 3u);
    for (size_t i = 0; i < 3; i++) {
        EXPECT_DOUBLE_EQ(r->getPointVector()[i].x, s.getPointVector()[i].x);
        EXPECT_DOUBLE_EQ(r->getPointVector()[i].y, s.getPointVector()[i].y);
        EXPECT_DOUBLE_EQ(r->getPointVector()[i].z, s.getPointVector()[i].z);
    }
    EXPECT_TRUE(w.empty());
}

TEST(XJson, highlighterWithoutPressure) {
    Stroke s;
    s.setToolType(StrokeTool::HIGHLIGHTER);
    s.setWidth(8);
    s.addPoint(Point(0, 0));
    s.addPoint(Point(100, 0));
    std::vector<std::string> w;
    auto out = roundTrip({&s}, w);
    auto* r = static_cast<Stroke*>(out[0].get());
    EXPECT_EQ(r->getToolType(), StrokeTool::HIGHLIGHTER);
    EXPECT_FALSE(r->hasPressure());
}

TEST(XJson, textAndLinkRoundTrip) {
    Text t;
    t.setText("Hello\nworld");
    t.setFont(XojFont("Serif", 17));
    t.setColor(Color(0, 0x80, 0));
    t.setAlignment(TextAlignment::CENTER);
    t.setTransformation(xoj::util::Matrix::TRANSLATION(100, 200).rotate(0.3));
    Link l;
    l.setText("docs");
    l.setUrl("https://xournalpp.github.io");
    l.setTransformation(xoj::util::Matrix::TRANSLATION(5, 6));
    std::vector<std::string> w;
    auto out = roundTrip({&t, &l}, w);
    auto* rt = static_cast<Text*>(out[0].get());
    EXPECT_EQ(rt->getText(), "Hello\nworld");
    EXPECT_EQ(rt->getFontName(), "Serif");
    EXPECT_DOUBLE_EQ(rt->getFontSize(), 17);
    EXPECT_EQ(static_cast<TextAlignment::Value>(rt->getAlign()), TextAlignment::CENTER);
    EXPECT_EQ(rt->getTransformation(), t.getTransformation());
    auto* rl = static_cast<Link*>(out[1].get());
    EXPECT_EQ(rl->getUrl(), "https://xournalpp.github.io");
    EXPECT_EQ(rl->getTransformation(), l.getTransformation());
}

TEST(XJson, imageRoundTripAndBboxPlacement) {
    Image img;
    img.setImage(tinyPng());
    img.setTransformation(xoj::util::Matrix::TRANSLATION(10, 10).scale(2, 3));
    std::vector<std::string> w;
    auto out = roundTrip({&img}, w);
    auto* ri = static_cast<Image*>(out[0].get());
    EXPECT_EQ(ri->getBinaryData(), img.getBinaryData());
    EXPECT_EQ(ri->getTransformation(), img.getTransformation());

    json authored = {{"type", "image"}, {"data", base64Encode(tinyPng())}, {"bbox", {50, 60, 200, 100}}};
    auto placed = xjson::parse(json::array({authored}), w);
    const auto& bb = placed[0]->getBoundingBox();
    EXPECT_NEAR(bb.x, 50, 1e-9);
    EXPECT_NEAR(bb.width, 200, 1e-9);
    EXPECT_NEAR(bb.height, 100, 1e-9);
}

TEST(XJson, lenientAuthoringDefaults) {
    std::vector<std::string> w;
    json doc = json::array({{{"type", "stroke"}, {"points", {{0, 0}, {"10", 5}}}},
                            {{"type", "text"}, {"text", "Hi"}, {"x", 3}, {"y", 4}, {"color", "red"}}});
    auto out = xjson::parse(doc, w);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_DOUBLE_EQ(static_cast<Stroke*>(out[0].get())->getWidth(), 1.41);
    EXPECT_EQ(out[1]->getColor(), Color(0xff, 0, 0));
    EXPECT_DOUBLE_EQ(out[1]->getBoundingBox().x, 3);
}

TEST(XJson, errorsNameTheElement) {
    std::vector<std::string> w;
    auto expectError = [&](const json& doc, const std::string& fragment) {
        try {
            xjson::parse(doc, w);
            FAIL() << "expected an error containing " << fragment;
        } catch (const ToolError& e) {
            EXPECT_NE(std::string(e.what()).find(fragment), std::string::npos) << e.what();
        }
    };
    expectError(json::array({{{"type", "stroke"}, {"points", {{0, 0}}}}}), "elements[0]");
    expectError(json::array({{{"type", "blob"}}}), "unknown type");
    expectError(json::array({{{"type", "text"}, {"text", "x"}}}), "needs a position");
    expectError(json::array({{{"type", "stroke"}, {"points", {{0, 0, 1}, {1, 1}}}}}), "same form");
    expectError({{"format", "other"}}, "Not an xjson");
    expectError(json::array({{{"type", "latex"}, {"latex", "x^2"}}}), "create_latex");
}

#endif  // ENABLE_MCP
