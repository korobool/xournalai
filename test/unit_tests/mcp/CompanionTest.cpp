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
    for (const char* needle: {"4242", "`*!`", "`**!`", "`*w!`", "`*c!`", "`*r!`", "Assist, don't redo", "[xournalai]",
                              "Auto-improve", "Ask [command]", "spoken English", "Spoken instructions",
                              "Audio notes to keep", "The user's request for it", "Never silent", "audio_transcribe"}) {
        EXPECT_NE(t.find(needle), std::string::npos) << needle;
    }
}

TEST(Companion, subagentsHaveFrontmatterAndTheProtocol) {
    for (const std::string& a: {Companion::quickAgent(), Companion::artistAgent()}) {
        EXPECT_EQ(a.rfind("---\nname: canvas-", 0), 0u);
        EXPECT_NE(a.find("\nmodel: "), std::string::npos);
        EXPECT_NE(a.find("\n---\n"), std::string::npos);
        EXPECT_NE(a.find("transaction_begin"), std::string::npos);
        EXPECT_NE(a.find("\"animate\":true"), std::string::npos);
    }
}

#endif  // ENABLE_MCP
