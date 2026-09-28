"""E3: page and layer management."""

import os
import tempfile

from xoai import glyphs, make_xopp

PATH = make_xopp(os.path.join(tempfile.mkdtemp(), "three.xopp"),
                 [glyphs(50, 50, 1), glyphs(50, 50, 2), glyphs(50, 50, 3)])
APP_ARGS = [PATH]


def counts(c):
    return [sum(l["elements"] for l in p["layers"]) for p in c.call("doc_info")["pages"]]


def test_pages(app):
    c = app.client()
    assert counts(c) == [1, 2, 3]
    r = c.call("page_manage", op="insert", page=1, background="graph")
    assert r["page"] == 2 and r["background"]["type"] == "graph" and r["page_count"] == 4
    assert counts(c) == [1, 0, 2, 3]
    c.call("page_manage", op="move", page=4, to=1)
    assert counts(c) == [3, 1, 0, 2]
    c.call("page_manage", op="duplicate", page=1)
    assert counts(c) == [3, 3, 1, 0, 2]
    c.call("page_manage", op="delete", page=4)
    assert counts(c) == [3, 3, 1, 2]
    c.call("undo")
    assert counts(c) == [3, 3, 1, 0, 2]
    s = c.call("page_manage", op="size", page=2, width=400, height=300)
    assert s["width"] == 400 and s["height"] == 300
    b = c.call("page_manage", op="background", page=2, color="#ffffdd", background="dotted")
    assert b["background"]["color"] == "#ffffdd" and b["background"]["type"] == "dotted"
    g = c.call("page_manage", op="goto", page=3)
    assert g["current_page"] == 3


def test_layers(app):
    c = app.client()
    c.call("page_manage", op="goto", page=1)
    r = c.call("layer_manage", op="add", page=1, name="Notes")
    assert [l["name"] for l in r["layers"]] == ["Layer 1", "Notes"]
    r = c.call("layer_manage", op="rename", page=1, layer="Notes", name="Ideas")
    assert r["layers"][1]["name"] == "Ideas"
    r = c.call("layer_manage", op="hide", page=1, layer=1)
    assert not r["layers"][0]["visible"]
    r = c.call("layer_manage", op="show", page=1, layer=1)
    assert r["layers"][0]["visible"]
    r = c.call("layer_manage", op="copy", page=1, layer=1)
    assert len(r["layers"]) == 3
    r = c.call("layer_manage", op="move_up", page=1, layer=1)
    r = c.call("layer_manage", op="merge_down", page=1, layer=2)
    assert len(r["layers"]) == 2
    r = c.call("layer_manage", op="delete", page=1, layer="Ideas")
    assert len(r["layers"]) == 1
    assert "at least one layer" in c.call_error("layer_manage", op="delete", page=1, layer=1)
    assert "No layer named" in c.call_error("layer_manage", op="select", page=1, layer="Nope")
