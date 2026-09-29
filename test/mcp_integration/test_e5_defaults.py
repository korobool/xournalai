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
    # the user's layer setting wins: an agent asking for another layer is told why it wasn't used
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 5}], animate=False, layer="AI")
    assert r["layer"] == p["layer"] and "settings" in r["layer_note"]
    pen = c.call("pen_draw", strokes=[{"points": [[50, 500], [250, 500]]}], speed=0, layer="AI")
    assert pen["layer"] == p["layer"]
    # the agent's own hidden draft layer still works
    d = c.call("draft", op="begin")
    drafted = c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 5}], animate=False,
                     layer=d["layer"])
    assert "layer_note" not in drafted
    c.call("draft", op="discard", draft=d["draft"])
