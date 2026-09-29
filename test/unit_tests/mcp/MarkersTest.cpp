#include "config-features.h"

#ifdef ENABLE_MCP

#include <gtest/gtest.h>

#include "assistant/Markers.h"

using namespace xoj::assistant;

namespace {
StrokeShape line(const std::string& id, double x1, double y1, double x2, double y2) {
    return {id, {Point(x1, y1), Point((x1 + x2) / 2, (y1 + y2) / 2), Point(x2, y2)}};
}
StrokeShape dot(const std::string& id, double x, double y) { return {id, {Point(x, y), Point(x + 0.8, y + 0.6)}}; }
/// an asterisk of 3 crossing lines centred at (cx, cy)
std::vector<StrokeShape> star(const std::string& p, double cx, double cy) {
    return {line(p + "a", cx - 5, cy, cx + 5, cy), line(p + "b", cx - 4, cy - 4, cx + 4, cy + 4),
            line(p + "c", cx - 4, cy + 4, cx + 4, cy - 4)};
}
std::vector<StrokeShape> bang(const std::string& p, double x, double top) {
    return {line(p + "bar", x, top, x + 0.5, top + 12), dot(p + "dot", x + 0.3, top + 16)};
}
std::vector<StrokeShape> cat(std::vector<std::vector<StrokeShape>> parts) {
    std::vector<StrokeShape> out;
    for (auto& p: parts) {
        out.insert(out.end(), p.begin(), p.end());
    }
    return out;
}
}  // namespace

TEST(Markers, starBang) {
    auto m = findMarkers(cat({star("s", 100, 100), bang("x", 112, 92)}));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].kind, "*!");
    EXPECT_EQ(m[0].ids.size(), 5u);
}

TEST(Markers, doubleStarBang) {
    auto m = findMarkers(cat({star("s", 100, 100), star("t", 114, 100), bang("x", 126, 92)}));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].kind, "**!");
    EXPECT_EQ(m[0].ids.size(), 8u);
}

TEST(Markers, starLetterBang) {
    // *w!  : a "w" zigzag between the asterisk and the "!"
    StrokeShape w{"w", {Point(108, 96), Point(110, 104), Point(112, 98), Point(114, 104), Point(116, 96)}};
    auto m = findMarkers(cat({star("s", 100, 100), {w}, bang("x", 120, 92)}));
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0].kind, "*?!");
    EXPECT_EQ(m[0].ids.size(), 6u);
}

TEST(Markers, ordinaryWritingIsNotAMarker) {
    // a star without "!", a "!" without a star, and a long line crossing a short one
    EXPECT_TRUE(findMarkers(star("s", 100, 100)).empty());
    EXPECT_TRUE(findMarkers(bang("x", 100, 100)).empty());
    EXPECT_TRUE(findMarkers(cat({{line("l", 50, 100, 250, 100), line("m", 100, 95, 100, 105)}, bang("x", 260, 92)}))
                        .empty());
    // a star far away from the "!" (another line of text)
    EXPECT_TRUE(findMarkers(cat({star("s", 100, 100), bang("x", 112, 160)})).empty());
}

TEST(Markers, twoMarkersOnAPage) {
    auto m = findMarkers(cat({star("s", 100, 100), bang("x", 112, 92), star("t", 300, 400), bang("y", 312, 392)}));
    ASSERT_EQ(m.size(), 2u);
    EXPECT_EQ(m[0].kind, "*!");
    EXPECT_EQ(m[1].kind, "*!");
}

#endif  // ENABLE_MCP
