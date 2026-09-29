"""E7: edit transactions: agents prepare in parallel, edits land one at a time as ordered, single undo steps."""

import threading
import time

import xoai


def ids_on_page(c):
    return {e["id"] for e in c.call("page_elements", detail="bbox", limit=5000)["elements"]}


def history(c):
    return [h.get("description") or h.get("text") or str(h) for h in c.call("history")["undo"]]


def test_1_draft_commit_is_one_undo_step(app):
    c = app.client()
    t = c.call("transaction_begin", label="add a sun", region=[50, 50, 120, 120])
    c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 30},
                                    {"type": "line", "from": [100, 30], "to": [100, 10]}],
           layer=t["draft_layer"], animate=False)
    before = ids_on_page(c)
    r = c.call("transaction_commit", transaction=t["transaction"], ops=[{"op": "draw", "animate": False}])
    assert r["committed"] and len(r["created"]) == 2 and "AI: add a sun" in r["undo"]
    assert set(r["created"]) <= ids_on_page(c)  # (ids are kept from the draft)
    c.call("undo")
    assert not (set(r["created"]) & ids_on_page(c))  # both gone with ONE undo
    assert c.call("transaction_list")["transactions"] == []


def test_2_ordered_stylus_replace(app):
    c = app.client()
    old = c.call("create_strokes", strokes=[{"points": [[300, 300], [310, 320], [330, 305], [350, 330]]}],
                 animate=False)["created"][0]["id"]
    t = c.call("transaction_begin", label="improve strokes", base_ids=[old], region=[280, 280, 100, 80])
    c.call("create_strokes", strokes=[{"points": [[300, 305], [325, 318], [350, 310]]}], layer=t["draft_layer"],
           animate=False)
    t0 = time.time()
    r = c.call("transaction_commit", transaction=t["transaction"],
               ops=[{"op": "draw", "animate": True, "speed": 2}, {"op": "delete", "ids": [old]}])
    assert r["committed"] and old not in ids_on_page(c) and r["created"][0] in ids_on_page(c)
    c.call("undo")  # one step brings the old one back and removes the new one
    page = ids_on_page(c)
    assert old in page and r["created"][0] not in page


def test_3_conflicts_protect_newer_work(app):
    c = app.client()
    f = c.call("create_strokes", strokes=[{"points": [[100, 500], [160, 500]]}], animate=False)["created"][0]["id"]
    t = c.call("transaction_begin", label="formula → LaTeX", base_ids=[f], region=[80, 470, 120, 60])
    c.call("create_shapes", shapes=[{"type": "rectangle", "x": 100, "y": 490, "w": 60, "h": 20}],
           layer=t["draft_layer"], animate=False)
    c.call("elements_edit", op="restyle", element_ids=[f], color="#ff0000")  # changed meanwhile
    err = c.call_error("transaction_commit", transaction=t["transaction"], ops=[{"op": "delete", "ids": [f]}])
    assert "Conflict" in err and f in err and "commit again" in err
    # after re-reading, the same transaction commits (the draft was kept)
    r = c.call("transaction_commit", transaction=t["transaction"], ops=[{"op": "delete", "ids": [f]}])
    assert r["committed"] and f not in ids_on_page(c)


def test_4_the_user_drawing_over_it_is_a_conflict(app):
    c = app.client()
    cx, cy = app.canvas_center(c)
    # a big stroke: the user's lines across the page certainly cross its area
    old = c.call("create_strokes", strokes=[{"points": [[80, 120], [500, 760]]}], animate=False)["created"][0]["id"]
    t = c.call("transaction_begin", label="tidy", base_ids=[old])
    # the user draws across the whole visible page: certainly over the old stroke's area
    w = [x for x in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if x["type"] == "GtkXournal"][0]
    x, y, ww, hh = w["bbox"]
    for yy in range(y + 20, y + hh - 20, 25):
        app.user_drag([(x + 10, yy), (x + ww // 2, yy), (x + ww - 10, yy)])
    err = c.call_error("transaction_commit", transaction=t["transaction"], ops=[{"op": "delete", "ids": [old]}])
    assert "Conflict" in err
    c.call("transaction_abort", transaction=t["transaction"])


def test_5_claims_limits_abort_and_stop(app):
    c = app.client()
    a = c.call("transaction_begin", label="a", page=1, region=[0, 0, 100, 100])
    assert "claimed" in c.call_error("transaction_begin", label="b", region=[50, 50, 100, 100])
    others = [c.call("transaction_begin", label=f"x{i}", region=[200 + i * 60, 0, 50, 50]) for i in range(4)]
    assert "At most 5" in c.call_error("transaction_begin", label="sixth", region=[500, 700, 10, 10])
    c.call("transaction_abort", transaction=a["transaction"], reason="changed my mind")
    assert "aborted: changed my mind" in c.call_error("transaction_commit", transaction=a["transaction"], ops=[])
    c.call("action_run", action="win.ai-stop")  # Stop aborts every open transaction
    assert c.call("transaction_list")["transactions"] == []
    assert "aborted" in c.call_error("transaction_commit", transaction=others[0]["transaction"], ops=[])


def test_6_parallel_commits_play_one_after_the_other(app):
    c = app.client()
    time.sleep(1.2)  # (the Stop of the previous test makes playbacks finish instantly for a second)
    ts = []
    for i in range(3):
        t = c.call("transaction_begin", label=f"stroke {i}", region=[40 + i * 150, 600, 120, 120])
        pts = [[60 + i * 150 + k * 2, 640 + (k % 7) * 3] for k in range(200)]
        c.call("create_strokes", strokes=[{"points": pts}], layer=t["draft_layer"], animate=False)
        ts.append(t["transaction"])
    results = {}

    def go(tid):
        results[tid] = (app.client().call("transaction_commit", transaction=tid,
                                          ops=[{"op": "draw", "animate": True, "speed": 1}]), time.time())

    threads = [threading.Thread(target=go, args=(tid,)) for tid in ts]
    for th in threads:
        th.start()
    for th in threads:
        th.join(timeout=60)
    assert all(results[t][0]["committed"] for t in ts)
    ends = sorted(results[t][1] for t in ts)
    assert ends[2] - ends[0] > 0.5  # played in turn, not at once
