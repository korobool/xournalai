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
SPEAK = 3.0
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
    time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
    c.call("test_pen", op="barrel_up", x=400, y=300)
    ask = last_ask(c)
    assert ask and "americans" in ask["text"].lower(), ask
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
        time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
        c.call("test_pen", op="barrel_up", x=300, y=300)
        time.sleep(2)
        a = c.call("app_status")["ask"]
        assert a.get("last") == before and a["state"] in ("idle", "nothing heard"), a
    finally:
        shutil.copy(SAMPLE, MIC)


def close_popover(c):
    w = widget(c, "ask-close")
    if w:
        c.call("ui_interact", op="click", target=w["id"])


def test_3_pointing_without_a_lasso(app):
    c = app.client()
    ready(c)
    close_popover(c)
    before = c.call("app_status")["ask"].get("last")
    c.call("test_pen", op="hover", x=500, y=350)
    c.call("test_pen", op="barrel_down", x=500, y=350)
    c.call("test_pen", op="hover", x=505, y=352)
    time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
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
    time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
    c.call("test_pen", op="barrel_up", x=420, y=320)
    ask = last_ask(c, before)
    x, y, w, h = ask["area"]
    inner = c.call("create_strokes", strokes=[{"points": [[x + w / 2 - 5, y + h / 2], [x + w / 2 + 5, y + h / 2]]}],
                   animate=False)["created"][0]["id"]
    outer = c.call("create_strokes", strokes=[{"points": [[x + w + 40, y], [x + w + 60, y]]}],
                   animate=False)["created"][0]["id"]
    text = widget(c, "ask-text")
    assert text and text.get("visible", True), text
    assert "americans" in (text.get("value") or "").lower(), text
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


def element_count(c):
    return len(c.call("page_elements", page=1, detail="bbox", limit=5000)["elements"])


def test_6_the_ask_button_arms_a_lasso_and_the_pen_button_dictates(app):
    c = app.client()
    ready(c)
    cursor = c.call("changes_get")["cursor"]
    n = element_count(c)
    c.call("ui_interact", op="click", target=widget(c, "aiAsk")["id"])  # the toolbar's Ask
    assert c.call("app_status")["ask"]["state"].startswith("circle the area")
    lasso(c, 350, 280, r=50)  # with the tip only: the lasso, not ink
    time.sleep(0.3)
    assert element_count(c) == n
    text = widget(c, "ask-text")
    assert text and text.get("visible", True) and not text.get("value"), text
    c.call("test_pen", op="barrel_down", x=350, y=280)  # push-to-talk into the open popover
    time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
    c.call("test_pen", op="barrel_up", x=350, y=280)
    deadline = time.time() + 15
    while time.time() < deadline and "americans" not in (widget(c, "ask-text").get("value") or "").lower():
        time.sleep(0.2)
    assert "americans" in widget(c, "ask-text").get("value", "").lower(), widget(c, "ask-text")
    c.call("ui_interact", op="click", target=widget(c, "ask-send")["id"])
    intents = intents_since(c, cursor)
    assert intents and 'Ask: "' in intents[-1]["step"] and "americans" in intents[-1]["step"].lower(), intents


def test_7_the_mouse_can_draw_the_lasso_too(app):
    c = app.client()
    n = element_count(c)
    c.call("action_run", action="win.ai-ask", state=True)
    cx, cy = app.canvas_center(c)
    app.user_drag([(cx + 40 * math.cos(a / 10), cy + 40 * math.sin(a / 10)) for a in range(0, 63, 3)])
    time.sleep(0.5)
    assert element_count(c) == n  # no ink
    assert widget(c, "ask-text").get("visible", True)
    assert c.call("app_status")["ask"]["state"] == "idle"  # disarmed after one lasso


def test_8_a_recording_indicator_shows_listening_then_transcribing(app):
    c = app.client()
    ready(c)
    close_popover(c)
    c.call("test_pen", op="hover", x=300, y=330)
    c.call("test_pen", op="barrel_down", x=300, y=330)
    assert c.call("app_status")["ask"]["recording"] == "listening"  # at once, before any word
    time.sleep(1.0)
    assert c.call("app_status")["ask"]["recording_levels"] > 3  # the level bars move with the voice
    time.sleep(SPEAK)  # "speaking": the fake microphone plays in real time
    c.call("test_pen", op="barrel_up", x=300, y=330)
    assert c.call("app_status")["ask"]["recording"] == "transcribing"
    deadline = time.time() + 15
    while time.time() < deadline and c.call("app_status")["ask"]["recording"] != "off":
        time.sleep(0.1)
    assert c.call("app_status")["ask"]["recording"] == "off"


