"""E13: the owner closes terminal tabs they don't want and opens tabs with their own command lines."""

import json
import os
import tempfile
import time


def widgets(c, name):
    return [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == name]


def wait(pred, timeout=8):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def test_1_close_tab_ends_its_program(app):
    c = app.client()
    marker = os.path.join(tempfile.mkdtemp(prefix="xoai-close-"), "alive")
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    assert wait(lambda: len(widgets(c, "aiTerminalCloseTab")) == 2)
    for ch in f"sleep 600; touch {marker}":
        app.user_key({" ": "space", ";": "semicolon", "/": "slash", "-": "minus", ".": "period",
                      "_": "underscore"}.get(ch, ch))
    app.user_key("Return")
    time.sleep(0.5)
    # the × of the second tab (the one shown)
    c.call("ui_interact", op="click", target=widgets(c, "aiTerminalCloseTab")[1]["id"])
    assert wait(lambda: len(widgets(c, "aiTerminalCloseTab")) == 1)
    c.call("ui_interact", op="click", target=widgets(c, "aiTerminalCloseTab")[0]["id"])
    assert wait(lambda: not widgets(c, "aiTerminalCloseTab"))
    dock = widgets(c, "aiTerminalDock")
    assert dock and not dock[0].get("visible", True)  # nothing left to show
    assert not os.path.exists(marker)


def test_2_custom_command_opens_and_is_remembered(app):
    c = app.client()
    out = os.path.join(tempfile.mkdtemp(prefix="xoai-custom-"), "out.txt")
    c.call("action_run", action="win.ai-terminal-custom")
    assert wait(lambda: widgets(c, "aiTerminalCustomCommand"))
    cmd = f"echo custom-$((6*7)) > {out}; sleep 30"
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalCustomCommand")[0]["id"], value=cmd)
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalCustomTitle")[0]["id"], value="Mine")
    c.call("ui_interact", op="activate", target=widgets(c, "aiTerminalCustomCommand")[0]["id"])
    assert wait(lambda: os.path.exists(out) and open(out).read().strip() == "custom-42")
    labels = [w.get("text") or w.get("label") for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert "Mine" in labels
    saved = json.load(open(str(app.home / "config" / "xournalpp" / "ai-terminals.json")))["commands"]
    assert saved[0]["command"] == cmd and saved[0]["title"] == "Mine"
    # offered again: the popover lists it, and it can be forgotten there
    c.call("action_run", action="win.ai-terminal-custom")
    assert wait(lambda: widgets(c, "aiTerminalSaved"))
    c.call("ui_interact", op="click", target=widgets(c, "aiTerminalForget")[0]["id"])
    assert wait(lambda: json.load(open(str(app.home / "config" / "xournalpp" / "ai-terminals.json")))["commands"] == [])
