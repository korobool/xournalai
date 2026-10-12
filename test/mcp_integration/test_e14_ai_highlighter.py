"""E14: the AI highlighter — highlight what you mean while you say it; a pause opens Ask with the transcript, the
places highlighted and what is under them. (Synthetic pen events; the "microphone" plays a WAV file.)"""

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


def test_1_highlight_while_speaking_opens_ask(app):
    c = app.client()
    ready(c)
    tool_before = c.call("tool_get")["tool"]
    cursor = c.call("changes_get")["cursor"]
    under = c.call("create_strokes", strokes=[{"points": [[150, 160], [250, 160]]}], animate=False)["created"][0]["id"]
    c.call("ui_interact", op="click", target=widget(c, "aiHighlighter")["id"])  # the AI toolbar's Highlight
    assert c.call("tool_get")["tool"] == "highlighter"
    # two places, one request; the fake microphone "speaks" meanwhile
    stroke(c, 200, 250, 420)
    time.sleep(SPEAK / 2)
    stroke(c, 200, 330, 420)
    time.sleep(SPEAK / 2)
    text = ask_text(c)
    assert text and "americans" in text.lower(), text
    last = c.call("app_status")["ask"]["last"]
    assert last["highlights"] == 2 and len(last["marks"]) == 2, last
    # the highlighter's marks stay (default), and the tool is given back
    assert c.call("tool_get")["tool"] == tool_before
    c.call("ui_interact", op="click", target=widget(c, "ask-send")["id"])
    intents = intents_since(c, cursor)
    assert intents and "highlighted 2 place(s)" in intents[-1]["step"], intents
    assert "americans" in intents[-1]["step"].lower()
    ids = {e["id"] for e in c.call("page_elements", page=1, detail="bbox", limit=5000)["elements"]}
    assert all(m in ids for m in last["marks"])


def test_2_marks_can_be_removed_after_sending(app):
    c = app.client()
    ready(c)
    c.call("action_run", action="win.ai-highlighter-keep", state=False)
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 420, 400)
    time.sleep(SPEAK)
    assert ask_text(c)
    marks = c.call("app_status")["ask"]["last"]["marks"]
    assert marks
    c.call("ui_interact", op="click", target=widget(c, "ask-send")["id"])
    time.sleep(0.5)
    ids = {e["id"] for e in c.call("page_elements", page=1, detail="bbox", limit=5000)["elements"]}
    assert not any(m in ids for m in marks)
    c.call("action_run", action="win.ai-highlighter-keep", state=True)


def test_3_switching_it_off_gives_the_tool_back(app):
    c = app.client()
    c.call("tool_set", tool="pen")
    c.call("action_run", action="win.ai-highlighter", state=True)
    assert c.call("tool_get")["tool"] == "highlighter"
    c.call("action_run", action="win.ai-highlighter", state=False)
    assert c.call("tool_get")["tool"] == "pen"
