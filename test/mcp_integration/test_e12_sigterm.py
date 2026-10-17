"""E12: SIGTERM (a logout, `kill`) closes the app cleanly: the document is saved for recovery and the app exits, without
the crash it had once the stall watchdog ran (2026-10-17 23:36: "terminate called without an active exception", the
watchdog's thread was still joinable when exit() ran the static destructors)."""

import os
import signal
import subprocess
import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def test_1_sigterm_closes_without_a_crash(app):
    c = app.client()
    c.call("page_manage", op="insert")  # (something to save)
    time.sleep(3)  # the stall watchdog starts once the app is idle
    pids = subprocess.run(["pgrep", "-g", str(app.proc.pid), "-x", "xournalpp"], capture_output=True,
                          text=True).stdout.split()
    assert len(pids) == 1, pids
    pid = int(pids[0])
    os.kill(pid, signal.SIGTERM)  # the app alone (its Xvfb stays: the signal itself must end it)
    deadline = time.time() + 15
    while time.time() < deadline and os.path.exists(f"/proc/{pid}"):
        time.sleep(0.1)
    time.sleep(0.5)
    log = app.read_log()
    assert "Force close requested with signal 15" in log, log[-2000:]
    assert "Successfully saved document" in log, log[-2000:]
    assert "terminate called" not in log and "crashed" not in log, log[-2000:]
