"""E15: agents work beside the user without moving their view. page_manage made each page it touched the current one
and scrolled there (2026-10-20: an insert at the end took the owner from page 1 to page 7); the view now moves only
when asked (goto, show=true)."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def view(c):
    v = c.call("view")
    return v["current_page"], [round(x) for x in v.get("visible_region", [])]


def setup(c, pages=6):
    while c.call("doc_info")["page_count"] < pages:
        c.call("page_manage", op="insert", page=c.call("doc_info")["page_count"])
    c.call("page_manage", op="goto", page=2)  # the user is on page 2
    time.sleep(0.3)


def test_1_page_operations_elsewhere_leave_the_view(app):
    c = app.client()
    setup(c)
    before = view(c)
    assert before[0] == 2
    steps = [("insert at the end", dict(op="insert", page=6)),
             ("insert with a background", dict(op="insert", page=4, background="graph")),
             ("background", dict(op="background", page=5, background="dotted")),
             ("colour", dict(op="background", page=5, color="#fff8e0")),
             ("size", dict(op="size", page=6, width=400, height=300)),
             ("duplicate", dict(op="duplicate", page=5)),
             ("move", dict(op="move", page=6, to=8)),
             ("delete", dict(op="delete", page=8))]
    for what, args in steps:
        c.call("page_manage", **args)
        time.sleep(0.2)
        assert view(c) == before, (what, view(c), before)
    info = c.call("page_manage", op="goto", page=5)
    assert info["current_page"] == 5  # goto still moves it


def test_2_show_takes_the_user_to_the_page(app):
    c = app.client()
    setup(c)
    r = c.call("page_manage", op="insert", page=3, show=True)
    assert r["current_page"] == 4 and view(c)[0] == 4, r


def test_3_an_inserted_page_is_one_undo_step(app):
    c = app.client()
    setup(c)
    count = c.call("doc_info")["page_count"]
    c.call("page_manage", op="insert", page=count, background="graph")
    assert c.call("doc_info")["page_count"] == count + 1
    c.call("action_run", action="win.undo")
    assert c.call("doc_info")["page_count"] == count


def test_4_layer_operations_elsewhere_leave_the_view(app):
    c = app.client()
    setup(c)
    before = view(c)
    steps = [dict(op="add", page=5, name="AI notes"), dict(op="rename", page=5, layer="AI notes", name="AI draft 2"),
             dict(op="hide", page=5, layer="AI draft 2"), dict(op="show", page=5, layer="AI draft 2"),
             dict(op="copy", page=5, layer="AI draft 2"), dict(op="move_down", page=5, layer=3),
             dict(op="merge_down", page=5, layer=2), dict(op="select", page=5, layer=1),
             dict(op="delete", page=5, layer=2)]
    for args in steps:
        r = c.call("layer_manage", **args)
        time.sleep(0.1)
        assert view(c) == before, (args, view(c), before)
    # add, copy (+2), merge_down, delete (-2): the page has its one layer again, and the work happened there
    assert len(r["layers"]) == 1, r["layers"]
