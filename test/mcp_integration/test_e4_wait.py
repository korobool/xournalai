"""E4: wait_for_user returns after the user draws and pauses."""

import threading
import time

import xoai


def test_wait_returns_after_user_pause(app):
    c = app.client()
    cx, cy = app.canvas_center(c)
    result = {}

    def waiter():
        result["r"] = app.client().call_raw("wait_for_user", idle_ms=600, timeout_s=20)

    th = threading.Thread(target=waiter)
    th.start()
    time.sleep(0.5)
    t0 = time.time()
    app.user_drag([(cx - 80 + i * 8, cy + (i % 4) * 4) for i in range(20)])
    th.join(timeout=30)
    r = result["r"]
    meta = r["structuredContent"]
    assert not meta["timed_out"]
    assert any(e["type"] == "element_added" and e["origin"] == "user" for e in meta["events"])
    assert "area" in meta and len(xoai.Mcp.images(r)) == 1
    assert time.time() - t0 >= 0.6  # returned only after the pause


def test_wait_ignores_agent_changes_and_times_out(app):
    c = app.client()
    result = {}

    def waiter():
        result["r"] = app.client().call("wait_for_user", idle_ms=300, timeout_s=2)

    th = threading.Thread(target=waiter)
    th.start()
    time.sleep(0.3)
    c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 10}], animate=False)
    th.join(timeout=10)
    assert result["r"]["timed_out"] and result["r"]["events"] == []


def test_since_cursor_and_region(app):
    c = app.client()
    cursor = c.call("changes_get")["cursor"]
    cx, cy = app.canvas_center(c)
    app.user_drag([(cx + i * 5, cy + 40) for i in range(15)])
    r = c.call("wait_for_user", since=cursor, idle_ms=300, timeout_s=10, render=False)
    assert not r["timed_out"] and r["cursor"] > cursor
    far = c.call("wait_for_user", since=cursor, idle_ms=200, timeout_s=1, region=[5000, 5000, 10, 10])
    assert far["timed_out"]
