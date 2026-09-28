"""E3: inspecting and operating the real user interface."""

import time

import xoai


def wait_window(c, kind="dialog", timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        wins = [w for w in c.call("ui_windows")["windows"] if w["kind"] == kind]
        if wins:
            return wins[0]
        time.sleep(0.1)
    raise AssertionError(f"no {kind} window appeared")


def test_windows_inspect_screenshot(app):
    c = app.client()
    wins = c.call("ui_windows")["windows"]
    assert [w["kind"] for w in wins] == ["main"], wins  # installed app: no startup error dialogs
    main = wins[0]
    zoom = c.call("ui_inspect", target=main["id"], filter="zoom")["widgets"]
    assert any(w["role"] in ("button", "toggle") for w in zoom)
    shot = c.call_raw("ui_screenshot", target=main["id"])
    img = xoai.Mcp.images(shot)[0]
    assert img[:4] == b"\x89PNG"


def test_dialog_is_inspectable(app):
    c = app.client()
    c.call("page_manage", op="insert", page=1)
    c.call("action_run", action="win.goto-page")
    dlg = wait_window(c)
    assert dlg["title"] == "Go to Page"
    widgets = c.call("ui_inspect", target=dlg["id"])["widgets"]
    names = {w.get("name"): w for w in widgets}
    assert names["spinPage"]["role"] == "spin" and names["btOk"]["label"] == "Ok"
    assert "Unknown or closed" in c.call_error("ui_inspect", target="w99999")
