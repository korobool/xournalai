"""E2: editing, deleting, selecting, undo/redo and history."""


def els(c):
    return {e["id"]: e for e in c.call("page_elements", detail="simplified", limit=5000)["elements"]}


def test_draw_undo_redo_keeps_ids(app):
    c = app.client()
    r = c.call("create_shapes", shapes=[{"type": "rectangle", "x": 50, "y": 50, "w": 100, "h": 50}], animate=False)
    rid = r["created"][0]["id"]
    assert rid in els(c)
    u = c.call("undo")
    assert u["undone"] and rid not in els(c)
    c.call("redo")
    assert rid in els(c)  # same element object, same id
    h = c.call("history", max=5)
    assert h["undo"]


def test_move_scale_rotate_and_undo(app):
    c = app.client()
    r = c.call("create_shapes", shapes=[{"type": "rectangle", "x": 100, "y": 200, "w": 100, "h": 50}],
               animate=False)
    rid = r["created"][0]["id"]
    x0 = els(c)[rid]["bbox"]
    m = c.call("elements_edit", op="move", element_ids=[rid], dx=30, dy=-10)
    assert abs(m["bbox"][0] - (x0[0] + 30)) < 0.5 and abs(m["bbox"][1] - (x0[1] - 10)) < 0.5
    c.call("undo")
    assert abs(els(c)[rid]["bbox"][0] - x0[0]) < 0.5
    s = c.call("elements_edit", op="scale", element_ids=[rid], factor=2)
    assert abs(s["bbox"][2] - 2 * x0[2]) < 3
    c.call("undo")
    rot = c.call("elements_edit", op="rotate", element_ids=[rid], degrees=90)
    assert abs(rot["bbox"][3] - x0[2]) < 3  # width became height
    c.call("undo")
    assert abs(els(c)[rid]["bbox"][2] - x0[2]) < 0.5


def test_restyle_reorder_layer_delete_by_operation(app):
    c = app.client()
    r = c.call("create_strokes", strokes=[{"points": [[50, 400], [200, 400]]}, {"points": [[50, 420], [200, 420]]}],
               width=2, animate=False)
    op = r["operation"]
    ids = [x["id"] for x in r["created"]]
    st = c.call("elements_edit", op="restyle", operation=op, color="#ff00ff", width=5, line_style="dash")
    assert st["changed"] == 2
    e = els(c)[ids[0]]
    assert e["color"] == "#ff00ff" and e["width"] == 5 and e["line_style"] == "dash"
    full = [x for x in c.call("page_elements", detail="full", limit=5000)["elements"] if x["id"] == ids[0]][0]
    assert max(p[2] for p in full["points"]) > 3  # per-point widths scaled along
    c.call("elements_edit", op="reorder", element_ids=[ids[0]], to="front")
    order = [x["id"] for x in c.call("page_elements", detail="bbox", limit=5000)["elements"]]
    assert order.index(ids[0]) > order.index(ids[1])
    moved = c.call("elements_edit", op="to_layer", element_ids=ids, layer="Other")
    assert moved["layer"] == "Other" and els(c)[ids[0]]["layer"] != r["layer"]
    c.call("undo")
    assert els(c)[ids[0]]["layer"] == r["layer"]
    d = c.call("elements_delete", operation=op)
    assert d["deleted"] == 2 and ids[0] not in els(c)
    c.call("undo")
    assert ids[0] in els(c)


def test_select_and_errors(app):
    c = app.client()
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [400, 600], "r": 20}], animate=False)
    s = c.call("elements_select", element_ids=[r["created"][0]["id"]])
    assert s["count"] == 1
    assert "Unknown element id" in c.call_error("elements_edit", op="move", element_ids=["e99999"], dx=1)
    assert "Give element_ids" in c.call_error("elements_delete")
    assert "positive" in c.call_error("elements_edit", op="scale", element_ids=[r["created"][0]["id"]], factor=-1)


def test_draft_commit_is_one_undo_step(app):
    c = app.client()
    d = c.call("draft", op="begin")
    c.call("create_shapes", shapes=[{"type": "line", "from": [10, 700], "to": [200, 700]}], layer=d["layer"],
           animate=False)
    c.call("create_shapes", shapes=[{"type": "line", "from": [10, 720], "to": [200, 720]}], layer=d["layer"],
           animate=False)
    before = len(c.call("history", max=500)["undo"])
    com = c.call("draft", op="commit", draft=d["draft"], animate=False)
    assert len(c.call("history", max=500)["undo"]) == before + 1
    c.call("undo")
    ids = els(c)
    assert all(x["id"] not in ids for x in com["created"])
