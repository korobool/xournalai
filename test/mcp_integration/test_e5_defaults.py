"""E5: out of the box, agent drawings go into the user's current layer (editable with the eraser right away)."""


def test_default_layer_is_current(app):
    c = app.client()
    assert c.call("app_status")["mcp"]["default_layer"] == "current"
    before = c.call("doc_info")["pages"][0]["layers"]
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [200, 200], "r": 20}], animate=False)
    after = c.call("doc_info")["pages"][0]["layers"]
    assert len(after) == len(before) and r["layer_name"] != "AI"
    p = c.call("pen_draw", strokes=[{"points": [[50, 400], [250, 400]]}], speed=0)
    assert p["layer"] == r["layer"]
    assert c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 5}], animate=False,
                  layer="AI")["layer_name"] == "AI"  # still available on request
