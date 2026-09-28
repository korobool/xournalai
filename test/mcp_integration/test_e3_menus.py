"""E3: the main menu, navigated like a user."""

import threading
import time


def test_menu_tree(app):
    c = app.client()
    t = c.call("ui_menu_tree")
    paths = {e["path"]: e for e in t["entries"]}
    assert t["count"] > 100
    assert paths["File/Save"]["action"] == "win.save" and paths["File/Save"]["shortcut"] == "<Ctrl>s"
    assert "checked" in paths["Edit/Grid Snapping"]
    assert paths["File"]["submenu"]


def test_visible_menu_navigation_opens_real_menus(app):
    c = app.client()
    seen = {}

    def slow_select():
        seen["result"] = app.client().call("ui_menu_select", path="Journal/New Page After", step_ms=900)

    before = c.call("doc_info")["page_count"]
    th = threading.Thread(target=slow_select)
    th.start()
    time.sleep(1.4)
    kinds = [w["kind"] for w in c.call("ui_windows")["windows"]]
    th.join(timeout=20)
    assert "menu" in kinds, kinds  # the Journal menu was really open on screen
    assert seen["result"]["selected"] == "Journal/New Page After"
    assert c.call("doc_info")["page_count"] == before + 1
    assert [w["kind"] for w in c.call("ui_windows")["windows"]] == ["main"]  # menu closed again


def test_menu_opens_dialog_and_errors(app):
    c = app.client()
    r = c.call("ui_menu_select", path="goto page", visible=False)  # suffix, case-insensitive
    assert r["selected"] == "Navigation/Goto Page"
    kinds = [w["kind"] for w in c.call("ui_windows")["windows"]]
    assert "dialog" in kinds
    assert "No menu entry" in c.call_error("ui_menu_select", path="File/Teleport")
    assert "submenu" in c.call_error("ui_menu_select", path="File/Recent Documents")
    assert "destructive" in c.call_error("ui_menu_select", path="File/Quit")
