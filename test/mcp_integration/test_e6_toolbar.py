"""E6: the AI toolbar turns the marker actions into intents for the serving session and for any agent."""

import os
import pathlib
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-tb-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def got():
    return [l[4:] for l in open(LOG).read().splitlines() if l.startswith("GOT [xournalai]")] if os.path.exists(LOG) else []


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def widgets(c):
    return c.call("ui_inspect", max_depth=60, all=True)["widgets"]


def test_1_toolbar_row(app):
    c = app.client()
    names = {w.get("name") for w in widgets(c)}
    for n in ("aiToolbar", "aiAct-improve", "aiAct-illustrate", "aiAct-web", "aiAct-image", "aiAct-command",
              "aiAct-revise", "aiAutoImprove", "aiPause", "aiTerminal"):
        assert n in names, n
    assert "AI Agent/Show AI toolbar" in {e["path"] for e in c.call("ui_menu_tree")["entries"]}


def test_2_actions_become_intents(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    cursor = c.call("changes_get")["cursor"]
    c.call("action_run", action="win.ai-act", parameter="revise")  # nothing selected, nothing drawn: the page
    assert wait_for(lambda: len(got()) == 1)
    assert "revise the whole page" in got()[0] and "the whole page on page 1" in got()[0]
    circle = c.call("create_shapes", shapes=[{"type": "circle", "center": [200, 200], "r": 30}],
                    animate=False)["created"][0]["id"]
    c.call("elements_select", element_ids=[circle])
    c.call("action_run", action="win.ai-act", parameter="improve")
    assert wait_for(lambda: len(got()) == 2)
    assert "*! improve the strokes" in got()[1] and "the selection (1 element(s))" in got()[1] and circle in got()[1]
    c.call("action_run", action="win.ai-act", parameter="command:make it red")
    assert wait_for(lambda: len(got()) == 3) and '*c! the user\'s command: "make it red"' in got()[2]
    intents = [e for e in c.call("changes_get", since=cursor)["events"] if e["type"] == "intent"]
    assert len(intents) == 3 and intents[1]["ids"] == [circle]


def test_3_command_button_prompt(app):
    c = app.client()
    n = len(got())
    btn = [w for w in widgets(c) if w.get("name") == "aiAct-command"][0]
    x, y, w, h = btn["bbox"]
    app._xdo("mousemove", x + w // 2, y + h // 2, "click", 1)
    assert wait_for(lambda: any(w.get("name") == "aiCommandEntry" for w in widgets(c)), timeout=5)
    for ch in "tidy":
        app.user_key(ch)
    app.user_key("Return")
    assert wait_for(lambda: len(got()) == n + 1) and '"tidy"' in got()[-1]
