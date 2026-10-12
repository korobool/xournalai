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
    # (the dock opens asynchronously)
    assert wait(lambda: widgets(c, "aiTerminalDock") and widgets(c, "aiTerminalDock")[0].get("visible", True))
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


def test_10_colours_and_zoom_in_the_tools(app):
    """Tools has the palette's colours (a colour picks the pen if the tool has none) and zoom."""
    c = app.client()
    c.call("tool_set", tool="pen", color="#123456")
    c.call("tool_set", tool="eraser")
    c.call("ui_interact", op="click", target=widgets(c, "aiQuickToolsButton")[0]["id"])
    assert wait(lambda: len(tiles(c)) >= 10)
    shown = tiles(c)
    for t in ("color-0", "color-1", "zoom-in", "zoom-out", "zoom-fit"):
        assert t in shown
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-color-1")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "pen")
    assert wait(lambda: not c.call("tool_get")["color"].startswith("#123456"))


def test_11_focus_hides_menus_and_toolbars(app):
    """Focus hides the menu bar and the toolbars; the big buttons stay and bring them back. Remembered."""
    c = app.client()
    shown = lambda name: widgets(c, name)[0].get("visible", True)
    labels = lambda: [w.get("label") or w.get("text") or "" for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert "Focus" in labels()
    c.call("ui_interact", op="click", target=widgets(c, "aiFocusButton")[0]["id"])
    assert wait(lambda: "Menus" in labels())
    assert wait(lambda: not shown("aiToolbarColumn"))
    assert counts(app)["_focus"] == 1
    assert widgets(c, "aiQuickToolsButton")[0].get("visible", True)
    c.call("action_run", action="win.ai-focus", state=False)  # (as F9 does)
    assert wait(lambda: "Focus" in labels())
    assert wait(lambda: shown("aiToolbarColumn"))
    assert counts(app)["_focus"] == 0


def test_12_shapes_in_the_tools(app):
    """Shapes are drawn with the pen: a shape tile picks the pen and the shape; tapping it again, freehand."""
    c = app.client()
    c.call("tool_set", tool="eraser")
    c.call("ui_interact", op="click", target=widgets(c, "aiQuickToolsButton")[0]["id"])
    assert wait(lambda: "shape-rect" in tiles(c))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-shape-rect")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "pen")
    assert wait(lambda: c.call("tool_get")["drawing_type"] == "rectangle")
    time.sleep(0.5)
    c.call("ui_interact", op="click", target=widgets(c, "aiQuickToolsButton")[0]["id"])
    assert wait(lambda: "shape-rect" in tiles(c))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-shape-rect")[0]["id"])
    assert wait(lambda: c.call("tool_get")["drawing_type"] == "default")


def test_13_tablet_buttons_in_focus(app):
    """In Focus the strip's left side holds big icon buttons (in priority order); the pen gets a colour button."""
    c = app.client()
    shown = lambda name: bool(widgets(c, name)) and widgets(c, name)[0].get("visible", True)
    assert not shown("aiTabletButtons")
    c.call("action_run", action="win.ai-focus", state=True)
    assert wait(lambda: shown("aiTabletButtons"))
    # (the test window is narrow: the first ones are there, the last ones may have given way)
    for t in ("ai-highlighter", "ask", "pen"):
        assert shown("aiTablet-" + t), t
    tiles = [w for w in widgets(c, prefix="aiTablet-") if w.get("visible", True) and w["name"] != "aiTablet-colours"]
    assert tiles == sorted(tiles, key=lambda w: w["bbox"][0])
    assert max(w["bbox"][0] + w["bbox"][2] for w in tiles) <= widgets(c, "aiFocusButton")[0]["bbox"][0]
    c.call("ui_interact", op="click", target=widgets(c, "aiTablet-eraser")[0]["id"]) if shown("aiTablet-eraser") else \
        c.call("tool_set", tool="eraser")
    assert wait(lambda: c.call("tool_get")["tool"] == "eraser")
    assert wait(lambda: not shown("aiTablet-colours"))
    c.call("ui_interact", op="click", target=widgets(c, "aiTablet-pen")[0]["id"])
    assert wait(lambda: c.call("tool_get")["tool"] == "pen")
    assert wait(lambda: shown("aiTablet-colours"))
    c.call("tool_set", tool="pen", color="#123456")
    c.call("ui_interact", op="click", target=widgets(c, "aiTablet-colours")[0]["id"])
    assert wait(lambda: shown("aiQuick-color-1"))
    c.call("ui_interact", op="click", target=widgets(c, "aiQuick-color-1")[0]["id"])
    assert wait(lambda: not c.call("tool_get")["color"].startswith("#123456"))
    c.call("action_run", action="win.ai-focus", state=False)
    assert wait(lambda: not shown("aiTabletButtons"))


def test_14_focus_survives_full_screen(app):
    """Full screen on and off doesn't bring the toolbars back while Focus is on; leaving Focus does."""
    c = app.client()
    shown = lambda name: widgets(c, name)[0].get("visible", True)
    bars = lambda: [w for w in widgets(c) if (w.get("name") or "") in ("tbTop1", "tbTop2", "tbLeft1", "tbBottom1")]
    c.call("action_run", action="win.ai-focus", state=True)
    assert wait(lambda: not shown("aiToolbarColumn"))
    for on in (True, False):
        c.call("action_run", action="win.fullscreen", state=on)
        time.sleep(1)
        assert not shown("aiToolbarColumn")
        assert not any(w.get("visible", True) and w["bbox"][2] > 1 for w in bars() if w["name"] != "tbTop1" and w["name"] != "tbTop2"), bars()
    c.call("action_run", action="win.ai-focus", state=False)
    assert wait(lambda: shown("aiToolbarColumn"))


def test_15_tablet_states_stay_consistent(app):
    """Picking a tool ends the AI highlighter / lasso (even the tool in use); any tool after any other."""
    c = app.client()
    tool = lambda: c.call("tool_get")["tool"]
    state = lambda a: [x.get("state") for x in c.call("actions_list", filter=a)["actions"] if x["name"] == "win." + a][0]
    c.call("action_run", action="win.ai-focus", state=True)
    assert wait(lambda: widgets(c, "aiTablet-pen")[0].get("visible", True))
    click = lambda t: c.call("ui_interact", op="click", target=widgets(c, "aiTablet-" + t)[0]["id"])
    click("pen")
    assert wait(lambda: tool() == "pen")
    click("ai-highlighter")
    assert wait(lambda: state("ai-highlighter") is True)
    click("pen")  # the tool in use already: still ends the AI highlighter
    assert wait(lambda: state("ai-highlighter") is False) and tool() == "pen"
    click("ai-highlighter")
    assert wait(lambda: state("ai-highlighter") is True)
    click("ai-highlighter")  # a second tap: off
    assert wait(lambda: state("ai-highlighter") is False)
    click("ask")
    assert wait(lambda: state("ai-ask") is True)
    click("eraser")  # a tool ends the armed lasso
    assert wait(lambda: tool() == "eraser") and wait(lambda: state("ai-ask") is False)
    click("ai-highlighter")
    assert wait(lambda: state("ai-highlighter") is True)
    click("ask")  # the lasso replaces the highlighter
    assert wait(lambda: state("ai-highlighter") is False) and state("ai-ask") is True
    click("pen")
    assert wait(lambda: tool() == "pen") and state("ai-ask") is False
    c.call("action_run", action="win.ai-focus", state=False)
