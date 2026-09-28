"""E3: tool, view and clipboard control."""


def test_tool_get_set(app):
    c = app.client()
    t = c.call("tool_set", tool="highlighter", color="#00ff00", size="thick")
    assert t["tool"] == "highlighter" and t["color"].startswith("#00ff00") and t["size"] == "thick"
    t = c.call("tool_set", tool="pen", drawing_type="rectangle", line_style="dash")
    assert t["drawing_type"] == "rectangle" and t["line_style"] == "dash"
    t = c.call("tool_set", drawing_type="default", line_style="plain")
    e = c.call("tool_set", tool="eraser", eraser_type="deleteStroke")
    assert e["eraser_type"] == "deleteStroke"
    assert c.call("tool_get")["tool"] == "eraser"
    c.call("tool_set", tool="pen")
    assert "Unknown tool" in c.call_error("tool_set", tool="lightsaber") or "must be one of" in \
        c.call_error("tool_set", tool="lightsaber")


def test_view(app):
    c = app.client()
    c.call("page_manage", op="insert", page=1)
    v = c.call("view", op="zoom", factor=2)
    assert abs(v["zoom"] - 2) < 0.2, v
    v = c.call("view", op="scroll", page=2)
    assert v["current_page"] == 2
    assert "visible_region" in v
    v = c.call("view", op="sidebar", on=False)
    assert v["sidebar"] is False
    v = c.call("view", op="sidebar", on=True)
    v = c.call("view", op="zoom_100")
    assert c.call("view")["current_page"] >= 1


def test_clipboard(app):
    c = app.client()
    c.call("clipboard", op="set_text", text="hello from the agent")
    assert c.call("clipboard", op="get_text")["text"] == "hello from the agent"
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [200, 200], "r": 30}], animate=False,
               layer="current")
    c.call("clipboard", op="copy", element_ids=[r["created"][0]["id"]])
    p = c.call("clipboard", op="paste")
    assert len(p["pasted"]) == 1 and p["pasted"][0] != r["created"][0]["id"]
