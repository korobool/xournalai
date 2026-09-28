"""E1: exporting content in all formats."""

import gzip
import json
import os
import tempfile

import xoai
from xoai import glyphs, make_xopp, stroke, wobbly

DIR = tempfile.mkdtemp(prefix="xoai-export-")
SOURCE = make_xopp(os.path.join(DIR, "notes.xopp"),
                   [glyphs(100, 100, 8) + [stroke(wobbly([(50, 300), (300, 320)]), color="#ff0000ff")],
                    glyphs(100, 200, 4)])
APP_ARGS = [SOURCE]


def test_pdf(app):
    c = app.client()
    r = c.call("export", format="pdf", path=os.path.join(DIR, "out.pdf"))
    f = r["files"][0]
    assert r["pages"] == [1, 2] and open(f, "rb").read(5) == b"%PDF-"
    one = c.call("export", format="pdf", pages="2", path=os.path.join(DIR, "p2.pdf"))
    assert one["pages"] == [2]
    assert "exists" in c.call_error("export", format="pdf", path=os.path.join(DIR, "out.pdf"))


def test_png_and_svg(app):
    c = app.client()
    r = c.call("export", format="png", pages="all", dpi=72, path=os.path.join(DIR, "img.png"))
    assert [os.path.basename(f) for f in r["files"]] == ["img-p1.png", "img-p2.png"]
    assert open(r["files"][0], "rb").read(4) == b"\x89PNG"
    svg = c.call("export", format="svg", pages="1", region=[40, 80, 300, 260], path=os.path.join(DIR, "cut.svg"))
    text = open(svg["files"][0]).read()
    assert "<svg" in text and 'width="300' in text
    inline = c.call_raw("export", format="png", region=[40, 80, 100, 50], delivery="inline")
    assert len(xoai.Mcp.images(inline)) == 1


def test_xopp_copy(app):
    c = app.client()
    r = c.call("export", format="xopp", path=os.path.join(DIR, "copy.xopp"))
    with gzip.open(r["files"][0], "rt") as f:
        assert f.read().count("<page") == 2
    assert c.call("file_info")["path"] == SOURCE  # the open document is unchanged
    assert "whole document" in c.call_error("export", format="xopp", pages="1")


def test_xjson_inline_and_file(app):
    c = app.client()
    doc = c.call("export", format="xjson", pages="1", delivery="inline")
    assert doc["format"] == "xournalai.xjson" and len(doc["elements"]) == 9
    red = [e for e in doc["elements"] if e["color"] == "#ff0000"]
    assert len(red) == 1 and len(red[0]["points"]) > 50
    ids = [e["id"] for e in c.call("page_elements", page=1, detail="bbox", limit=2)["elements"]]
    two = c.call("export", format="xjson", element_ids=ids, delivery="inline")
    assert len(two["elements"]) == 2
    f = c.call("export", format="xjson", pages="all", path=os.path.join(DIR, "all.xjson"))["files"][0]
    assert len(json.load(open(f))["elements"]) == 13


def test_latex_document_roundtrip_fields(app):
    c = app.client()
    c.call("file_open", path=str(xoai.ROOT / "test/files/load/latex-fileversion-5.xopp"))
    doc = c.call("export", format="xjson", pages="all", delivery="inline")
    latex = [e for e in doc["elements"] if e["type"] == "latex"]
    assert latex and latex[0]["latex"] and len(latex[0]["data"]) > 100 and "transform" in latex[0]
