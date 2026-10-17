"""E12: an X error during drawing no longer ends the app (2026-10-18 00:00: GDK's handler ended it on an error answering
a RENDER request, from GTK's drawing). RENDER's errors and BadAlloc are logged with their request and dropped; GDK still
gets the others."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def test_1_a_render_error_is_logged_and_survived(app):
    c = app.client()
    deadline = time.time() + 10
    while time.time() < deadline and not c.call("app_status")["ui"]["x_requests_logged"]:
        time.sleep(0.2)  # (the X hooks are set once the app is idle after starting)
    before = c.call("app_status")["ui"]["x_errors_survived"]
    c.call("test_x_error")
    time.sleep(1)  # the error comes back with the next events
    c.call("page_manage", op="insert")  # (the app still works)
    st = c.call("app_status")
    assert st["ui"]["x_errors_survived"] == before + 1, st["ui"]
    log = app.read_log()
    assert "X error" in log and "RENDER" in log and "ignored" in log, log[-1500:]
    assert "X Window System error" not in log, log[-1500:]
