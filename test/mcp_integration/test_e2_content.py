"""E2: text, links, images and LaTeX."""

import base64
import os
import shutil
import tempfile

import xoai

APP_CONFIG = {"default_layer": "AI"}  # these scenarios exercise the separate AI layer


def elements(c, types):
    return c.call("page_elements", detail="bbox", types=types, limit=500)["elements"]


def test_text_and_link(app):
    c = app.client()
    r = c.call("create_text", texts=[{"text": "Hello\nfrom the agent", "x": 100, "y": 100,
                                      "font": {"name": "Serif", "size": 18}, "color": "#aa0000"},
                                     {"text": "second", "x": 100, "y": 200}], animate=False)
    assert len(r["created"]) == 2 and r["layer_name"] == "AI"
    texts = {e["id"]: e for e in elements(c, ["text"])}
    t = texts[r["created"][0]["id"]]
    assert t["text"] == "Hello\nfrom the agent" and t["font"]["size"] == 18 and t["color"] == "#aa0000"
    assert abs(t["bbox"][0] - 100) < 3 and abs(t["bbox"][1] - 100) < 3
    link = c.call("create_link", links=[{"text": "docs", "url": "https://xournalpp.github.io", "x": 100, "y": 300}],
                  animate=False)
    got = [e for e in elements(c, ["link"]) if e["id"] == link["created"][0]["id"]][0]
    assert got["url"] == "https://xournalpp.github.io"
    assert "required" in c.call_error("create_text", texts=[{"text": "no position"}]) or \
        "position" in c.call_error("create_text", texts=[{"text": "no position"}])


def test_image_from_data_and_file(app):
    c = app.client()
    png = xoai.make_png(40, 20, (0, 128, 255))
    r = c.call("create_image", data=base64.b64encode(png).decode(), x=50, y=400, width=200, animate=False)
    x, y, w, h = r["created"][0]["bbox"]
    assert abs(w - 200) < 1 and abs(h - 100) < 1  # aspect ratio kept
    path = os.path.join(tempfile.mkdtemp(), "img.png")
    with open(path, "wb") as f:
        f.write(png)
    r2 = c.call("create_image", path=path, x=300, y=400, animate=False)
    assert abs(r2["created"][0]["bbox"][2] - 40) < 1
    assert "image format" in c.call_error("create_image", data=base64.b64encode(b"nope").decode(), x=0, y=0)


def test_latex(app):
    c = app.client()
    r = c.call_raw("create_latex", latex="\\int_0^1 x^2\\,dx = \\frac{1}{3}", x=100, y=550, height=30,
                   animate=False, timeout=120)
    if r["isError"]:
        text = r["content"][0]["text"]
        assert shutil.which("pdflatex") is None, text  # with TeX installed (even minimal) it must work
        assert "LaTeX" in text, text  # a missing TeX installation must be reported, not crash
        return
    out = r["structuredContent"]["created"][0]
    assert out["type"] == "latex" and abs(out["bbox"][3] - 30) < 1
    lat = [e for e in elements(c, ["latex"]) if e["id"] == out["id"]][0]
    assert "frac" in lat["latex"]
    bad = c.call_raw("create_latex", latex="\\frac{1}{", x=100, y=650, animate=False, timeout=120)
    assert bad["isError"] and "LaTeX" in bad["content"][0]["text"]
