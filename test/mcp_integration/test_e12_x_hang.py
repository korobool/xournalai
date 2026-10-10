"""E12: when the X server stops answering the app (as on 2026-10-05), the app rescues the document and exits
instead of staying frozen; the next start offers the rescue. Here a helper on the test's own Xvfb grabs the server,
so the app's next X round trip goes unanswered (the same wait as on the owner's laptop)."""

import pathlib
import subprocess
import sys
import tempfile
import textwrap
import time

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-xhang-")) / "stalls.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TRACE_STALLS": str(TRACE), "XOURNALAI_RESCUE_AFTER_MS": "1500"}

GRABBER = textwrap.dedent("""
    import ctypes, sys, time
    x = ctypes.cdll.LoadLibrary("libX11.so.6")
    x.XOpenDisplay.restype = ctypes.c_void_p
    d = x.XOpenDisplay(None)
    x.XGrabServer(ctypes.c_void_p(d)); x.XSync(ctypes.c_void_p(d), 0)
    print("grabbed", flush=True)
    time.sleep(float(sys.argv[1]))
""")


def test_1_an_unanswered_x_server_rescues_and_exits(app):
    c = app.client()
    deadline = time.time() + 10
    while time.time() < deadline and not TRACE.exists():  # (the watchdog starts once the app is idle)
        time.sleep(0.1)
    c.call("page_manage", op="insert")  # (something to rescue)
    rescue = app.config_file.parent / "emergencysave.xopp"
    rescue.unlink(missing_ok=True)
    env = app.app_env()  # the app's Xvfb, never the desktop
    grab = subprocess.Popen([sys.executable, "-c", GRABBER, "30"], env=env, stdout=subprocess.PIPE, text=True)
    try:
        assert grab.stdout.readline().strip() == "grabbed"
        try:
            c.call("ui_screenshot", timeout=8)  # needs an X round trip on the UI thread: it waits, like the owner's
        except Exception:
            pass
        deadline = time.time() + 20
        while time.time() < deadline and app.proc.poll() is None and app._group_alive() and not rescue.exists():
            time.sleep(0.2)
        deadline = time.time() + 20
        while time.time() < deadline and _app_alive(app):
            time.sleep(0.2)
    finally:
        grab.kill()
    assert rescue.exists() and rescue.stat().st_size > 100
    assert not _app_alive(app), "the app should have exited"
    log = TRACE.read_text()
    assert "running the hang handler (rescue)" in log, log[-2000:]
    assert "X: last request sent #" in log
    line = [x for x in log.splitlines() if "running the hang handler" in x][0]
    assert "unanswered: #" in line and "before: #" in line, line  # the requests themselves, by type
    unanswered = line.split("unanswered: ")[1].split(" | ")[0]
    assert any(name in unanswered for name in ("GetInputFocus", "GetImage", "GetProperty", "PutImage", "RENDER", "op")), unanswered
    assert "closing the display connection" in app.read_log()


def _app_alive(app):
    out = subprocess.run(["pgrep", "-g", str(app.proc.pid), "-f", "install/bin/xournalpp"], capture_output=True,
                         text=True).stdout.split()
    return bool(out)
