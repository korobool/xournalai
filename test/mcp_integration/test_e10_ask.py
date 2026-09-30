"""E10: Ask — hold the pen's barrel button, speak, circle an area: the transcript and the area become a request.
Silence changes nothing. (Synthetic pen events through GTK; the "microphone" plays a WAV file.)"""

import math
import pathlib
import shutil
import tempfile
import time

from test_e10_stt import MODEL, SAMPLE, write_wav

MIC = pathlib.Path(tempfile.mkdtemp(prefix="xoai-mic-")) / "mic.wav"
shutil.copy(SAMPLE, MIC)
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_FAKE_MIC": str(MIC),
           "XOURNALAI_STT_MODELS": str(MODEL.parent)}
APP_CONFIG = {"assistant": {"speech": True, "speech_model": MODEL.name[len("ggml-"):-len(".bin")]}}


def ready(c, timeout=20):
    deadline = time.time() + timeout
    while time.time() < deadline and c.call("app_status")["speech"]["state"] != "ready":
        time.sleep(0.2)
    assert c.call("app_status")["speech"]["state"] == "ready"


def last_ask(c, previous=None, timeout=15):
    deadline = time.time() + timeout
    while time.time() < deadline:
        a = c.call("app_status")["ask"]
        if a.get("last") != previous and a["state"] == "idle":
            return a.get("last")
        time.sleep(0.2)
    return c.call("app_status")["ask"].get("last")


def lasso(c, cx, cy, r=60, n=24):
    c.call("test_pen", op="hover", x=cx + r, y=cy)
    c.call("test_pen", op="tip_down", x=cx + r, y=cy)
    for i in range(1, n + 1):
        a = 2 * math.pi * i / n
        c.call("test_pen", op="move", x=cx + r * math.cos(a), y=cy + r * math.sin(a))
    c.call("test_pen", op="tip_up", x=cx + r, y=cy)


def test_1_speak_and_circle(app):
    c = app.client()
    ready(c)
    c.call("test_pen", op="hover", x=400, y=300)
    c.call("test_pen", op="barrel_down", x=400, y=300)
    assert c.call("app_status")["ask"]["state"] == "listening"
    lasso(c, 400, 300)
    c.call("test_pen", op="barrel_up", x=400, y=300)
    ask = last_ask(c)
    assert ask and "what your country can do for you" in ask["text"].lower(), ask
    assert ask["page"] == 1 and ask["lasso_points"] >= 20
    x, y, w, h = ask["area"]
    assert 20 < w < 200 and 20 < h < 200, ask  # the circle, in page points


def test_2_silence_changes_nothing(app):
    c = app.client()
    ready(c)
    before = c.call("app_status")["ask"].get("last")
    write_wav(MIC, [0.0] * 32000)
    try:
        c.call("test_pen", op="barrel_down", x=300, y=300)
        lasso(c, 300, 300)
        c.call("test_pen", op="barrel_up", x=300, y=300)
        time.sleep(2)
        a = c.call("app_status")["ask"]
        assert a.get("last") == before and a["state"] == "idle", a
    finally:
        shutil.copy(SAMPLE, MIC)


def test_3_pointing_without_a_lasso(app):
    c = app.client()
    ready(c)
    before = c.call("app_status")["ask"].get("last")
    c.call("test_pen", op="hover", x=500, y=350)
    c.call("test_pen", op="barrel_down", x=500, y=350)
    c.call("test_pen", op="hover", x=505, y=352)
    c.call("test_pen", op="barrel_up", x=505, y=352)
    ask = last_ask(c, before)
    assert ask and ask["lasso_points"] == 0 and ask["area"][2] == 200, ask


def widget(c, name):
    for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]:
        if w.get("name") == name:
            return w
    return None


def intents_since(c, cursor):
    return [e for e in c.call("changes_get", since=cursor)["events"] if e["type"] == "intent"]


def test_4_the_popover_shows_what_was_said_and_sends_a_command(app):
    c = app.client()
    ready(c)
    cursor = c.call("changes_get")["cursor"]
    before = c.call("app_status")["ask"].get("last")
    c.call("test_pen", op="barrel_down", x=420, y=320)
    lasso(c, 420, 320)
    c.call("test_pen", op="barrel_up", x=420, y=320)
    ask = last_ask(c, before)
    x, y, w, h = ask["area"]
    inner = c.call("create_strokes", strokes=[{"points": [[x + w / 2 - 5, y + h / 2], [x + w / 2 + 5, y + h / 2]]}],
                   animate=False)["created"][0]["id"]
    outer = c.call("create_strokes", strokes=[{"points": [[x + w + 40, y], [x + w + 60, y]]}],
                   animate=False)["created"][0]["id"]
    text = widget(c, "ask-text")
    assert text and text.get("visible", True), text
    assert "country" in (text.get("text") or text.get("value") or ""), text
    c.call("ui_interact", op="click", target=widget(c, "ask-improve")["id"])
    sent = c.call("app_status")["ask"]["last"]["submitted"]
    assert sent["command"] == "improve" and sent["zone"] > 0 and inner in sent["ids"] and outer not in sent["ids"]
    intents = intents_since(c, cursor)
    assert intents and 'Ask [improve]: "' in intents[-1]["step"] and "(circled)" in intents[-1]["step"]
    zones = {z["id"]: z for z in c.call("thinking_list")["zones"]}
    assert sent["zone"] in zones


def test_5_command_button_opens_ask_for_typing(app):
    c = app.client()
    cursor = c.call("changes_get")["cursor"]
    c.call("ui_interact", op="click", target=widget(c, "aiAct-command")["id"])
    time.sleep(0.3)
    text = widget(c, "ask-text")
    c.call("ui_interact", op="set_value", target=text["id"], value="make it red")
    c.call("ui_interact", op="activate", target=text["id"])
    intents = intents_since(c, cursor)
    assert intents and 'Ask: "make it red"' in intents[-1]["step"], intents
