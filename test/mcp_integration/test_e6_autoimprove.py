"""E6: the Auto-improve toggle: on = everything the user writes wakes the session; off = commands only."""

import json
import os
import pathlib
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-ai-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}",
                            "wake_idle_ms": 600}}


def got():
    return [l[4:] for l in open(LOG).read().splitlines() if l.startswith("GOT [xournalai]")] if os.path.exists(LOG) else []


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def status(c):
    return [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "mcpStatus"][0]["label"]


def draw(app, c, dx):
    cx, cy = app.canvas_center(c)
    app.user_drag([(cx - 80 + dx + i * 6, cy - 50 + (i % 3) * 3) for i in range(18)])


def test_auto_improve_toggle(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    assert "Auto-improve off" in status(c)
    draw(app, c, 0)
    time.sleep(2)
    assert got() == []  # off: drawing alone wakes nothing

    app.user_key("ctrl+alt+i")  # the shortcut turns it on
    assert wait_for(lambda: "Auto-improve ON" in status(c), timeout=5)
    assert json.loads(app.config_file.read_text())["assistant"]["auto_improve"] is True
    c.call("action_run", action="win.ai-rule-colours", state=False)
    assert "colours" not in json.loads(app.config_file.read_text())["assistant"]["rules"]
    time.sleep(5.5)  # the fake agent has no hooks: wake-ups are spaced
    draw(app, c, 40)
    assert wait_for(lambda: len(got()) == 1)
    assert "Auto-improve is ON (rules: formulas, text, diagrams)" in got()[0]

    c.call("action_run", action="win.ai-auto-improve", state=False)
    assert json.loads(app.config_file.read_text())["assistant"]["auto_improve"] is False
    time.sleep(5.5)
    draw(app, c, 80)
    time.sleep(2)
    assert len(got()) == 1
    menu = {e["path"] for e in c.call("ui_menu_tree")["entries"]}
    assert "AI Agent/Auto-improve" in menu and "AI Agent/Auto-improve rules/Consistent colours" in menu
