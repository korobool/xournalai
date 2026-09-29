"""E6: visible thinking: zones on the canvas for everything the AI works on."""

import json
import os
import pathlib
import subprocess
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-th-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def zones(c):
    return {z["id"]: z["state"] for z in c.call("thinking_list")["zones"]}


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def hook(app, event):
    subprocess.run([app.binary, "--ai-hook", event], input=json.dumps({"session_id": "t"}),
                   env=dict(app.app_env(), XOURNALAI_PORT=str(app.port)), text=True, timeout=10, check=True)


def test_1_session_marks_its_own_work(app):
    c = app.client()
    z = c.call("thinking", op="start", page=1, region=[100, 100, 200, 80], text="typesetting formula…")["id"]
    assert zones(c)[z] == "thinking"
    overlay = [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "aiThinkingButton"]
    assert overlay and overlay[0].get("visible", True)
    c.call("thinking", op="update", id=z, text="drawing 2/3")
    c.call("thinking", op="done", id=z)
    assert zones(c)[z] == "done"
    assert wait_for(lambda: z not in zones(c), timeout=5)  # the tick fades away
    with_fail = c.call("thinking", op="start", text="whole page")["id"]
    c.call("thinking", op="fail", id=with_fail, text="couldn't read it")
    assert zones(c)[with_fail] == "failed"


def test_2_toolbar_intent_zone_follows_the_session(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    hook(app, "SessionStart")
    before = set(zones(c))
    c.call("create_shapes", shapes=[{"type": "circle", "center": [200, 300], "r": 30}], animate=False)
    c.call("action_run", action="win.ai-act", parameter="illustrate")
    new = [z for z in zones(c) if z not in before]
    assert len(new) == 1
    z = new[0]
    assert wait_for(lambda: zones(c).get(z) == "thinking")  # delivered to the idle session
    hook(app, "UserPromptSubmit")
    hook(app, "Stop")  # it finished its turn
    assert wait_for(lambda: zones(c).get(z) in ("done", None))


def test_3_it_looks_right(app):
    c = app.client()
    c.call("thinking", op="start", page=1, region=[60, 380, 300, 120], text="improving strokes…")
    time.sleep(0.5)
    shot = c.call_raw("ui_screenshot")
    assert not shot.get("isError")


def test_4_the_pen_and_clicks_go_through(app):
    # zones must never block drawing (a windowed overlay once swallowed every click and stroke)
    c = app.client()
    cx, cy = app.canvas_center(c)
    c.call("thinking", op="start", page=1, text="covering the whole page")
    before = len(c.call("page_elements", detail="bbox", limit=5000)["elements"])
    app.user_drag([(cx - 60 + i * 6, cy + (i % 3) * 2) for i in range(20)])
    time.sleep(0.3)
    assert len(c.call("page_elements", detail="bbox", limit=5000)["elements"]) == before + 1
