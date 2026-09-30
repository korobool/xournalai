"""E5: limits and responsiveness on heavy documents."""

import os
import random
import tempfile
import time

import xoai

DIR = tempfile.mkdtemp(prefix="xoai-perf-")
PATH = os.path.join(DIR, "heavy.xopp")
_rnd = random.Random(5)
xoai.make_xopp(PATH, [[xoai.stroke([(40 + i * 1.2, 40 + k * 0.25 + _rnd.uniform(-1, 1)) for i in range(400)])
                       for k in range(2500)]] + [[xoai.stroke([(50, 50), (80, 80)])] for _ in range(5)])
APP_ARGS = [PATH]


def test_big_pages_are_paged_by_size(app):
    c = app.client()
    first = c.call("page_elements", page=1, detail="full", limit=5000)
    assert first["total"] == 2500 and first["returned"] < 2500 and first["next_offset"] == first["returned"]
    second = c.call("page_elements", page=1, detail="full", limit=5000, offset=first["next_offset"])
    assert second["offset"] == first["returned"] and second["returned"] > 0
    ids = {e["id"] for e in first["elements"]} | {e["id"] for e in second["elements"]}
    assert len(ids) == first["returned"] + second["returned"]
    assert c.call("page_elements", page=1, detail="bbox", limit=5000)["returned"] == 2500  # small: no size cap


def test_oversized_requests_are_refused(app):
    c = app.client()
    status, body = c._post({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                            "params": {"name": "app_status", "arguments": {"pad": "x" * (65 * 1024 * 1024)}}})
    assert status == 413 and "too large" in body["error"]["message"]
    assert c.call("app_status")["app"] == "xournalai"


def test_backups_do_not_block(app):
    c = app.client()
    c.call("page_manage", op="insert", page=2)
    t0 = time.time()
    r = c.call("page_manage", op="delete", page=1)  # 1M points: the safety copy is written in the background
    took = time.time() - t0
    assert took < 1.5, f"{took:.2f}s"
    assert xoai.wait_for_file(r["backup"], timeout=30)
    c.call("undo")


def test_rapid_page_operations_while_rendering(app):
    # Used to crash the render thread (layout read while pages were inserted/moved)
    c = app.client()
    before = c.call("app_status")["document"]["page_count"]
    for i in range(15):
        c.call("page_manage", op="insert", page=2)
        c.call("page_manage", op="move", page=2, to=4)
        c.call("page_manage", op="goto", page=1 + i % 5)
        c.call("page_manage", op="delete", page=4)
    assert c.call("app_status")["document"]["page_count"] == before
