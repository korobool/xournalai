/*
 * xournalai (based on Xournal++)
 *
 * Ask: point and say. While the pen's first barrel button is held, the assistant listens (local speech to text);
 * the pen's strokes meanwhile (a lasso, with the button's usual tool) tell where. On release: silence → nothing
 * (the lasso stays the user's normal selection); speech → an ask with the transcript and the area.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <functional>  // for function
#include <string>      // for string
#include <vector>      // for vector

#include "gui/inputdevices/PenButtonObserver.h"  // for PenButtonEvent
#include "model/PageRef.h"                       // for PageRef
#include "util/Point.h"                          // for Point
#include "util/Rectangle.h"                      // for Rectangle

namespace xoj::assistant {

class SpeechToText;

struct AskCapture {
    PageRef page;
    std::vector<xoj::util::Point<double>> lasso;  ///< page coordinates; empty: the user only pointed
    xoj::util::Rectangle<double> area;            ///< the lasso's bounding box, or an area around the pen
    std::string text;                             ///< what was said
};

class AskController final {
public:
    using OnAsk = std::function<void(const AskCapture&)>;
    using OnStatus = std::function<void(const std::string& status)>;  ///< "" when idle
    /// Whether an ask is open that spoken words should go to (the popover, or the armed lasso)
    using HasTarget = std::function<bool()>;
    using OnDictation = std::function<void(const std::string& text)>;

    AskController(SpeechToText* speech, OnAsk onAsk, OnStatus onStatus, HasTarget hasTarget, OnDictation onDictation);

    void onPen(const xoj::input::PenButtonEvent& e);
    bool listening() const { return active; }

private:
    void finish(const std::string& text);

    SpeechToText* speech;
    OnAsk onAsk;
    OnStatus onStatus;
    HasTarget hasTarget;
    OnDictation onDictation;
    bool active = false;
    AskCapture capture;
    xoj::util::Point<double> hover;
};

}  // namespace xoj::assistant
