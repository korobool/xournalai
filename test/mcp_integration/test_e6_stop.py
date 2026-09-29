"""E6: stopping AI work: click a zone's spinner or Stop; queued requests are dropped, the session interrupted."""

import json
import os
import pathlib
import subprocess
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-stop-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def got():
    return [l for l in open(LOG).read().splitlines() if l.startswith("GOT [xournalai]")] if os.path.exists(LOG) else []


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


def status(c):
    return [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "mcpStatus"][0]["label"]


def test_1_click_the_spinner(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    hook(app, "SessionStart")
    c.call("action_run", action="win.ai-act", parameter="revise")
    z = max(zones(c))
    assert wait_for(lambda: zones(c).get(z) == "thinking")
    hook(app, "UserPromptSubmit")  # it's working on it
    assert "1 in progress" in status(c)
    btn = [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "aiThinkingButton"][0]
    x, y, w, h = btn["bbox"]
    app._xdo("mousemove", x + w // 2, y + h // 2, "click", 1)
    assert wait_for(lambda: zones(c).get(z) == "failed")
    assert "in progress" not in status(c)
    hook(app, "Stop")


def test_2_stop_drops_queued_requests(app):
    c = app.client()
    hook(app, "UserPromptSubmit")  # busy: new requests wait
    n = len(got())
    c.call("action_run", action="win.ai-act", parameter="web")
    c.call("action_run", action="win.ai-act", parameter="image")
    queued = [z for z, s in zones(c).items() if s == "queued"]
    assert len(queued) == 2
    c.call("action_run", action="win.ai-stop")
    assert all(zones(c).get(z) in ("failed", None) for z in queued)
    hook(app, "Stop")  # idle again: nothing was left to deliver
    time.sleep(2)
    assert len(got()) == n
    assert "AI Agent/Stop current AI work" in {e["path"] for e in c.call("ui_menu_tree")["entries"]}
