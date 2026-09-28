"""E2 acceptance: text-to-drawing workflow and co-drawing with the pen, as an agent would do it."""

import os

import xoai

SUNSET = open(os.path.join(os.path.dirname(__file__), "sunset.svg")).read()


def test_text_to_drawing_workflow(app):
    c = app.client()
    room = c.call("find_free_space", width=460, height=345)
    assert room["found"]
    d = c.call("draft", op="begin")
    c.call("create_from_svg", svg=SUNSET, target=room["region"], profile="ink", tremor=0.25, layer=d["layer"],
           animate=False)
    x, y, w, h = room["region"]
    c.call("create_text", texts=[{"text": "Legend: sun, mountains, river", "x": x, "y": y + h + 6}],
           layer=d["layer"], animate=False)
    preview = c.call_raw("draft", op="render", draft=d["draft"], save=False)
    assert len(xoai.Mcp.images(preview)) == 1
    before = len(c.call("history", max=500)["undo"])
    done = c.call("draft", op="commit", draft=d["draft"], animate=True, speed=20)
    assert len(done["created"]) > 10 and done["layer_name"] == "AI"
    assert len(c.call("history", max=500)["undo"]) == before + 1
    strokes = [e for e in c.call("page_elements", detail="simplified", types=["stroke"], limit=5000)["elements"]]
    assert any(e["pressure"] for e in strokes)  # stylus-like variation


def test_pen_co_drawing(app):
    c = app.client()
    r = c.call("pen_draw", strokes=[{"points": [[60, 780], [300, 760], [520, 790]]}], speed=10)
    assert len(r["created"]) == 1
    e = c.call("pen_draw", tool="eraser", layer="AI", strokes=[{"points": [[300, 740], [300, 800]]}], speed=0)
    assert e["erased"] >= 1
