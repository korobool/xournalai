#include "config-features.h"

#ifdef ENABLE_MCP

#include <gtest/gtest.h>

#include "assistant/Companion.h"

using xoj::assistant::Companion;

TEST(Companion, managedBlockIsAddedAndReplacedKeepingUserText) {
    const std::string first = Companion::mergeManagedBlock("", "v1");
    EXPECT_NE(first.find("v1"), std::string::npos);
    EXPECT_EQ(first.find(Companion::BEGIN_MARK), 0u);

    const std::string withUser = first + "\n# My own notes\nkeep me\n";
    const std::string second = Companion::mergeManagedBlock(withUser, "v2");
    EXPECT_EQ(second.find("v1"), std::string::npos);
    EXPECT_NE(second.find("v2"), std::string::npos);
    EXPECT_NE(second.find("keep me"), std::string::npos);

    // A file without markers keeps its content below the new block
    const std::string plain = Companion::mergeManagedBlock("user text\n", "v3");
    EXPECT_NE(plain.find("user text"), std::string::npos);
    EXPECT_LT(plain.find("v3"), plain.find("user text"));
}

TEST(Companion, instructionsMentionPortMarkersAndRules) {
    xoj::assistant::CompanionSetup s;
    s.port = 4242;
    const std::string t = Companion::instructions(s);
    for (const char* needle:
         {"4242", "`*!`", "`**!`", "`*w!`", "`*c!`", "`*r!`", "Assist, don't redo", "[xournalai]", "Auto-improve"}) {
        EXPECT_NE(t.find(needle), std::string::npos) << needle;
    }
}

#endif  // ENABLE_MCP
