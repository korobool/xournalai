"""E3 milestone: operate dialogs, file choosers and keyboard like a user."""

import os
import tempfile
import time

from xoai import glyphs, make_xopp

DIR = tempfile.mkdtemp(prefix="xoai-dialogs-")
SOURCE = make_xopp(os.path.join(DIR, "a.xopp"), [glyphs(50, 50, 3), glyphs(50, 50, 5)])


def named(c, window_id):
    return {w.get("name"): w for w in c.call("ui_inspect", target=window_id)["widgets"] if w.get("name")}


def labelled(c, window_id, label):
    return [w for w in c.call("ui_inspect", target=window_id)["widgets"] if w.get("label") == label and
            w["role"] == "button"][0]


def test_goto_dialog(app):
    c = app.client()
    c.call("page_manage", op="insert", page=1)
    c.call("page_manage", op="insert", page=1)
    c.call("ui_menu_select", path="Navigation/Goto Page", visible=False)
    w = c.call("ui_wait_for_window", title="go to")["window"]
    ws = named(c, w["id"])
    c.call("ui_interact", op="set_value", target=ws["spinPage"]["id"], value=3)
    r = c.call("ui_interact", op="click", target=ws["btOk"]["id"])
    assert [x["kind"] for x in r["windows"]] == ["main"]
    assert c.call("view")["current_page"] == 3


def test_unsaved_prompt_open_and_save_as(app):
    c = app.client()
    c.call("ui_menu_select", path="File/Open", visible=False)
    prompt = c.call("ui_wait_for_window", kind="dialog")["window"]  # document has unsaved changes
    cancel = labelled(c, prompt["id"], "Cancel")
    c.call("ui_interact", op="click", target=cancel["id"])
    c.call("file_save_as", path=os.path.join(DIR, "work.xopp"))
    c.call("ui_menu_select", path="File/Open", visible=False)
    c.call("ui_wait_for_window", kind="file_chooser")
    c.call("ui_file_chooser", path=SOURCE)
    end = time.time() + 5
    while c.call("file_info")["path"] != SOURCE and time.time() < end:
        time.sleep(0.2)
    assert c.call("file_info")["path"] == SOURCE
    c.call("create_shapes", shapes=[{"type": "line", "from": [10, 10], "to": [100, 100]}], animate=False)
    c.call("ui_menu_select", path="File/Save As", visible=False)
    c.call("ui_wait_for_window", kind="file_chooser")
    target = os.path.join(DIR, "b.xopp")
    c.call("ui_file_chooser", path=target)
    end = time.time() + 5
    while not os.path.exists(target) and time.time() < end:
        time.sleep(0.2)
    assert os.path.exists(target) and c.call("file_info")["path"] == target


def test_keys(app):
    c = app.client()
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 20}], animate=False)
    k = c.call("ui_keys", op="shortcut", keys="<Ctrl>z")
    assert k["action"] == "win.undo"
    ids = [e["id"] for e in c.call("page_elements", detail="bbox", limit=500)["elements"]]
    assert r["created"][0]["id"] not in ids
    assert "parse" in c.call_error("ui_keys", op="shortcut", keys="<Ctrl>")
    c.call("ui_menu_select", path="Navigation/Goto Page", visible=False)
    w = c.call("ui_wait_for_window", title="go to")["window"]
    ws = named(c, w["id"])
    c.call("ui_interact", op="focus", target=ws["spinPage"]["id"])
    c.call("ui_keys", op="shortcut", keys="Escape")
    time.sleep(0.3)
    assert [x["kind"] for x in c.call("ui_windows")["windows"]] == ["main"]  # Escape closed the dialog
