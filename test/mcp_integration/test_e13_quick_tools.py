"""E13: one big button (or F8) opens the owner's most used tools as big buttons; tapping one uses it, and the
order follows how often each tool is used, from anywhere."""

import json
import time


def widgets(c, name=None, prefix=None):
    ws = c.call("ui_inspect", max_depth=80, all=True)["widgets"]
    return [w for w in ws if (name and w.get("name") == name) or (prefix and (w.get("name") or "").startswith(prefix))]


def wait(pred, timeout=6):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def tiles(c):
    # (a GtkGrid lists its children last-added first)
    return list(reversed([w["name"][len("aiQuick-"):] for w in widgets(c, prefix="aiQuick-")]))


def counts(app):
    try:
        return json.load(open(app.home / "config" / "xournalpp" / "quick-tools.json"))["uses"]
    except FileNotFoundError:
        return {}


def test_1_big_button_opens_ten_tools(app):
    c = app.client()
    button = widgets(c, "aiQuickToolsButton")
    assert button and button[0].get("visible", True)
    c.call("ui_interact", op="click", target=button[0]["id"])
    assert wait(lambda: len(tiles(c)) == 10)
    shown = tiles(c)
    assert shown[:3] == ["pen", "eraser", "ask"], shown
    for t in ("select", "terminal", "size", "new-page"):
        assert t in shown


def test_2_tap_uses_the_tool_and_counts_it(app):
    c = app.client()
    c.call("action_run", action="win.ai-quick-tools")
    assert wait(lambda: widgets(c, "aiQuick-eraser"))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-eraser")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "eraser")
    assert wait(lambda: not widgets(c, "aiQuick-eraser"))  # the tap closed the grid
    assert counts(app).get("eraser") == 1


def test_3_order_follows_use_from_anywhere(app):
    c = app.client()
    for _ in range(3):  # the toolbar, a menu or a shortcut: all through the app's action
        c.call("action_run", action="win.select-tool", parameter=13)  # TOOL_HAND
    assert counts(app).get("hand") == 3
    c.call("action_run", action="win.ai-quick-tools")
    assert wait(lambda: len(tiles(c)) == 10)
    assert tiles(c)[0] == "hand", tiles(c)


def test_4_size_cycles(app):
    c = app.client()
    c.call("action_run", action="win.ai-quick-tools")
    assert wait(lambda: widgets(c, "aiQuick-pen"))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-pen")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "pen")
    size0 = c.call("tool_get").get("size")
    c.call("action_run", action="win.ai-quick-tools")
    assert wait(lambda: widgets(c, "aiQuick-size"))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-size")[0]["id"])
    assert wait(lambda: c.call("tool_get").get("size") != size0)


def test_5_ai_tiles(app):
    c = app.client()
    for tool in ("terminal", "ask"):
        c.call("action_run", action="win.ai-quick-tools")
        assert wait(lambda: widgets(c, "aiQuick-" + tool))
        c.call("ui_interact", op="click", target=widgets(c, "aiQuick-" + tool)[0]["id"])
    dock = widgets(c, "aiTerminalDock")
    assert dock and dock[0].get("visible", True)
    states = {a["name"]: a.get("state") for a in c.call("actions_list", filter="ai-ask")["actions"]}
    assert states.get("win.ai-ask") is True, states.get("win.ai-ask")
    c.call("action_run", action="win.ai-ask", state=False)  # off again: not a use
    assert counts(app).get("ask") == 1 and counts(app).get("terminal") == 1, counts(app)
