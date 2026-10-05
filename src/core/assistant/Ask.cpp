#include "Ask.h"

#include <algorithm>  // for min, max
#include <cmath>      // for hypot

#include "model/XojPage.h"  // for XojPage

#include "SpeechToText.h"

namespace xoj::assistant {

AskController::AskController(SpeechToText* speech, OnAsk onAsk, OnStatus onStatus, HasTarget hasTarget,
                             OnDictation onDictation):
        speech(speech),
        onAsk(std::move(onAsk)),
        onStatus(std::move(onStatus)),
        hasTarget(std::move(hasTarget)),
        onDictation(std::move(onDictation)) {}

AskController::~AskController() {
    if (limit) {
        g_source_remove(limit);
    }
}

void AskController::onPen(const xoj::input::PenButtonEvent& e) {
    using K = xoj::input::PenButtonEvent;
    if (e.kind != K::Point) {
        SpeechToText::log(std::string("pen button ") + (e.kind == K::Down ? "down" : "up") +
                          (e.page ? "" : " (not over a page)") + (active ? "" : " [not listening]") +
                          (speech ? std::string(" speech=") + SpeechToText::name(speech->state()) : ""));
    }
    if (e.kind == K::Down) {
        if (!speech || speech->state() == SpeechToText::State::Unavailable) {
            if (speech) {
                speech->warmUp();  // try again next time
            }
            return;
        }
        speech->start();
        active = true;
        if (limit) {
            g_source_remove(limit);
        }
        limit = g_timeout_add(
                MAX_LISTEN_MS,
                +[](gpointer self) -> gboolean {
                    auto* a = static_cast<AskController*>(self);
                    a->limit = 0;
                    if (a->active) {
                        SpeechToText::log("ask: no pen button release for 2 minutes: stopping");
                        a->onPen(xoj::input::PenButtonEvent{xoj::input::PenButtonEvent::Up});
                    }
                    return G_SOURCE_REMOVE;
                },
                this);
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
    if (limit) {
        g_source_remove(limit);
        limit = 0;
    }
    onStatus("transcribing…");
    SpeechToText::log("ask: stop, lasso points " + std::to_string(capture.lasso.size()));
    speech->stop([this](const std::string& text, bool silent, const std::string& error) {
        SpeechToText::log("ask: result " +
                          (error.empty() ? (silent ? std::string("silent") : "\"" + text + "\"") : "error " + error));
        if (!error.empty()) {
            onStatus("speech: " + error);
            return;
        }
        if (silent || text.empty()) {
            onStatus("nothing heard");
            return;
        }
        onStatus("");
        // No lasso drawn while an ask is open: the words are for it (push-to-talk); a lasso makes a new ask
        if (capture.lasso.size() < 3 && hasTarget && hasTarget()) {
            onDictation(text);
        } else {
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
