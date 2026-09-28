"""E2: finding room and drawing on new pages."""

import xoai
from xoai import glyphs, make_xopp
import tempfile, os

PATH = make_xopp(os.path.join(tempfile.mkdtemp(), "busy.xopp"), [glyphs(40, 40, 30) + glyphs(40, 60, 30)])
APP_ARGS = [PATH]


def test_free_space_avoids_notes(app):
    c = app.client()
    r = c.call("find_free_space", width=200, height=80)
    assert r["found"]
    x, y, w, h = r["region"]
    notes = c.call("page_elements", detail="bbox", limit=500)["elements"]
    for e in notes:  # no overlap with any note
        ex, ey, ew, eh = e["bbox"]
        assert x >= ex + ew or x + w <= ex or y >= ey + eh or y + h <= ey, (r["region"], e["bbox"])
    first = notes[0]["id"]
    r2 = c.call("find_free_space", width=60, height=20, near_element=first, side="below")
    assert r2["found"] and r2["region"][1] > notes[0]["bbox"][1]
    none = c.call("find_free_space", width=2000, height=20)
    assert not none["found"] and "new_page" in none["suggestion"]


def test_draw_on_new_page(app):
    c = app.client()
    before = c.call("doc_info")["page_count"]
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 50}], new_page=True,
               animate=False)
    info = c.call("doc_info")
    assert info["page_count"] == before + 1 and r["page"] == before + 1
