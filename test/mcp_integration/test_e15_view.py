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


def test_5_pages_coming_and_going_above_keep_the_view(app):
    c = app.client()
    setup(c)
    mine = c.call("create_strokes", page=3, strokes=[{"points": [[300, 420], [340, 440]]}],
                  animate=False)["created"][0]["id"]
    c.call("view", op="scroll", page=3, region=[0, 380, 200, 120])  # the user looks at the middle of page 3
    time.sleep(0.3)
    page, region = view(c)
    assert page == 3, page

    def mine_on(n):
        return any(e["id"] == mine for e in c.call("page_elements", page=n, detail="bbox", limit=5000)["elements"])

    c.call("page_manage", op="insert", page=0)  # above: the same page is now page 4, still where it was
    time.sleep(0.3)
    assert view(c) == (4, region), (view(c), region)
    assert mine_on(4)
    c.call("page_manage", op="delete", page=1)  # and back
    time.sleep(0.3)
    assert view(c) == (3, region), (view(c), region)
    assert mine_on(3)


def test_6_the_users_selection_survives_agent_work_on_other_pages(app):
    c = app.client()
    setup(c)
    stroke = [{"points": [[100, 100], [160, 130]]}]
    mine = c.call("create_strokes", page=2, strokes=stroke, animate=False)["created"][0]["id"]
    theirs = c.call("create_strokes", page=4, strokes=stroke, animate=False)["created"][0]["id"]

    def select_mine():
        c.call("elements_select", element_ids=[mine])  # (as if the user had selected it)
        assert c.call("view").get("selection")

    def commit(page):
        t = c.call("transaction_begin", label="t", page=page, region=[0, 0, 300, 300], base_ids=[])
        c.call("create_strokes", page=page, layer=t["draft_layer"], strokes=stroke, animate=False)
        c.call("transaction_commit", transaction=t["transaction"], ops=[{"op": "draw", "animate": False}])

    select_mine()
    for what, step in [("draw", lambda: c.call("create_strokes", page=4, strokes=stroke, animate=False)),
                       ("commit", lambda: commit(4)),
                       ("move", lambda: c.call("elements_edit", element_ids=[theirs], op="move", dx=5, dy=5)),
                       ("delete", lambda: c.call("elements_delete", element_ids=[theirs]))]:
        step()
        assert c.call("view").get("selection"), f"{what} on page 4 dropped the selection on page 2"
    # on its own page the agent goes first
    c.call("create_strokes", page=2, strokes=stroke, animate=False)
    assert not c.call("view").get("selection")
    # and an element the user holds can still be changed: the selection lets go of it
    select_mine()
    c.call("elements_edit", element_ids=[mine], op="move", dx=10, dy=0)
    assert not c.call("view").get("selection")


def chip(c):
    for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]:
        if w.get("name") == "aiAgentPageChip" and w.get("visible", True):
            return w.get("label") or w.get("text") or ""
    return None


def wait_for(pred, timeout=6):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.2)
    return pred()


def test_7_a_chip_shows_where_an_agent_works_out_of_view(app):
    c = app.client()
    setup(c, pages=7)
    stroke = [{"points": [[100, 100], [160, 130]]}]
    c.call("create_strokes", page=6, strokes=stroke, animate=False)
    assert wait_for(lambda: chip(c) == "AI ↓ p. 6"), chip(c)
    assert view(c)[0] == 2  # the user stays
    c.call("create_strokes", page=2, strokes=stroke, animate=False)  # in view: nothing new to point at
    time.sleep(0.5)
    assert chip(c) == "AI ↓ p. 6", chip(c)
    target = [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == "aiAgentPageChip"]
    c.call("ui_interact", op="click", target=target[0]["id"])  # the user goes there
    assert wait_for(lambda: view(c)[0] == 6 and chip(c) is None), (view(c), chip(c))
    c.call("create_strokes", page=1, strokes=stroke, animate=False)
    assert wait_for(lambda: chip(c) == "AI ↑ p. 1"), chip(c)
    assert wait_for(lambda: chip(c) is None, timeout=16)  # quiet for a while: it goes
