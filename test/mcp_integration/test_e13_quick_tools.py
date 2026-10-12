"""E13: one big button (or F8) opens the owner's most used tools as big buttons; tapping one uses it, and the
order follows how often each tool is used, from anywhere."""

import json

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
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


def test_1_big_button_opens_the_tools(app):
    c = app.client()
    button = widgets(c, "aiQuickToolsButton")
    assert button and button[0].get("visible", True)
    c.call("ui_interact", op="click", target=button[0]["id"])
    assert wait(lambda: len(tiles(c)) >= 10)
    shown = tiles(c)
    for t in ("select", "select-rect", "select-object", "terminal", "size-0", "size-4", "new-page"):
        assert t in shown
    assert "hand" not in shown and "size" not in shown
    for t in ():
        assert t in shown


def test_2_tap_uses_the_tool_and_counts_it(app):
    c = app.client()
    c.call("action_run", action="win.ai-quick-tools")
    assert wait(lambda: widgets(c, "aiQuick-eraser"))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-eraser")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "eraser")
    assert wait(lambda: not widgets(c, "aiQuick-eraser"))  # the tap closed the grid
    assert counts(app).get("eraser") == 1


def test_3_use_is_counted_from_anywhere(app):
    c = app.client()
    for _ in range(3):  # the toolbar, a menu or a shortcut: all through the app's action
        c.call("action_run", action="win.select-tool", parameter=13)  # TOOL_HAND
    assert counts(app).get("hand") == 3


def test_4_explicit_sizes(app):
    c = app.client()
    for size, name in (("size-4", "veryThick"), ("size-1", "thin")):
        c.call("action_run", action="win.ai-quick-tools")
        assert wait(lambda: widgets(c, "aiQuick-" + size))
        c.call("ui_interact", op="click", target=widgets(c, "aiQuick-" + size)[0]["id"])
        assert wait(lambda: c.call("tool_get").get("size") == name), c.call("tool_get")


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


def test_6_tools_button_beside_the_toolbars(app):
    """One big button to the right of the toolbars (no row of its own); groups with the three ways of selecting;
    hidden on request and remembered."""
    c = app.client()
    assert widgets(c, "aiToolbarsWithTools") and not widgets(c, "aiTouchBar")
    c.call("ui_interact", op="click", target=widgets(c, "aiQuickToolsButton")[0]["id"])
    assert wait(lambda: widgets(c, "aiQuick-select-object"))
    for t in ("select", "select-rect", "select-object", "pen", "eraser", "undo", "ask"):
        assert widgets(c, "aiQuick-" + t), t
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-select-object")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "selectObject"), c.call("tool_get")
    c.call("action_run", action="win.ai-tools-button", state=False)
    assert wait(lambda: not widgets(c, "aiQuickToolsButton")[0].get("visible", True))
    assert json.load(open(app.home / "config" / "xournalpp" / "quick-tools.json"))["uses"]["_toolsButton"] == 0
    c.call("action_run", action="win.ai-tools-button", state=True)
    assert wait(lambda: widgets(c, "aiQuickToolsButton")[0].get("visible", True))


def test_7_record_button_beside_tools(app):
    """A big Record button next to Tools (experimental), as tall; hidden on request and remembered."""
    c = app.client()
    rec, tools = widgets(c, "aiRecordButton")[0], widgets(c, "aiQuickToolsButton")[0]
    assert rec.get("visible", True)
    assert rec["bbox"][0] + rec["bbox"][2] <= tools["bbox"][0] and abs(rec["bbox"][3] - tools["bbox"][3]) <= 2
    c.call("action_run", action="win.ai-record-button", state=False)
    assert wait(lambda: not widgets(c, "aiRecordButton")[0].get("visible", True))
    assert json.load(open(app.home / "config" / "xournalpp" / "quick-tools.json"))["uses"]["_recordButton"] == 0
    c.call("action_run", action="win.ai-record-button", state=True)
    assert wait(lambda: widgets(c, "aiRecordButton")[0].get("visible", True))


def test_8_record_button_follows_the_recorder(app):
    """Started anywhere (here: the test hook, as the recorder would), the big button turns into Stop with the time."""
    c = app.client()
    labels = lambda: [w.get("label") or w.get("text") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert "Record" in labels()
    c.call("test_recorder", on=True)
    assert wait(lambda: any(l.startswith("Stop 0:0") for l in labels()))
    c.call("test_recorder", on=False)
    assert wait(lambda: "Record" in labels())


def test_9_fullscreen_button(app):
    """A big Full screen button next to Record: tapping it toggles full screen, and it says how to leave."""
    c = app.client()
    full, rec = widgets(c, "aiFullscreenButton")[0], widgets(c, "aiRecordButton")[0]
    assert full.get("visible", True)
    assert full["bbox"][0] + full["bbox"][2] <= rec["bbox"][0] and abs(full["bbox"][3] - rec["bbox"][3]) <= 2
    labels = lambda: [w.get("label") or w.get("text") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert "Full screen" in labels()
    c.call("ui_interact", op="click", target=full["id"])
    assert wait(lambda: "Window" in labels())
    c.call("ui_interact", op="click", target=widgets(c, "aiFullscreenButton")[0]["id"])
    assert wait(lambda: "Full screen" in labels())
    c.call("action_run", action="win.ai-fullscreen-button", state=False)
    assert wait(lambda: not widgets(c, "aiFullscreenButton")[0].get("visible", True))
    assert counts(app)["_fullscreenButton"] == 0
