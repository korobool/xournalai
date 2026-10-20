"""E15: the app does not flood the X server. On 2026-10-20 at 15:23 the window froze while asking the X server for a
new range of resource ids (XC-MISC): an AI zone's animation redrew the whole window 60 times a second (~40 000 X
requests a second, a new pixmap per icon and shadow piece), which used up the app's two million ids within minutes.
Counted with app_status.ui.x_requests_by_kind (the requests the app sends, by kind)."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def rate(c, secs=4):
    """X requests a second and frames a second (CopyArea: GTK puts a frame on the window)"""
    totals = lambda: c.call("app_status")["ui"]["x_requests_by_kind"]
    a, t = totals(), time.time()
    time.sleep(secs)
    b, dt = totals(), time.time() - t
    count = lambda kinds: sum(v[0] for v in kinds.values())
    frames = (b.get("CopyArea", [0, 0])[0] - a.get("CopyArea", [0, 0])[0]) / dt
    return (count(b) - count(a)) / dt, frames


def test_1_the_request_log_counts_by_kind(app):
    c = app.client()
    deadline = time.time() + 10
    while time.time() < deadline and not c.call("app_status")["ui"]["x_requests_by_kind"]:
        time.sleep(0.2)  # (the log starts once the app is up)
    kinds = c.call("app_status")["ui"]["x_requests_by_kind"]
    assert kinds and all(len(v) == 2 and v[0] > 0 for v in kinds.values()), kinds
    assert "CopyArea" in kinds  # (GTK puts every frame on the window with one)


def test_2_an_ai_zone_costs_the_x_server_little(app):
    c = app.client()
    time.sleep(3)  # (the first renders)
    idle, _ = rate(c)
    t = c.call("transaction_begin", label="draw something", page=1, region=[50, 50, 300, 200], base_ids=[])
    try:
        time.sleep(0.5)
        working, frames = rate(c)
    finally:
        c.call("transaction_abort", transaction=t["transaction"])
    # (before: ~41 000 requests and 59 frames a second; now 3 500-9 000 and 18-20, the zone's animation)
    assert working < 15000, (working, idle)
    assert frames <= 30, frames
    assert idle < 4000, idle
