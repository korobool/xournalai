#include "config-features.h"

#ifdef ENABLE_MCP

#include <memory>

#include <gtest/gtest.h>

#include "api/ElementIds.h"
#include "model/Stroke.h"

using xoj::api::ElementIds;

TEST(ElementIds, stableIdsAndLookup) {
    auto& ids = ElementIds::get();
    auto a = std::make_unique<Stroke>();
    auto b = std::make_unique<Stroke>();
    const auto idA = ids.idOf(a.get());
    const auto idB = ids.idOf(b.get());
    EXPECT_NE(idA, idB);
    EXPECT_EQ(idA[0], 'e');
    EXPECT_EQ(ids.idOf(a.get()), idA);  // stable
    EXPECT_EQ(ids.lookup(idA), a.get());
    EXPECT_EQ(ids.lookup(idA.substr(1)), a.get());  // "42" works as well as "e42"
    EXPECT_EQ(ids.existingId(b.get()), idB);
}

TEST(ElementIds, destructionRetiresId) {
    auto& ids = ElementIds::get();
    auto s = std::make_unique<Stroke>();
    const auto id = ids.idOf(s.get());
    const size_t before = ids.size();
    s.reset();
    EXPECT_EQ(ids.lookup(id), nullptr);
    EXPECT_EQ(ids.size(), before - 1);

    // A new element (possibly at the same address) never inherits the retired id
    auto t = std::make_unique<Stroke>();
    EXPECT_NE(ids.idOf(t.get()), id);
}

TEST(ElementIds, parse) {
    EXPECT_EQ(ElementIds::parse("e17"), 17u);
    EXPECT_EQ(ElementIds::parse("17"), 17u);
    EXPECT_FALSE(ElementIds::parse("x17").has_value());
    EXPECT_FALSE(ElementIds::parse("e").has_value());
    EXPECT_FALSE(ElementIds::parse("").has_value());
    EXPECT_EQ(ElementIds::get().lookup("e999999999"), nullptr);
}

TEST(ElementIds, attributionFollowsElementLifetime) {
    auto& ids = ElementIds::get();
    auto a = std::make_unique<Stroke>();
    auto b = std::make_unique<Stroke>();
    ids.setOrigin(a.get(), "op900");
    ids.setOrigin(b.get(), "op900");
    EXPECT_EQ(ids.origin(a.get()), "op900");
    EXPECT_EQ(ids.elementsOfOperation("op900").size(), 2u);
    b.reset();
    EXPECT_EQ(ids.elementsOfOperation("op900").size(), 1u);
    EXPECT_FALSE(ids.origin(reinterpret_cast<const Element*>(&ids)).has_value());
}

#endif  // ENABLE_MCP
