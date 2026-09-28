"""E2: importing svg, images, xjson (lossless round trip), xopp pages and pdf pages."""

import base64
import os
import tempfile

import xoai
from xoai import glyphs, make_xopp, stroke, wobbly
from test_e1_pdf import make_pdf

DIR = tempfile.mkdtemp(prefix="xoai-import-")


def test_xjson_roundtrip_is_lossless(app):
    c = app.client()
    c.call("create_strokes", strokes=[{"points": [[50, 50], [120, 80], [200, 60]], "pressure": [0.2, 0.9, 0.4]}],
           color="#123456", width=3, animate=False)
    c.call("create_text", texts=[{"text": "round trip", "x": 60, "y": 100, "font": {"name": "Serif", "size": 15}}],
           animate=False)
    first = c.call("export", format="xjson", pages="1", delivery="inline")
    r = c.call("import", kind="xjson", data=first, new_page=True, animate=False)
    assert len(r["created"]) == len(first["elements"])
    second = c.call("export", format="xjson", pages=str(r["page"]), delivery="inline")

    def norm(doc):
        return sorted([{k: v for k, v in e.items() if k != "id"} for e in doc["elements"]], key=str)

    assert norm(first) == norm(second)


def test_xjson_file_with_offset(app):
    c = app.client()
    path = os.path.join(DIR, "e.xjson")
    c.call("export", format="xjson", pages="1", path=path)
    r = c.call("import", kind="xjson", path=path, dx=0, dy=300, page=1, animate=False)
    assert all(x["bbox"][1] > 300 for x in r["created"])


def test_svg_and_image(app):
    c = app.client()
    svg = '<svg xmlns="http://www.w3.org/2000/svg"><circle cx="20" cy="20" r="15" stroke="black" fill="none"/></svg>'
    r = c.call("import", kind="svg", data=svg, target=[300, 500, 100, 100], animate=False)
    assert r["strokes"] == 1
    png = base64.b64encode(xoai.make_png(10, 10)).decode()
    i = c.call("import", kind="image", data=png, target=[50, 600, 80, 80], animate=False)
    assert abs(i["created"][0]["bbox"][2] - 80) < 1


def test_xopp_and_pdf_pages(app):
    c = app.client()
    before = c.call("doc_info")["page_count"]
    other = make_xopp(os.path.join(DIR, "other.xopp"), [glyphs(50, 50, 5), glyphs(50, 50, 7), glyphs(50, 50, 3)])
    r = c.call("import", kind="xopp", path=other, pages="2-3")
    assert len(r["inserted_pages"]) == 2 and r["page_count"] == before + 2
    els = c.call("page_elements", page=r["inserted_pages"][0], detail="bbox", limit=100)["elements"]
    assert len(els) == 7
    pdf = os.path.join(DIR, "doc.pdf")
    make_pdf(pdf, ["Imported PDF page"])
    p = c.call("import", kind="pdf", path=pdf, after_page=1, dpi=72)
    assert p["inserted_pages"] == [2]
    imgs = c.call("page_elements", page=2, types=["image"], detail="bbox")["elements"]
    assert len(imgs) == 1 and imgs[0]["bbox"][2] > 590
    assert "Could not" in c.call_error("import", kind="pdf", data=base64.b64encode(b"not a pdf").decode())
