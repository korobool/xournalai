"""E2: the pen engine drives the real stylus pipeline."""

import math


def by_id(c, ident):
    return [e for e in c.call("page_elements", detail="full", limit=5000)["elements"] if e["id"] == ident][0]


def test_pen_stroke_has_stylus_pressure_and_restores_tool(app):
    c = app.client()
    before = c.call("app_status")["user_tool"]
    wave = [[80 + i * 3, 150 + 30 * math.sin(i / 7)] for i in range(120)]
    r = c.call("pen_draw", strokes=[{"points": wave}], color="#aa3300", size="thick", speed=5)
    assert len(r["created"]) == 1 and r["erased"] == 0 and r["layer_name"] == "AI"
    s = by_id(c, r["created"][0]["id"])
    ws = [p[2] for p in s["points"]]
    assert s["pressure"] and min(ws) < 0.7 * max(ws) and s["color"] == "#aa3300"
    assert c.call("app_status")["user_tool"] == before


def test_explicit_pressure_is_hardware_mapped(app):
    c = app.client()
    r = c.call("pen_draw", strokes=[{"points": [[50, 700], [150, 700], [250, 700]], "pressure": [0.3, 1.0, 0.3]}],
               speed=0)
    s = by_id(c, r["created"][0]["id"])
    ws = [p[2] for p in s["points"]]
    assert max(ws) > 2 * min(ws)


def test_eraser_and_selection(app):
    c = app.client()
    target = c.call("create_shapes", shapes=[{"type": "line", "from": [300, 300], "to": [400, 300]}],
                    layer="current", animate=False)["created"][0]["id"]
    e = c.call("pen_draw", tool="eraser", strokes=[{"points": [[350, 280], [350, 320]]}], speed=0)
    assert e["erased"] >= 1
    assert target not in {x["id"] for x in c.call("page_elements", detail="bbox", limit=5000)["elements"]}
    c.call("create_shapes", shapes=[{"type": "circle", "center": [400, 500], "r": 20}], layer="current",
           animate=False)
    s = c.call("pen_draw", tool="select_lasso",
               strokes=[{"points": [[350, 450], [450, 450], [450, 550], [350, 550], [350, 450]]}], speed=0)
    assert s["selected"] == 1 and "selection" in s["note"]


def test_shape_tools_and_recognizer(app):
    c = app.client()
    before = c.call("app_status")["user_tool"]
    for tool in ("shape_rect", "shape_ellipse", "shape_arrow", "shape_line", "shape_axes"):
        r = c.call("pen_draw", tool=tool, strokes=[{"points": [[100, 350], [200, 420]]}], speed=0)
        assert len(r["created"]) >= 1, tool
    circle = [[300 + 60 * math.cos(a / 20 * math.pi), 600 + 60 * math.sin(a / 20 * math.pi)] for a in range(41)]
    r = c.call("pen_draw", tool="recognizer", strokes=[{"points": circle}], speed=0)
    assert len(r["created"]) == 1
    assert c.call("app_status")["user_tool"] == before  # whatever the user had is restored


def test_errors(app):
    c = app.client()
    assert "Unknown pen tool" in c.call_error("pen_draw", tool="brush", strokes=[{"points": [[0, 0], [1, 1]]}])
    assert "at least 2" in c.call_error("pen_draw", strokes=[{"points": [[0, 0]]}])
    assert "one value per point" in c.call_error("pen_draw", strokes=[{"points": [[0, 0], [1, 1]], "pressure": [1]}])
