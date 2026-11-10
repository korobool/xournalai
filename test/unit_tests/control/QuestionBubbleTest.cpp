#include <gtest/gtest.h>

#include "assistant/ThinkingOverlay.h"

using xoj::assistant::bubbleTapAllowed;

// A question's bubble on the canvas (1.19.0): a finger or a mouse answers; the pen never does, nor the palm that
// rests on the screen while he writes (a touch right after the pen)
TEST(QuestionBubble, thePenNeverAnswers) {
    EXPECT_FALSE(bubbleTapAllowed(GDK_SOURCE_PEN, -1));
    EXPECT_FALSE(bubbleTapAllowed(GDK_SOURCE_ERASER, 10 * G_USEC_PER_SEC));
}

TEST(QuestionBubble, aTouchRightAfterThePenIsThePalm) {
    EXPECT_FALSE(bubbleTapAllowed(GDK_SOURCE_TOUCHSCREEN, 0));
    EXPECT_FALSE(bubbleTapAllowed(GDK_SOURCE_TOUCHSCREEN, 300 * 1000));
    EXPECT_TRUE(bubbleTapAllowed(GDK_SOURCE_TOUCHSCREEN, 2 * G_USEC_PER_SEC));
    EXPECT_TRUE(bubbleTapAllowed(GDK_SOURCE_TOUCHSCREEN, -1));  // (no pen yet)
}

TEST(QuestionBubble, aMouseAnswers) {
    EXPECT_TRUE(bubbleTapAllowed(GDK_SOURCE_MOUSE, 0));
    EXPECT_TRUE(bubbleTapAllowed(GDK_SOURCE_TOUCHPAD, -1));
}
