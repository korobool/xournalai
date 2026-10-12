"""E14: the AI highlighter — highlight what you mean while you say it; a pause opens Ask with the transcript, the
places highlighted (their exact shape) and what is under them. The marks are not ink: they go when the AI is done. (Synthetic pen events; the "microphone" plays a WAV file.)"""

import pathlib
import time

from test_e10_ask import APP_ENV, APP_CONFIG, SPEAK, ready, widget, intents_since  # noqa: F401


def stroke(c, x1, y, x2, n=12):
    c.call("test_pen", op="hover", x=x1, y=y)
    c.call("test_pen", op="tip_down", x=x1, y=y)
    for i in range(1, n + 1):
        c.call("test_pen", op="move", x=x1 + (x2 - x1) * i / n, y=y)
    c.call("test_pen", op="tip_up", x=x2, y=y)


def ask_text(c, timeout=20):
    deadline = time.time() + timeout
    while time.time() < deadline:
        w = widget(c, "ask-text")
        if w and w.get("visible", True) and (w.get("value") or ""):
            return w["value"]
        time.sleep(0.2)
    w = widget(c, "ask-text")
    return w.get("value") if w else None


def last_text(c):
    return c.call("app_status")["ask"].get("last", {}).get("text")


def done(c):
    """Taps Done and waits for the transcript (a new request: its text, not the previous one's)"""
    before = c.call("app_status")["ask"].get("last")
    c.call("ui_interact", op="click", target=widget(c, "aiHighlightDone")["id"])
    deadline = time.time() + 20
    while time.time() < deadline:
        last = c.call("app_status")["ask"].get("last")
        if last != before and last.get("text") and c.call("app_status")["ask"]["state"] == "idle":
            return last["text"]
        time.sleep(0.2)


def marks(c):
    return c.call("app_status")["ask"].get("highlight_marks", 0)


def elements(c):
    return {e["id"]: e for e in c.call("page_elements", page=1, detail="bbox", limit=5000)["elements"]}


def test_1_highlight_while_speaking_opens_ask(app):
    c = app.client()
    ready(c)
    c.call("tool_set", tool="pen")
    cursor = c.call("changes_get")["cursor"]
    before = set(elements(c))
    c.call("ui_interact", op="click", target=widget(c, "aiHighlighter")["id"])  # the AI toolbar's Highlight
    assert c.call("tool_get")["tool"] == "pen"  # (no tool of its own: the marks are not ink)
    # two places, one request; the fake microphone "speaks" meanwhile
    stroke(c, 200, 250, 420)
    time.sleep(SPEAK / 2)
    stroke(c, 200, 330, 420)
    time.sleep(SPEAK / 2)
    assert not (widget(c, "ask-text") or {}).get("value")  # (no pause ends it: the owner does)
    done(c)
    text = ask_text(c)
    assert text and "americans" in text.lower(), text
    last = c.call("app_status")["ask"]["last"]
    assert last["highlights"] == 2, last
    assert set(elements(c)) == before  # nothing was drawn into the document
    assert marks(c) == 1
    c.call("ui_interact", op="click", target=widget(c, "ask-send")["id"])
    intents = intents_since(c, cursor)
    step = intents[-1]["step"]
    assert "highlighted 2 place(s)" in step and "pt wide" in step, step
    assert "americans" in step.lower()
    sent = c.call("app_status")["ask"]["last"]
    assert pathlib.Path(sent["detail_image"]).is_file()
    # the marks stay while the AI works on it, and go when it is done
    assert marks(c) == 1
    zone = sent["submitted"]["zone"]
    c.call("thinking", op="update", id=zone, text="looking…")
    assert marks(c) == 1
    c.call("thinking", op="done", id=zone)
    deadline = time.time() + 5
    while time.time() < deadline and marks(c):
        time.sleep(0.2)
    assert marks(c) == 0


def test_2_the_footprint_decides_what_is_meant(app):
    """What the marks touch counts (a stroke partly covered is flagged); what is only near them doesn't."""
    c = app.client()
    ready(c)
    cursor = c.call("changes_get")["cursor"]
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 420, 400)
    time.sleep(SPEAK)
    done(c)
    assert ask_text(c)
    area = c.call("app_status")["ask"]["last"]["area"]
    x, y, w, h = area
    cy = y + h / 2
    touched = c.call("create_strokes", strokes=[{"points": [[x + w / 2, cy - 3], [x + w / 2, cy + 3]]}],
                     animate=False)["created"][0]["id"]
    partly = c.call("create_strokes", strokes=[{"points": [[x + w / 3, cy], [x + w / 3, cy + 120]]}],
                    animate=False)["created"][0]["id"]
    near = c.call("create_strokes", strokes=[{"points": [[x + w / 2, y + h + 25], [x + w / 2 + 10, y + h + 25]]}],
                  animate=False)["created"][0]["id"]
    c.call("ui_interact", op="click", target=widget(c, "ask-send")["id"])
    sent = c.call("app_status")["ask"]["last"]["submitted"]
    assert touched in sent["ids"] and partly in sent["ids"] and near not in sent["ids"], sent
    step = intents_since(c, cursor)[-1]["step"]
    assert "only partly covered" in step and partly in step.split("only partly covered")[1], step
    c.call("thinking", op="done", id=sent["zone"])


