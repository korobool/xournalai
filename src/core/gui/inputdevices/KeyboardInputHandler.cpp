#include "KeyboardInputHandler.h"

#include "gui/XournalView.h"                        // for XournalView
#include "gui/XournalppCursor.h"                    // for XournalppCursor
#include "gui/inputdevices/GeometryToolInputHandler.h"  // for GeometryToolInputHandler

#include "InputContext.h"  // for InputContext
#include "InputEvents.h"   // for KeyEvent

KeyboardInputHandler::KeyboardInputHandler(InputContext* inputContext): inputContext(inputContext) {}

KeyboardInputHandler::~KeyboardInputHandler() = default;

bool KeyboardInputHandler::keyPressed(const KeyEvent& e) const {
    if (e.keyval == GDK_KEY_Control_L || e.keyval == GDK_KEY_Control_R) {
        inputContext->getView()->getCursor()->forceRefresh();  // (xournalai) Ctrl re-applies the cursor
    }
    return inputContext->getView()->onKeyPressEvent(e);
}

bool KeyboardInputHandler::keyReleased(const KeyEvent& e) const {
    return inputContext->getView()->onKeyReleaseEvent(e);
}
