"""E15: a page with something on it is deleted only when confirmed; every page deletion is logged with what did it.
On 2026-11-09 the owner's drawn first page vanished twice during live notes; the app's change log only said "user"
(the toolbar's Delete page button deletes the current page at once, and a page move looks like a delete too)."""

import tempfile
import time

import xoai

DIR = tempfile.mkdtemp(prefix="xoai-pagedel-")
DOC = xoai.make_xopp(f"{DIR}/doc.xopp", [[xoai.stroke([(50, 50), (150, 80)])], [], []])
APP_ARGS = [DOC]


def dialog(c, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        wins = [w for w in c.call("ui_windows")["windows"] if w["kind"] != "main"]
        if wins:
            return wins[0]
        time.sleep(0.2)
    return None


def press(c, win, label):
    b = [w for w in c.call("ui_inspect", target=win["id"], max_depth=40, all=True)["widgets"]
         if w.get("label") == label][0]
    c.call("ui_interact", op="click", target=b["id"])
    time.sleep(0.4)


def pages(c):
    return len(c.call("doc_info")["pages"])


def test_1_a_page_with_ink_asks_first(app):
    c = app.client()
    c.call("page_manage", op="goto", page=1, show=True)
    c.call("action_run", action="win.delete-page")
    d = dialog(c)
    assert d, "no question"
    assert pages(c) == 3  # (nothing yet)
    press(c, d, "Cancel")
    assert pages(c) == 3 and not dialog(c, 1)
    c.call("action_run", action="win.delete-page")
    press(c, dialog(c), "Delete page")
    assert pages(c) == 2
    assert "Page 1 deleted (1 element(s)) by the Delete page action (confirmed)" in app.read_log()


def test_2_an_empty_page_goes_at_once_and_says_so(app):
    c = app.client()
    c.call("page_manage", op="goto", page=2, show=True)
    c.call("action_run", action="win.delete-page")
    assert not dialog(c, 1) and pages(c) == 1
    assert "Page 2 deleted (empty) by the Delete page action" in app.read_log()


def test_3_undo_brings_them_back_and_an_agents_delete_is_logged(app):
    c = app.client()
    c.call("undo")
    c.call("undo")
    assert pages(c) == 3
    c.call("page_manage", op="delete", page=3)
    assert "Page 3 deleted (0 element(s)) through MCP" in app.read_log()
