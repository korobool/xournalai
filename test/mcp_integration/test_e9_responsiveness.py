"""E9: the UI never freezes while agents think, and hardly while they (or the user) draw.

A watchdog notices every time the UI thread does not respond for more than 50 ms (app_status.ui). Agents' reading
tools run off the UI thread; background rendering holds the document lock only to copy what it draws."""

import threading
import time

import xoai
from test_e5_performance import APP_ARGS  # a heavy page: 2500 strokes, 1M points  # noqa: F401

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
LIMIT_MS = 100  # the pen must never freeze longer than this


def ui(c):
    return c.call("app_status")["ui"]


def new_stalls(c, before):
    """Stalls since `before` (an app_status.ui), as [(ms, what)]."""
    now = ui(c)
    n = now["stalls"] - before["stalls"]
    return [(s["ms"], s["what"]) for s in now["recent_stalls"][-n:]] if n > 0 else []


def long_ones(stalls):
    return [s for s in stalls if s[0] >= LIMIT_MS]


def settle(c):
    time.sleep(3)  # the first renders of the heavy page
    c.call("page_render", page=1, dpi=10)


def test_1_the_watchdog_sees_a_blocked_ui(app):
    c = app.client()
    settle(c)
    before = ui(c)
    c.call("test_block_ui", ms=300)
    time.sleep(0.2)
    stalls = new_stalls(c, before)
    assert any(ms >= 250 and "tool test_block_ui" in what and "test stall" in what for ms, what in stalls), stalls


def test_2_agents_thinking_never_freeze_the_ui(app):
    c = app.client()
    settle(c)
    before = ui(c)
    c.call("page_render", page=1)
    c.call("page_render", page=1, dpi=200, max_px=3000)
    c.call("page_elements", page=1, detail="full", limit=5000)
    c.call("page_elements", page=1, detail="simplified", limit=5000)
    blocks = c.call("layout_analyze", page=1, include_element_ids=True)["blocks"]
    c.call("blocks_render", page=1, block_ids=[b["id"] for b in blocks[:3]])
    c.call("doc_info")
    c.call("changes_get")
    time.sleep(0.3)
    assert not long_ones(new_stalls(c, before)), new_stalls(c, before)


def test_3_the_pen_does_not_wait_for_agents_or_rendering(app):
    c = app.client()
    settle(c)
    cx, cy = app.canvas_center(c)
    agent = app.client()
    stop = threading.Event()

    def think():
        while not stop.is_set():
            agent.call("page_render", page=1)
            agent.call("page_elements", page=1, detail="full", limit=5000)

    before = ui(c)
    th = threading.Thread(target=think)
    th.start()
    try:
        for i in range(6):
            app.user_drag([(cx - 80 + k * 8, cy - 60 + 20 * i + (k % 3)) for k in range(20)], step_delay=0.01)
            time.sleep(0.2)
    finally:
        stop.set()
        th.join()
    time.sleep(0.3)
    assert not long_ones(new_stalls(c, before)), new_stalls(c, before)


def test_4_editing_does_not_wait_for_a_long_render(app):
    # a zoom change re-renders the heavy page in the background; an edit right after used to wait for all of it
    c = app.client()
    settle(c)
    c.call("view", op="zoom", factor=2.5)
    time.sleep(0.1)
    c.call("page_manage", op="insert", page=2)
    t0 = time.time()
    c.call("page_manage", op="delete", page=2)
    assert time.time() - t0 < 0.5, time.time() - t0