def test_3_cancel_takes_the_marks_away(app):
    c = app.client()
    ready(c)
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 470, 380)
    time.sleep(SPEAK)
    done(c)
    assert ask_text(c)
    assert marks(c) >= 1
    n = marks(c)
    c.call("ui_interact", op="click", target=widget(c, "ask-close")["id"])
    deadline = time.time() + 5
    while time.time() < deadline and marks(c) == n:
        time.sleep(0.2)
    assert marks(c) == n - 1


def test_4_a_tool_ends_it(app):
    c = app.client()
    c.call("tool_set", tool="pen")
    c.call("action_run", action="win.ai-highlighter", state=True)
    assert widget(c, "aiHighlightBar").get("visible", True)
    c.call("tool_set", tool="eraser")
    assert not widget(c, "aiHighlightBar").get("visible", True)
    states = {a["name"]: a.get("state") for a in c.call("actions_list", filter="ai-highlighter")["actions"]}
    assert states.get("win.ai-highlighter") is False, states


def test_5_the_strip_becomes_the_mode_bar(app):
    """While on, the strip shows only the level, time, marks, Discard and Done — at the same height."""
    c = app.client()
    canvas_y = lambda: [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]
                        if w.get("name") == "aiToolbarsWithTools"][0]["bbox"][3]
    h = canvas_y()
    c.call("action_run", action="win.ai-highlighter", state=True)
    time.sleep(0.5)
    assert widget(c, "aiHighlightBar").get("visible", True)
    for gone in ("aiQuickToolsButton", "aiRecordButton", "aiToolbarColumn"):
        assert not widget(c, gone).get("visible", True), gone
    assert abs(canvas_y() - h) <= 2  # the page doesn't move
    labels = [w.get("label") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert any("highlight what you mean" in l for l in labels)
    app.user_key("Escape")  # (= Discard; agents may not click Discard)
    time.sleep(0.3)
    assert not widget(c, "aiHighlightBar").get("visible", True)
    assert widget(c, "aiQuickToolsButton").get("visible", True)


def test_6_quiet_for_ten_seconds_is_done(app):
    """No marks and no words for 10 s: Done by itself (counted down on the button), with what was said."""
    c = app.client()
    ready(c)
    before = c.call("app_status")["ask"].get("last")
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 300, 380)
    labels = lambda: [w.get("label") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    deadline = time.time() + 25
    counted = False
    while time.time() < deadline and c.call("app_status")["ask"].get("last") == before:
        counted = counted or any(l.startswith("Done in") for l in labels())
        time.sleep(0.3)
    assert counted
    deadline = time.time() + 15
    while time.time() < deadline and not last_text(c):
        time.sleep(0.2)
    assert "americans" in (last_text(c) or "").lower()
    c.call("ui_interact", op="click", target=widget(c, "ask-close")["id"])


def test_7_escape_discards_and_tapping_again_is_done(app):
    c = app.client()
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 300, 380)
    app.user_key("Escape")
    time.sleep(0.5)
    assert not widget(c, "aiHighlightBar").get("visible", True) and marks(c) == 0
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 300, 380)
    c.call("action_run", action="win.ai-highlighter", state=False)  # = Done
    deadline = time.time() + 10
    while time.time() < deadline and not (widget(c, "ask-text") or {}).get("visible", False):
        time.sleep(0.2)
    assert widget(c, "ask-text").get("visible", True) and marks(c) == 1
    c.call("ui_interact", op="click", target=widget(c, "ask-close")["id"])



def finger(c, x1, y, x2, n=10):
    c.call("test_touch", op="begin", finger=0, x=x1, y=y)
    for i in range(1, n + 1):
        c.call("test_touch", op="update", finger=0, x=x1 + (x2 - x1) * i / n, y=y)
    c.call("test_touch", op="end", finger=0, x=x2, y=y)


def test_8_a_finger_marks_when_touch_drawing_is_on(app):
    """With touch drawing on, a finger marks for the AI (not ink); with it off (the default), a finger draws nothing."""
    c = app.client()
    c.call("tool_set", tool="pen")
    before = set(elements(c))
    c.call("action_run", action="win.ai-highlighter", state=True)
    finger(c, 220, 320, 380)  # touch drawing off: no mark, no ink
    time.sleep(0.3)
    assert set(elements(c)) == before
    labels = lambda: [w.get("label") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert not any("1 mark" in l for l in labels())
    c.call("action_run", action="win.toggle-touch-drawing", state=True)
    finger(c, 220, 320, 380)
    time.sleep(0.5)
    assert any("1 mark" in l for l in labels()), labels()
    assert set(elements(c)) == before  # a mark, not ink
    app.user_key("Escape")
    c.call("action_run", action="win.toggle-touch-drawing", state=False)