def hook(app, event, payload=None):
    import json as _json
    import subprocess
    env = dict(app.app_env(), XOURNALAI_PORT=str(app.port))
    r = subprocess.run([app.binary, "--ai-hook", event], input=_json.dumps(payload or {"session_id": "s"}), env=env,
                       text=True, capture_output=True, timeout=10)
    assert r.returncode == 0


def zone_state(c, zone):
    return {z["id"]: z["state"] for z in c.call("thinking_list")["zones"]}.get(zone)


def test_9_a_delegated_ask_stays_visible_until_its_work_ends(app):
    # The serving session hands an ask to a background subagent and ends its turn at once: the zone must stay
    # (it used to be closed right then, so the user saw nothing while the subagent worked)
    c = app.client()
    close_popover(c)
    c.call("action_run", action="win.ai-ask", state=True)
    lasso(c, 330, 300, r=40)
    c.call("ui_interact", op="set_value", target=widget(c, "ask-text")["id"], value="explain logistic regression")
    c.call("ui_interact", op="activate", target=widget(c, "ask-text")["id"])
    zone = c.call("app_status")["ask"]["last"]["submitted"]["zone"]
    hook(app, "SessionStart")
    hook(app, "UserPromptSubmit")
    c.call("thinking", op="update", id=zone, text="explaining…")  # (delivered: thinking)
    hook(app, "PreToolUse", {"session_id": "s", "tool_name": "Agent"})
    hook(app, "PostToolUse", {"session_id": "s", "tool_name": "Agent"})
    hook(app, "SubagentStop")  # (a background launch may be reported like this at once)
    hook(app, "Stop")  # the coordinator ends its turn
    time.sleep(0.5)
    assert zone_state(c, zone) == "thinking", c.call("thinking_list")
    t = c.call("transaction_begin", label="logistic regression", zone=zone)
    c.call("transaction_commit", transaction=t["transaction"], ops=[])
    assert zone_state(c, zone) in ("done", None)


def test_10_the_layer_selector_never_shows_an_ai_draft(app):
    c = app.client()
    t = c.call("transaction_begin", label="x", page=1)
    time.sleep(0.3)
    shown = [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]
             if "AI draft" in (w.get("label") or w.get("value") or w.get("text") or "") and w.get("visible", True)]
    c.call("transaction_abort", transaction=t["transaction"], reason="test")
    assert not shown, shown


def test_11_listening_shows_in_the_status_line(app):
    # the status line's recording pill animates for Ask too: "Ask: listening", no Stop (releasing the button ends it)
    c = app.client()
    ready(c)
    c.call("test_pen", op="hover", x=400, y=300)
    c.call("test_pen", op="barrel_down", x=400, y=300)
    time.sleep(0.5)
    ind, lab = widget(c, "recordingIndicator"), widget(c, "recordingIndicatorLabel")
    stop = widget(c, "recordingIndicatorStop")
    c.call("test_pen", op="barrel_up", x=400, y=300)
    assert ind and ind.get("visible", True), ind
    assert lab["label"].startswith("Ask: listening 0:0"), lab
    assert not stop.get("visible", True), stop
    last_ask(c)
    assert not widget(c, "recordingIndicator").get("visible", True)


def test_12_the_pen_button_dictates_into_the_recording_chooser(app):
    # with the chooser open (a recording just stopped), holding the pen button fills "What should AI do?"
    c = app.client()
    ready(c)
    assert c.call("test_recording", file="/tmp/xoai-x.ogg", name="xoai-x.ogg", duration_ms=4000)["observed"]
    time.sleep(0.3)
    c.call("test_pen", op="hover", x=400, y=300)
    c.call("test_pen", op="barrel_down", x=400, y=300)
    time.sleep(SPEAK)
    c.call("test_pen", op="barrel_up", x=400, y=300)
    deadline = time.time() + 15
    while time.time() < deadline and not widget(c, "recording-request").get("value"):
        time.sleep(0.3)
    said = widget(c, "recording-request").get("value", "")
    assert "americans" in said.lower(), said
    assert c.call("app_status")["ask"].get("last", {}).get("text") != said  # (no Ask was made of it)
    c.call("ui_interact", op="click", target=widget(c, "recording-keep")["id"])
