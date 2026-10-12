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
