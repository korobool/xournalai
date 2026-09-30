#include "Ask.h"

#include <algorithm>  // for min, max
#include <cmath>      // for hypot

#include "model/XojPage.h"  // for XojPage

#include "SpeechToText.h"

namespace xoj::assistant {

AskController::AskController(SpeechToText* speech, OnAsk onAsk, OnStatus onStatus):
        speech(speech), onAsk(std::move(onAsk)), onStatus(std::move(onStatus)) {}

void AskController::onPen(const xoj::input::PenButtonEvent& e) {
    using K = xoj::input::PenButtonEvent;
    if (e.kind == K::Down) {
        if (!speech || speech->state() == SpeechToText::State::Unavailable) {
            if (speech) {
                speech->warmUp();  // try again next time
            }
            return;
        }
        speech->start();
        active = true;
        capture = AskCapture{};
        capture.page = e.page;
        hover = {e.x, e.y};
        onStatus("listening…");
        return;
    }
    if (!active) {
        return;
    }
    if (e.kind == K::Point) {
        if (e.tipDown && e.page && (e.page == capture.page || capture.lasso.empty())) {
            capture.page = e.page;
            const xoj::util::Point<double> p{e.x, e.y};
            if (capture.lasso.empty() || std::hypot(p.x - capture.lasso.back().x, p.y - capture.lasso.back().y) >= 1) {
                capture.lasso.push_back(p);
            }
        } else if (!e.tipDown && e.page && capture.lasso.empty()) {
            capture.page = e.page;
            hover = {e.x, e.y};
        }
        return;
    }
    // Up: what was said?
    active = false;
    onStatus("transcribing…");
    speech->stop([this](const std::string& text, bool silent, const std::string& error) {
        if (!error.empty()) {
            onStatus("speech: " + error);
            return;
        }
        onStatus("");
        if (!silent && !text.empty()) {
            finish(text);
        }
    });
}

void AskController::finish(const std::string& text) {
    AskCapture c = capture;
    c.text = text;
    if (!c.page) {
        return;  // not over a page
    }
    const double w = c.page->getWidth(), h = c.page->getHeight();
    if (c.lasso.size() >= 3) {
        double x1 = c.lasso[0].x, y1 = c.lasso[0].y, x2 = x1, y2 = y1;
        for (const auto& p: c.lasso) {
            x1 = std::min(x1, p.x), y1 = std::min(y1, p.y), x2 = std::max(x2, p.x), y2 = std::max(y2, p.y);
        }
        c.area = {x1, y1, x2 - x1, y2 - y1};
    } else {
        // Only pointed: an area around the pen
        const double aw = std::min(200.0, w), ah = std::min(120.0, h);
        c.area = {std::clamp(hover.x - aw / 2, 0.0, w - aw), std::clamp(hover.y - ah / 2, 0.0, h - ah), aw, ah};
        c.lasso.clear();
    }
    onAsk(c);
}

}  // namespace xoj::assistant
