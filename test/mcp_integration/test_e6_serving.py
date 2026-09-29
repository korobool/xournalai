"""E6: the serving session starts with the app in the AI terminal, in the companion folder."""

import os
import pathlib
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-fake-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def test_serving_session_autostarts(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG) and "START" in open(LOG).read())
    start = open(LOG).read().splitlines()[0]
    assert "xournalai/companion" in start and f"port={app.port}" in start
    tabs = [w.get("label") for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"]
            if w.get("type") == "GtkLabel" and "serving" in (w.get("label") or "")]
    assert tabs == ["Claude · serving"]
    # typing reaches it once you click into it (the app's own wake-ups come with the next stage)
    term = [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w["type"] == "VteTerminal"][0]
    x, y, w, h = term["bbox"]
    app._xdo("mousemove", x + w // 2, y + h // 2, "click", 1)
    time.sleep(0.2)
    app.user_key("h", "i", "Return")
    assert wait_for(lambda: "GOT hi" in open(LOG).read())
