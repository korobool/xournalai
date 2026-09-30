//
// Created by ulrich on 06.04.19.
//

#include "AbstractInputHandler.h"

#include <string_view>

#include <glib.h>  // for gdouble

#include "control/settings/Settings.h"           // for Settings
#include "gui/Layout.h"                          // for Layout
#include "gui/PageView.h"                        // for XojPageView
#include "gui/XournalView.h"                     // for XournalView
#include "gui/XournalppCursor.h"                 // for XournalppCursor
#include "gui/inputdevices/InputEvents.h"        // for InputEvent
#include "gui/inputdevices/PositionInputData.h"  // for PositionInputData
#include "gui/inputdevices/StrokeInterceptor.h"  // for strokeInterceptor
#include "gui/widgets/XournalWidget.h"           // for GtkXournal
#include "model/Point.h"                         // for Point, Point::NO_PRE...
#include "util/Assert.h"                         // for xoj_assert
#include "util/safe_casts.h"                     // for round_cast

#include "InputContext.h"  // for InputContext

AbstractInputHandler::AbstractInputHandler(InputContext* inputContext) { this->inputContext = inputContext; }

AbstractInputHandler::~AbstractInputHandler() = default;

void AbstractInputHandler::block(bool block) {
    if (block == this->blocked) {
        return;
    }
    this->blocked = block;
    if (!this->blocked) {
        this->onUnblock();
    } else {
        this->onBlock();
    }
}

auto AbstractInputHandler::isBlocked() const -> bool { return this->blocked; }

auto AbstractInputHandler::handle(InputEvent const& event) -> bool {
    if (intercept(event)) {
        return true;
    }
    if (!this->blocked) {
        if (auto* v = this->inputContext->getView(); v) {
            v->getCursor()->setInputDeviceClass(event.deviceClass);
        }
        return this->handleImpl(event);
    }
    return true;
}

/**
 * Get Page at current position
 *
 * @return page or nullptr if none
 */
auto AbstractInputHandler::getPageAtCurrentPosition(InputEvent const& event) const -> XojPageView* {
    if (!event) {
        return nullptr;
    }

    GtkXournal* xournal = this->inputContext->getXournal();

    int x = round_cast<int>(event.relative.x);
    int y = round_cast<int>(event.relative.y);

    return xournal->layout->getPageViewAt(x, y);
}

/**
 * Get input data relative to current input page
 */
auto AbstractInputHandler::getInputDataRelativeToCurrentPage(XojPageView* page, InputEvent const& event) const
        -> PositionInputData {
    xoj_assert(page != nullptr);

    PositionInputData pos = {};
    auto pagePos = page->getPixelPosition();
    pos.x = event.relative.x - pagePos.x;
    pos.y = event.relative.y - pagePos.y;
    pos.pressure = Point::NO_PRESSURE;

    if (this->inputContext->getSettings()->isPressureSensitivity()) {
        pos.pressure = event.pressure;
    }

    pos.state = event.state;
    pos.timestamp = event.timestamp;

    pos.deviceId = event.deviceId;

    return pos;
}

void AbstractInputHandler::onBlock() {}

void AbstractInputHandler::onUnblock() {}

auto AbstractInputHandler::intercept(InputEvent const& event) -> bool {
    const auto interceptor = xoj::input::strokeInterceptor();  // a copy: it may remove itself while it runs
    const bool pointer = event.deviceClass == INPUT_DEVICE_PEN || event.deviceClass == INPUT_DEVICE_MOUSE;
    if (!interceptor || !pointer) {
        interceptDown = false;
        return false;
    }
    using K = xoj::input::InterceptedStroke;
    K::Kind kind;
    if (event.type == BUTTON_PRESS_EVENT && event.button == 1) {
        kind = K::Down;
        interceptDown = true;
    } else if (event.type == MOTION_EVENT && interceptDown) {
        kind = K::Move;
    } else if (event.type == BUTTON_RELEASE_EVENT && event.button == 1 && interceptDown) {
        kind = K::Up;
        interceptDown = false;
    } else {
        return event.type == BUTTON_PRESS_EVENT || event.type == BUTTON_RELEASE_EVENT ? false : interceptDown;
    }
    K s{kind};
    if (XojPageView* view = getPageAtCurrentPosition(event)) {
        const PositionInputData pos = getInputDataRelativeToCurrentPage(view, event);
        const double zoom = this->inputContext->getView()->getZoom();
        s.page = view->getPage();
        s.x = pos.x / zoom;
        s.y = pos.y / zoom;
    }
    return interceptor(s);
}
