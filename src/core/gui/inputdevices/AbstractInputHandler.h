/*
 * Xournal++
 *
 * [Header description]
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "PositionInputData.h"  // for PositionInputData

class InputContext;
class XojPageView;
struct InputEvent;

/**
 * Abstract class for a specific input state
 */
class AbstractInputHandler {
private:
    bool blocked = false;
    bool interceptDown = false;  ///< a stroke is going to the stroke interceptor (the Ask lasso)
    /// The Ask lasso: gives pen / mouse strokes to the stroke interceptor while one is set; true if consumed
    bool intercept(InputEvent const& event);

protected:
    InputContext* inputContext;
    bool inputRunning = false;

protected:
    XojPageView* getPageAtCurrentPosition(InputEvent const& event) const;
    PositionInputData getInputDataRelativeToCurrentPage(XojPageView* page, InputEvent const& event) const;

public:
    explicit AbstractInputHandler(InputContext* inputContext);
    virtual ~AbstractInputHandler();

    void block(bool block);
    bool isBlocked() const;
    virtual void onBlock();
    virtual void onUnblock();
    bool handle(InputEvent const& event);
    virtual bool handleImpl(InputEvent const& event) = 0;
};
