"""E12: drawing by agents flashes a "drawn here" hint popover; it is reused and closed, never destroyed while shown
(a popover destroyed mid-show left GtkWindow positioning a widget that was gone: a crash on 2026-10-08). Many
drawings in a row, then window resizes (full layouts), must not crash, and no popover is destroyed meanwhile."""

import pathlib
import tempfile
import time

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-pop-")) / "stalls.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TRACE_STALLS": str(TRACE)}


def test_1_many_drawings_then_resizes_do_not_crash(app):
    c = app.client()
    deadline = time.time() + 10
    while time.time() < deadline and not TRACE.exists():
        time.sleep(0.1)
    for i in range(40):
        c.call("create_shapes", page=1, shapes=[{"type": "line", "from": [50 + i * 5, 100], "to": [60 + i * 5, 300]}],
               animate=False)
        time.sleep(0.05)
    for w, h in ((900, 700), (1200, 800), (800, 600), (1100, 750)):
        app._xdo("search", "--sync", "--limit", "1", "--pid", app.proc.pid, "--onlyvisible", "--class", "xournal",
                 "windowsize", w, h)
        time.sleep(0.4)
    time.sleep(1.5)
    assert c.call("app_status")["version"]  # still alive
    log = TRACE.read_text()
    assert "popover destroyed" not in log, [line for line in log.splitlines() if "popover" in line][:5]
