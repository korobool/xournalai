"""E7: parallel work is visible: every transaction has a zone, subagents are counted, Stop ends everything."""

import json
import os
import pathlib
import re
import subprocess
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-par-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def zones(c):
    return {z["id"]: (z["state"], z["text"]) for z in c.call("thinking_list")["zones"]}


def hook(app, event, payload=None):
    subprocess.run([app.binary, "--ai-hook", event], input=json.dumps(payload or {"session_id": "t"}),
                   env=dict(app.app_env(), XOURNALAI_PORT=str(app.port)), text=True, timeout=10, check=True)


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def got():
    return [l for l in open(LOG).read().splitlines() if l.startswith("GOT [xournalai]")] if os.path.exists(LOG) else []


def test_1_every_transaction_is_visible(app):
    c = app.client()
    t = c.call("transaction_begin", label="colour the butterfly", region=[100, 100, 150, 100])
    z = [k for k, v in zones(c).items() if v[1].startswith("colour the butterfly")]
    assert len(z) == 1 and zones(c)[z[0]][0] == "thinking"
    c.call("create_shapes", shapes=[{"type": "circle", "center": [150, 150], "r": 20}], layer=t["draft_layer"],
           animate=False)
    c.call("transaction_commit", transaction=t["transaction"], ops=[{"op": "draw", "animate": False}])
    assert zones(c).get(z[0], ("gone",))[0] in ("done", "gone")


def test_2_request_zone_travels_to_the_transaction(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    hook(app, "SessionStart")
    c.call("action_run", action="win.ai-act", parameter="illustrate")
    assert wait_for(lambda: any("[zone " in g for g in got()))
    zone = int(re.search(r"\[zone (\d+)\]", [g for g in got() if "[zone " in g][-1]).group(1))
    hook(app, "UserPromptSubmit")
    hook(app, "PreToolUse", {"session_id": "t", "tool_name": "Agent"})  # the coordinator starts a subagent
    assert c.call("app_status")["mcp"]["serving"]["subagents"] == 1
    t = c.call("transaction_begin", label="illustration", zone=zone)  # the subagent claims the request's zone
    assert zones(c)[zone] == ("thinking", "illustration…")
    hook(app, "Stop")  # the coordinator ends its turn while the subagent works: the zone stays
    time.sleep(0.5)
    assert zones(c)[zone][0] == "thinking"
    c.call("transaction_commit", transaction=t["transaction"], ops=[])
    hook(app, "SubagentStop")
    assert "subagents" not in c.call("app_status")["mcp"]["serving"]
    assert zones(c).get(zone, ("gone",))[0] in ("done", "gone")


def test_3_stop_ends_parallel_work(app):
    c = app.client()
    a = c.call("transaction_begin", label="a", region=[0, 0, 50, 50])
    b = c.call("transaction_begin", label="b", region=[300, 300, 50, 50])
    za = [k for k, v in zones(c).items() if v[1] == "a…"][0]
    c.call("action_run", action="win.ai-stop")
    assert zones(c)[za][0] == "failed"
    assert c.call("transaction_list")["transactions"] == []
    assert "stopped by the user" in c.call_error("transaction_commit", transaction=b["transaction"], ops=[])
