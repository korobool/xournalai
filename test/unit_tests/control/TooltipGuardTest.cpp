#include <gtest/gtest.h>

#include "gui/TooltipGuard.h"

using xoj::gui::isTooltipHover;

// A pen or finger hovering outside the canvas would only start GTK tooltips (which can freeze the app on X11)
TEST(TooltipGuard, penAndTouchHoverOutsideTheCanvasIsHeldBack) {
    for (auto src: {GDK_SOURCE_PEN, GDK_SOURCE_ERASER, GDK_SOURCE_TOUCHSCREEN}) {
        EXPECT_TRUE(isTooltipHover(GDK_MOTION_NOTIFY, src, GdkModifierType{}, false));
        EXPECT_TRUE(isTooltipHover(GDK_ENTER_NOTIFY, src, GdkModifierType{}, false));
    }
}

TEST(TooltipGuard, everythingElsePasses) {
    EXPECT_FALSE(isTooltipHover(GDK_MOTION_NOTIFY, GDK_SOURCE_PEN, GdkModifierType{}, true));        // the canvas
    EXPECT_FALSE(isTooltipHover(GDK_MOTION_NOTIFY, GDK_SOURCE_PEN, GDK_BUTTON1_MASK, false));        // a drag
    EXPECT_FALSE(isTooltipHover(GDK_MOTION_NOTIFY, GDK_SOURCE_MOUSE, GdkModifierType{}, false));     // the mouse
    EXPECT_FALSE(isTooltipHover(GDK_MOTION_NOTIFY, GDK_SOURCE_TOUCHPAD, GdkModifierType{}, false));  // touchpad
    EXPECT_FALSE(isTooltipHover(GDK_BUTTON_PRESS, GDK_SOURCE_PEN, GdkModifierType{}, false));        // a tap
}
