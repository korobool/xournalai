#include <chrono>  // for milliseconds
#include <thread>  // for sleep_for

#include <gtest/gtest.h>

#include "gui/KeyboardFocus.h"

using xoj::gui::keyHeld;
using xoj::gui::noteKeyEvent;

namespace {
GdkEvent key(GdkEventType type, guint16 code, bool modifier = false) {
    GdkEvent e{};
    e.key.type = type;
    e.key.hardware_keycode = code;
    e.key.is_modifier = modifier;
    return e;
}

GdkEvent focusOut() {
    GdkEvent e{};
    e.focus_change.type = GDK_FOCUS_CHANGE;
    e.focus_change.in = FALSE;
    return e;
}

constexpr guint16 SPACE = 65;
}  // namespace

// While the pen draws, a key held in the AI terminal (push-to-talk dictation) keeps the keyboard there
TEST(KeyboardFocus, aKeyIsHeldFromItsPressToItsRelease) {
    auto press = key(GDK_KEY_PRESS, SPACE), release = key(GDK_KEY_RELEASE, SPACE);
    noteKeyEvent(&press);
    EXPECT_TRUE(keyHeld());
    noteKeyEvent(&press);  // (a repeat)
    EXPECT_TRUE(keyHeld());
    noteKeyEvent(&release);
    EXPECT_FALSE(keyHeld());
}

// No state that can stick: a release that never arrives (it went to another window) ends the hold by itself
TEST(KeyboardFocus, aLostReleaseDoesNotLeaveAKeyHeld) {
    auto press = key(GDK_KEY_PRESS, SPACE), release = key(GDK_KEY_RELEASE, SPACE);
    noteKeyEvent(&press);
    EXPECT_TRUE(keyHeld());
    std::this_thread::sleep_for(std::chrono::milliseconds(900));  // no repeats: the key is no longer down
    EXPECT_FALSE(keyHeld());
    noteKeyEvent(&release);

    noteKeyEvent(&press);
    auto out = focusOut();
    noteKeyEvent(&out);
    EXPECT_FALSE(keyHeld());
}

TEST(KeyboardFocus, modifiersDoNotCount) {
    auto ctrl = key(GDK_KEY_PRESS, 37, true), ctrlUp = key(GDK_KEY_RELEASE, 37, true);
    noteKeyEvent(&ctrl);
    EXPECT_FALSE(keyHeld());
    noteKeyEvent(&ctrlUp);
}
