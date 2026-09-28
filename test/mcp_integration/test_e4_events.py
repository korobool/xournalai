"""E4: change tracking with user/agent attribution."""

import time


def test_agent_changes_are_tracked(app):
    c = app.client()
    start = c.call("changes_get")["cursor"]
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 30}], animate=False)
    ev = c.call("changes_get", since=start)
    added = [e for e in ev["events"] if e["type"] == "element_added"]
    assert added and added[-1]["origin"] == "agent" and r["created"][0]["id"] in added[-1]["ids"]
    c.call("elements_edit", op="move", element_ids=[r["created"][0]["id"]], dx=20, dy=0)
    c.call("undo")
    ev2 = c.call("changes_get", since=ev["cursor"])
    assert {e["type"] for e in ev2["events"]} >= {"element_changed"}
    c.call("page_manage", op="insert", page=1)
    assert any(e["type"] == "page_inserted" for e in c.call("changes_get", since=ev2["cursor"])["events"])
    assert ev2["user_drawing"] is False


def test_user_drawing_is_attributed_to_user(app):
    c = app.client()
    c.call("page_manage", op="goto", page=1)
    cursor = c.call("changes_get")["cursor"]
    cx, cy = app.canvas_center(c)
    app.user_drag([(cx - 60 + i * 6, cy + (i % 5) * 3) for i in range(20)])
    time.sleep(0.5)
    ev = c.call("changes_get", since=cursor, origin="user")["events"]
    assert any(e["type"] == "element_added" for e in ev), ev
