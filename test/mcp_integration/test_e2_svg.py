"""E2: SVG to editable strokes (text-to-drawing channel)."""

import os

import xoai

SUNSET = open(os.path.join(os.path.dirname(__file__), "sunset.svg")).read()


def test_scene_fits_target_and_is_editable(app):
    c = app.client()
    r = c.call("create_from_svg", svg=SUNSET, target=[60, 80, 460, 345], profile="ink", tremor=0.2, animate=False)
    assert r["bounds"] == [60, 80, 460, 345] and r["strokes"] >= 14 and r["texts"] == 1
    ids = {x["id"] for x in r["created"]}
    els = [e for e in c.call("page_elements", detail="bbox", limit=500)["elements"] if e["id"] in ids]
    assert len(els) == len(ids)
    for e in els:
        x, y, w, h = e["bbox"]
        assert x >= 55 and y >= 75 and x + w <= 525 and y + h <= 430, e
    texts = [e for e in els if e["type"] == "text"]
    assert texts[0]["text"] == "Sunset over the mountains"
    full = c.call("page_elements", detail="simplified", types=["stroke"], limit=500)["elements"]
    assert any(e.get("fill_opacity") for e in full if e["id"] in ids)
    assert any(e["pressure"] for e in full if e["id"] in ids)  # ink profile on outlines


def test_placement_without_target_and_color_override(app):
    c = app.client()
    svg = '<svg xmlns="http://www.w3.org/2000/svg"><line x1="0" y1="0" x2="100" y2="0" stroke="red"/></svg>'
    r = c.call("create_from_svg", svg=svg, x=50, y=700, scale=2, color="#00aa00", animate=False)
    e = [x for x in c.call("page_elements", detail="bbox", limit=500)["elements"]
         if x["id"] == r["created"][0]["id"]][0]
    assert abs(e["bbox"][0] - 50) < 2 and abs(e["bbox"][2] - 200) < 3 and e["color"] == "#00aa00"


def test_gradient_warning_and_errors(app):
    c = app.client()
    svg = ('<svg xmlns="http://www.w3.org/2000/svg"><defs><linearGradient id="g"><stop offset="0" '
           'stop-color="#ff0000"/><stop offset="1" stop-color="#0000ff"/></linearGradient></defs>'
           '<rect x="0" y="0" width="50" height="50" fill="url(#g)"/></svg>')
    r = c.call("create_from_svg", svg=svg, x=400, y=600, animate=False)
    assert any("Gradient" in w for w in r["warnings"])
    assert "Not an SVG" in c.call_error("create_from_svg", svg="hello")
    assert "nothing drawable" in c.call_error("create_from_svg", svg='<svg xmlns="http://www.w3.org/2000/svg"/>')
    assert "Give" in c.call_error("create_from_svg")
