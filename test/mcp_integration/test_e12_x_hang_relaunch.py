"""E12: the restart after a frozen display works also when an update replaced the binary while the app ran
(2026-10-17 23:04: /proc/self/exe then ended in " (deleted)", the restart's exec failed, and the frozen app just
vanished). The app runs from its own link to the installed binary, which an "update" replaces; then a helper on the
test's Xvfb grabs the server, the app rescues and closes its display, and the restart must exec the updated file."""

import os
import pathlib
import subprocess
import sys
import tempfile
import time

import xoai
from test_e12_x_hang import GRABBER

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-relaunch-")) / "stalls.log"
ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TEST_RELAUNCH": "1", "XOURNALAI_TRACE_STALLS": str(TRACE),
       "XOURNALAI_RESCUE_AFTER_MS": "1500"}


def test_1_the_restart_survives_an_update(app):
    app.stop()  # this scenario runs the app from its own link to the binary
    installed = pathlib.Path(xoai.BINARY)
    # (a sibling of bin/: the resources are found as ../share)
    folder = installed.parent.with_name(f"bin-relaunch-test-{os.getpid()}")
    folder.mkdir(exist_ok=True)
    link = folder / "xournalpp"
    link.unlink(missing_ok=True)
    os.link(installed, link)
    own = xoai.App(binary=link, env=ENV)
    try:
        own.start()
        c = own.client()
        deadline = time.time() + 10
        while time.time() < deadline and not TRACE.exists():  # (the watchdog starts once the app is idle)
            time.sleep(0.1)
        c.call("page_manage", op="insert")  # (something to rescue)
        own.display  # (found by the binary's name, which is about to be "(deleted)")
        # The update: the name the app runs from is removed and comes back (as an install replaces the file), so the
        # running one is "(deleted)". (Renaming another link of the same file over it would do nothing.)
        link.unlink()
        os.link(installed, link)
        assert os.readlink(f"/proc/{_app_pid(own)}/exe").endswith(" (deleted)")
        grab = subprocess.Popen([sys.executable, "-c", GRABBER, "30"], env=own.app_env(), stdout=subprocess.PIPE,
                                text=True)
        try:
            assert grab.stdout.readline().strip() == "grabbed"
            try:
                c.call("ui_screenshot", timeout=8)  # an X round trip on the UI thread: it waits
            except Exception:
                pass
            deadline = time.time() + 25
            while time.time() < deadline and "Restarting xournalai" not in own.read_log():
                time.sleep(0.2)
            time.sleep(4)  # the restart waits 2 s, then execs the binary
        finally:
            grab.kill()
        log = own.read_log()
        assert f"Restarting xournalai in 2 s (the rescued document will be offered): {link}" in log, log[-2000:]
        assert "Could not restart" not in log and "not found" not in log, log[-2000:]
        # The restarted app ran (its Xvfb is gone with the old app, so it reports the missing display)
        assert "cannot open display" in log or "Cannot open display" in log, log[-2000:]
    finally:
        own.stop()
        link.unlink(missing_ok=True)
        folder.rmdir()


def _app_pid(app):
    out = subprocess.run(["pgrep", "-g", str(app.proc.pid), "-f", "bin-relaunch-test"], capture_output=True,
                         text=True).stdout.split()
    pids = [p for p in out if pathlib.Path(f"/proc/{p}/exe").exists() and "xvfb" not in
            pathlib.Path(f"/proc/{p}/cmdline").read_text()]
    assert pids, out
    return pids[0]
