"""E12: a crash whose handler hangs (2026-10-17: heap corruption left the allocator's lock held, the crash handler
waited for it forever, and the dead app kept its window, its MCP port and its helpers, so the next start's AI terminal
could not connect). Now the hang watchdog rescues the document and ends the app."""

import pathlib
import subprocess
import tempfile
import time

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-crash-")) / "stalls.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TRACE_STALLS": str(TRACE), "XOURNALAI_RESCUE_AFTER_MS": "1500"}


def _app_alive(app):
    out = subprocess.run(["pgrep", "-g", str(app.proc.pid), "-f", "bin/xournalpp"], capture_output=True,
                         text=True).stdout.split()
    return bool(out)


def test_1_a_stuck_crash_handler_rescues_and_ends_the_app(app):
    c = app.client()
    deadline = time.time() + 10
    while time.time() < deadline and not TRACE.exists():  # (the watchdog starts once the app is idle)
        time.sleep(0.1)
    c.call("page_manage", op="insert")  # (something to rescue)
    rescue = app.config_file.parent / "emergencysave.xopp"
    rescue.unlink(missing_ok=True)
    try:
        c.call("test_crash_stuck", timeout=3)
    except Exception:
        pass  # (no answer: it crashed)
    t0 = time.time()
    while time.time() - t0 < 20 and _app_alive(app):
        time.sleep(0.2)
    assert not _app_alive(app), "the crashed app should have ended"
    assert time.time() - t0 < 15, "ended by the watchdog (after the rescue), not only by the 25 s deadline"
    assert rescue.exists() and rescue.stat().st_size > 100
    log = app.read_log()
    assert "[Crash Handler] Crashed 1 time(s) with signal 6" in log, log[-2000:]
    assert "the crash handler is stuck: ending the app" in log, log[-2000:]
