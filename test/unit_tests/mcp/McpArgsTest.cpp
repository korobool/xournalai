#include "config-features.h"

#ifdef ENABLE_MCP

#include <gtest/gtest.h>

#include "mcp/Args.h"
#include "mcp/Media.h"
#include "mcp/Registry.h"

using namespace xoj::mcp;

TEST(McpArgs, typedAccessWithDefaults) {
    json j = {{"page", 3}, {"zoom", 1.5}, {"name", "notes"}, {"flag", true}, {"pts", {1, 2.5, "3"}}};
    Args a(j);
    EXPECT_EQ(a.integer("page"), 3);
    EXPECT_EQ(a.integer("missing", 7), 7);
    EXPECT_DOUBLE_EQ(a.number("zoom"), 1.5);
    EXPECT_EQ(a.str("name"), "notes");
    EXPECT_EQ(a.str("missing", "x"), "x");
    EXPECT_TRUE(a.boolean("flag", false));
    EXPECT_FALSE(a.boolean("missing", false));
    EXPECT_EQ(a.numbers("pts"), (std::vector<double>{1, 2.5, 3}));
    EXPECT_FALSE(a.numbersOpt("nope").has_value());
}

TEST(McpArgs, lenientStringsFromModels) {
    json j = {{"page", "4"}, {"scale", "0.5"}, {"on", "true"}, {"off", "no"}};
    Args a(j);
    EXPECT_EQ(a.integer("page"), 4);
    EXPECT_DOUBLE_EQ(a.number("scale"), 0.5);
    EXPECT_TRUE(a.boolean("on", false));
    EXPECT_FALSE(a.boolean("off", true));
}

TEST(McpArgs, helpfulErrors) {
    json j = {{"page", 2.5}, {"mode", "sideways"}, {"n", "abc"}, {"big", 500}, {"colour", "red"}};
    Args a(j);
    EXPECT_THROW(a.integer("page"), ToolError);
    EXPECT_THROW(a.number("n"), ToolError);
    EXPECT_THROW(a.integer("absent"), ToolError);
    EXPECT_THROW(a.integer("big", 0, 0, 100), ToolError);
    try {
        a.choice("mode", {"up", "down"}, "up");
        FAIL();
    } catch (const ToolError& e) {
        EXPECT_NE(std::string(e.what()).find("up, down"), std::string::npos);
    }
    try {
        a.rejectUnknown({"page", "mode", "n", "big", "color"});
        FAIL();
    } catch (const ToolError& e) {
        EXPECT_NE(std::string(e.what()).find("colour"), std::string::npos);
    }
    EXPECT_THROW(Args(json::array()), ToolError);
}

TEST(McpMedia, base64RoundTripAndDataUrl) {
    const std::string bytes("\x89PNG\r\n\x1a\n\0\x01", 10);
    const auto enc = base64Encode(bytes);
    EXPECT_EQ(base64Decode(enc), bytes);
    EXPECT_EQ(base64Decode("data:image/png;base64," + enc), bytes);
}

TEST(McpMedia, pngEncodingProducesSignature) {
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4);
    const auto png = encodePng(s);
    cairo_surface_destroy(s);
    ASSERT_GT(png.size(), 8u);
    EXPECT_EQ(png.substr(1, 3), "PNG");
}

#endif  // ENABLE_MCP
