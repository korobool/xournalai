"""E2: creating strokes and shapes with stylus-like pressure."""

import math

import xoai

APP_CONFIG = {"default_layer": "AI"}  # these scenarios exercise the separate AI layer


def strokes_of(c, ids):
    els = {e["id"]: e for e in c.call("page_elements", detail="full", limit=5000)["elements"]}
    return [els[i] for i in ids]


def test_ai_layer_and_user_layer_untouched(app):
    c = app.client()
    before = c.call("doc_info")["pages"][0]
    r = c.call("create_strokes", strokes=[{"points": [[50, 50], [150, 60]]}], animate=False)
    assert r["layer_name"] == "AI" and r["operation"].startswith("op")
    after = c.call("doc_info")["pages"][0]
    assert after["current_layer"] == before["current_layer"]
    assert [l["name"] for l in after["layers"]][-1] == "AI"
    again = c.call("create_strokes", strokes=[{"points": [[50, 70], [150, 80]]}], animate=False)
    assert not again["layer_created"] and again["layer"] == r["layer"]


def test_explicit_pressure_maps_like_hardware(app):
    c = app.client()
    r = c.call("create_strokes", strokes=[{"points": [[50, 100], [100, 100], [150, 100]],
                                           "pressure": [0.2, 0.5, 1.0]}], width=2, animate=False)
    s = strokes_of(c, [r["created"][0]["id"]])[0]
    ws = [p[2] for p in s["points"]]
    assert s["pressure"] and abs(ws[1] - 1.0) < 1e-6 and abs(ws[2] - 2.0) < 1e-6  # p * width (multiplier 1)
    r = c.call("create_strokes", strokes=[{"points": [[50, 120], [150, 120]], "widths": [0.5, 3]}], animate=False)
    s = strokes_of(c, [r["created"][0]["id"]])[0]
    assert [p[2] for p in s["points"]] == [0.5, 3]


def test_profiles_vary_width(app):
    c = app.client()
    pts = [[50 + i, 150] for i in range(0, 300, 10)]
    ids = {}
    for prof in ("ink", "brush", "constant", "none"):
        ids[prof] = c.call("create_strokes", strokes=[{"points": pts}], profile=prof, width=4,
                           animate=False)["created"][0]["id"]
    got = {k: strokes_of(c, [v])[0] for k, v in ids.items()}
    ink = [p[2] for p in got["ink"]["points"]]
    assert min(ink) < 0.6 * max(ink)  # tapered
    assert len(ink) > 250  # resampled to stylus density
    brush = [p[2] for p in got["brush"]["points"]]
    assert brush[len(brush) // 2] > 2 * brush[3]
    assert set(p[2] for p in got["constant"]["points"]) == {4}
    assert not got["none"]["pressure"]


def test_highlighter_never_has_pressure(app):
    c = app.client()
    r = c.call("create_strokes", strokes=[{"points": [[50, 200], [300, 200]], "pressure": [0.1, 1]}],
               tool="highlighter", color="yellow", animate=False)
    s = strokes_of(c, [r["created"][0]["id"]])[0]
    assert s["tool"] == "highlighter" and not s["pressure"]


def test_shapes(app):
    c = app.client()
    r = c.call("create_shapes", shapes=[
        {"type": "rectangle", "x": 50, "y": 300, "w": 100, "h": 60, "fill_opacity": 0.3},
        {"type": "circle", "center": [250, 330], "r": 30},
        {"type": "arrow", "from": [150, 330], "to": [220, 330]},
        {"type": "coordinate_system", "x": 50, "y": 500, "x_length": 100, "y_length": 80}], animate=False)
    assert len(r["created"]) == 5  # the coordinate system has two strokes
    rect = strokes_of(c, [r["created"][0]["id"]])[0]
    x, y, w, h = rect["bbox"]
    assert abs(x - 50) < 2 and abs(w - 100) < 3 and rect["fill_opacity"] > 0.25 and not rect["pressure"]
    hand = c.call("create_shapes", shapes=[{"type": "ellipse", "center": [400, 330], "rx": 40, "ry": 20}],
                  hand_drawn=True, animate=False)
    assert strokes_of(c, [hand["created"][0]["id"]])[0]["pressure"]


def test_layers_and_errors(app):
    c = app.client()
    cur = c.call("create_strokes", strokes=[{"points": [[10, 10], [20, 20]]}], layer="current", animate=False)
    assert cur["layer"] == 1
    named = c.call("create_strokes", strokes=[{"points": [[10, 10], [20, 20]]}], layer="Sketch", animate=False)
    assert named["layer_name"] == "Sketch" and named["layer_created"]
    assert "does not exist" in c.call_error("create_strokes", strokes=[{"points": [[0, 0], [1, 1]]}], layer="#9")
    assert "at least 2" in c.call_error("create_strokes", strokes=[{"points": [[0, 0]]}])
    assert "Unknown color" in c.call_error("create_strokes", strokes=[{"points": [[0, 0], [1, 1]]}],
                                           color="blurple")
    assert "profile" in c.call_error("create_strokes", strokes=[{"points": [[0, 0], [1, 1]]}], profile="crayon")
    assert "one value per point" in c.call_error("create_strokes",
                                                 strokes=[{"points": [[0, 0], [1, 1]], "pressure": [1]}])
    assert "strokes[1]" in c.call_error("create_strokes", strokes=[{"points": [[0, 0], [1, 1]]}, {"nope": 1}])
    assert "radius" in c.call_error("create_shapes", shapes=[{"type": "circle", "center": [5, 5], "r": -3}]) or \
        "positive" in c.call_error("create_shapes", shapes=[{"type": "circle", "center": [5, 5], "r": -3}])


def test_animation_completes(app):
    c = app.client()
    pts = [[100 + 2 * i, 600 + 20 * math.sin(i / 5)] for i in range(150)]
    r = c.call("create_strokes", strokes=[{"points": pts}], animate=True, speed=4)
    s = strokes_of(c, [r["created"][0]["id"]])[0]
    assert s["point_count"] > 250  # fully grown
