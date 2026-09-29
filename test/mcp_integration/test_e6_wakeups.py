"""E6: the app watches the canvas and wakes the idle serving session; it never polls and never interrupts."""

import json
import os
import pathlib
import subprocess
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-wake-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}",
                            "auto_improve": True, "wake_idle_ms": 800, "watchdog_s": 2}}


def got():
    return [l[4:] for l in open(LOG).read().splitlines() if l.startswith("GOT ")] if os.path.exists(LOG) else []


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def hook(app, event, payload=None):
    subprocess.run([app.binary, "--ai-hook", event], input=json.dumps(payload or {"session_id": "t"}),
                   env=dict(app.app_env(), XOURNALAI_PORT=str(app.port)), text=True, timeout=10, check=True)


def status(c):
    return [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "mcpStatus"][0]["label"]


def draw(app, c, dx=0, center=None):
    cx, cy = center or app.canvas_center(c)
    app.user_drag([(cx - 60 + dx + i * 6, cy - 40 + (i % 3) * 3) for i in range(20)])


def test_1_user_strokes_wake_it_after_a_pause(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 10}], animate=False)
    time.sleep(2)
    assert got() == []  # the agent's own drawing never wakes it
    draw(app, c)
    assert wait_for(lambda: len([g for g in got() if g.startswith("[xournalai]")]) == 1)
    msg = [g for g in got() if g.startswith("[xournalai]")][0]
    assert "the user wrote/drew" in msg and "page 1" in msg and "changes_get since=" in msg and "Auto-improve is ON" in msg
    assert "Up to 5 subagents in parallel" in msg


def test_2_busy_sessions_are_not_disturbed(app):
    c = app.client()
    n = len(got())
    hook(app, "SessionStart")  # from now on the app knows its state from hooks
    hook(app, "UserPromptSubmit")  # busy
    draw(app, c, dx=30)
    time.sleep(2.5)
    assert len(got()) == n  # held back while busy
    hook(app, "Stop")  # idle again: delivered
    assert wait_for(lambda: len(got()) == n + 1)
    hook(app, "UserPromptSubmit")  # it picked it up
    hook(app, "Stop")


def test_3_watchdog_and_pause(app):
    c = app.client()
    n = len(got())
    draw(app, c, dx=60)
    assert wait_for(lambda: len(got()) == n + 1)
    # no UserPromptSubmit follows: after watchdog_s it presses Enter once more, then reports
    assert wait_for(lambda: "didn't react" in status(c), timeout=6)
    assert wait_for(lambda: "isn't responding" in status(c), timeout=6)
    hook(app, "Stop")
    center = app.canvas_center(c)  # (inspecting is blocked while paused)
    app.user_key("ctrl+alt+Escape")  # paused: nothing is delivered
    time.sleep(0.3)
    m = len(got())
    draw(app, c, dx=90, center=center)
    time.sleep(2.5)
    assert len(got()) == m  # held back while paused
    app.user_key("ctrl+alt+Escape")
    assert wait_for(lambda: len(got()) == m + 1)
