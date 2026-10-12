#include <filesystem>

#include <glib.h>
#include <gtest/gtest.h>

#include "assistant/TerminalCommands.h"

using namespace xoj::assistant;
namespace tc = xoj::assistant::terminal_commands;

TEST(TerminalCommandsTest, titleFromTheProgram) {
    EXPECT_EQ(tc::tabTitle({"", "opencode --config ~/work.json", ""}), "opencode");
    EXPECT_EQ(tc::tabTitle({"", "OPENCODE_CONFIG=x  /usr/bin/opencode -c", ""}), "opencode");
    EXPECT_EQ(tc::tabTitle({"Work", "opencode", ""}), "Work");
    EXPECT_EQ(tc::tabTitle({"", "", ""}), "Shell");
}

TEST(TerminalCommandsTest, rememberMovesToFrontAndCaps) {
    std::vector<SavedCommand> l;
    l = tc::remember(l, {"", "a", ""});
    l = tc::remember(l, {"", "b", ""});
    l = tc::remember(l, {"A again", "a", ""});
    ASSERT_EQ(l.size(), 2u);
    EXPECT_EQ(l[0].title, "A again");
    EXPECT_EQ(l[1].command, "b");
    l = tc::remember(l, {"", "a", "/tmp"});  // another folder: another entry
    EXPECT_EQ(l.size(), 3u);
    for (int i = 0; i < 20; i++) {
        l = tc::remember(l, {"", "c" + std::to_string(i), ""});
    }
    EXPECT_EQ(l.size(), tc::MAX_SAVED);
    l = tc::forget(l, {"", "c19", ""});
    EXPECT_EQ(l.front().command, "c18");
}

TEST(TerminalCommandsTest, saveAndLoad) {
    const auto dir = std::filesystem::temp_directory_path() / ("xoai-tc-" + std::to_string(g_random_int()));
    const auto file = dir / "ai-terminals.json";
    EXPECT_TRUE(tc::load(file).empty());
    ASSERT_TRUE(tc::save(file, {{"Work", "opencode --config w.json", "~/proj"}, {"", "htop", ""}}));
    const auto l = tc::load(file);
    ASSERT_EQ(l.size(), 2u);
    EXPECT_EQ(l[0].title, "Work");
    EXPECT_EQ(l[0].folder, "~/proj");
    EXPECT_EQ(l[1].command, "htop");
    std::filesystem::remove_all(dir);
    EXPECT_EQ(tc::expandFolder("~/x"), std::string(g_get_home_dir()) + "/x");
    EXPECT_EQ(tc::expandFolder(""), "");
}

#include "assistant/QuickTools.h"

namespace qt = xoj::assistant::quick_tools;

TEST(QuickToolsTest, rankedByUseThenCatalogOrder) {
    const auto first = qt::ranked({});
    ASSERT_EQ(first.size(), qt::SHOWN);
    EXPECT_EQ(first[0], "pen");
    EXPECT_EQ(first[1], "eraser");
    const auto r = qt::ranked({{"redo", 5}, {"text", 5}, {"eraser", 2}, {"nonsense", 9}});
    EXPECT_EQ(r[0], "redo");  // (ties keep the catalog order: redo before text)
    EXPECT_EQ(r[1], "text");
    EXPECT_EQ(r[2], "eraser");
    EXPECT_EQ(r[3], "pen");
    EXPECT_EQ(r.size(), qt::SHOWN);
}

TEST(QuickToolsTest, countsSaveAndLoad) {
    const auto file = std::filesystem::temp_directory_path() / ("xoai-qt-" + std::to_string(g_random_int()) + ".json");
    ASSERT_TRUE(qt::save(file, {{"pen", 3}, {"ask", 1}}));
    const auto m = qt::load(file);
    EXPECT_EQ(m.at("pen"), 3);
    EXPECT_EQ(m.at("ask"), 1);
    std::filesystem::remove(file);
    EXPECT_TRUE(qt::load(file).empty());
}
