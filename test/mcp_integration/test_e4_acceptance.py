"""E4 milestone: co-creation. The user draws, the agent notices, answers in the user's own hand on its layer, talks
on the canvas, and the user stays in charge (pause, accept)."""

import threading
import time

import xoai


def test_co_creation_session(app):
    c = app.client()
    cx, cy = app.canvas_center(c)
    cursor = c.call("changes_get")["cursor"]

    # 1. The agent waits; the user sketches a line and pauses
    got = {}
    th = threading.Thread(target=lambda: got.update(r=app.client().call_raw("wait_for_user", idle_ms=500,
                                                                             timeout_s=20)))
    th.start()
    time.sleep(0.4)
    app.user_drag([(cx - 120 + i * 6, cy - 60 + (i % 5) * 2) for i in range(40)])
    th.join(timeout=30)
    seen = got["r"]["structuredContent"]
    assert not seen["timed_out"] and len(xoai.Mcp.images(got["r"])) == 1
    user_ids = [i for e in seen["events"] if e["origin"] == "user" for i in e["ids"]]
    assert user_ids

    # 2. The agent tries to match the user's hand (a mouse has no pressure: it falls back to an ink profile) and
    # answers on its own layer, with the pen
    style = c.call_raw("user_style")
    profile = "match_user" if not style.get("isError") else "ink"
    assert profile == "ink" and "stylus" in style["content"][0]["text"]
    x, y0, w, h = seen["area"]
    y = y0 + h + 30
    r = c.call("pen_draw", strokes=[{"points": [[x, y], [x + w, y]]}],
               profile=profile, speed=0)
    assert r["layer_name"] == "AI" and len(r["created"]) == 1
    m = c.call("show_message", text="Underlined it for you", x=x, y=y + 10, seconds=1)
    assert m["shown"]

    # 3. The change log tells both parties apart
    log = c.call("changes_get", since=cursor)["events"]
    assert {e["origin"] for e in log if e["type"] == "element_added"} == {"user", "agent"}

    # 4. The user pauses the agent; nothing gets through until they resume
    app.user_key("ctrl+alt+Escape")
    time.sleep(0.3)
    assert "paused" in c.call_error("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 5}])
    app.user_key("ctrl+alt+Escape")
    time.sleep(0.3)

    # 5. The user accepts the agent's layer: its strokes become part of the page, the user's stay untouched
    before = c.call("doc_info")["pages"][0]["layers"]
    assert any(layer["name"] == "AI" for layer in before)
    c.call("ui_menu_select", path="AI Agent/Accept AI layer (merge down)", visible=False)
    after = c.call("doc_info")["pages"][0]["layers"]
    assert not any(layer["name"] == "AI" for layer in after)
    ids = {e["id"] for e in c.call("page_elements", detail="bbox", limit=500)["elements"]}
    assert set(user_ids) <= ids
