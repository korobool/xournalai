"""E6: handwritten markers (*! **! *w! …) are recognised in the app, without a model, and delivered as intents."""

import os
import pathlib
import tempfile
import time

LOG = os.path.join(tempfile.mkdtemp(prefix="xoai-mk-"), "agent.log")
FAKE = pathlib.Path(__file__).with_name("fake_agent.py")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {FAKE}"}}


def got():
    return [l[4:] for l in open(LOG).read().splitlines() if l.startswith("GOT [xournalai]")] if os.path.exists(LOG) else []


def wait_for(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def star(app, x, y):
    for (x1, y1, x2, y2) in ((-6, 0, 6, 0), (-5, -5, 5, 5), (-5, 5, 5, -5)):
        app.user_drag([(x + x1, y + y1), (x + (x1 + x2) // 2, y + (y1 + y2) // 2), (x + x2, y + y2)])


def bang(app, x, y):
    app.user_drag([(x, y - 8), (x, y), (x, y + 6)])
    app.user_drag([(x, y + 11), (x + 1, y + 12)])


def test_markers(app):
    c = app.client()
    assert wait_for(lambda: os.path.exists(LOG))
    cx, cy = app.canvas_center(c)
    app.user_drag([(cx - 120 + i * 4, cy + (i % 5) * 3) for i in range(20)])  # some drawing
    star(app, cx - 20, cy)
    bang(app, cx - 4, cy)
    assert wait_for(lambda: any("handwritten marker *!" in g for g in got()))
    msg = [g for g in got() if "handwritten marker" in g][0]
    assert "page 1" in msg and "delete them after acting" in msg
    time.sleep(5.5)  # (the fake agent has no hooks: wake-ups are spaced)
    star(app, cx - 20, cy + 60)
    app.user_drag([(cx - 10, cy + 56), (cx - 8, cy + 63), (cx - 6, cy + 58), (cx - 4, cy + 63), (cx - 2, cy + 56)])  # w
    bang(app, cx + 6, cy + 60)
    assert wait_for(lambda: any("handwritten marker *?!" in g for g in got()))
    assert "read its letter" in [g for g in got() if "*?!" in g][0]
    # the same strokes are never reported twice; ordinary drawing isn't a marker
    time.sleep(5.5)
    n = len(got())
    app.user_drag([(cx + 60 + i * 5, cy + 120) for i in range(15)])
    time.sleep(3)
    assert len(got()) == n
