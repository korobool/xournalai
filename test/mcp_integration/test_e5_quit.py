"""E5: quitting normally tears everything down cleanly (the main window goes before the MCP server)."""

import time

APP_PERMISSIONS = {"read": True, "draw": True, "ui": True, "files": True, "destructive": True}


def test_quit_is_clean(app):
    c = app.client()
    c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 10}], animate=False)
    c.call("file_new", on_unsaved="discard")  # nothing unsaved: quitting asks nothing
    app.user_key("ctrl+alt+Escape")  # paused state and the settings/menu must not matter either
    try:
        c.call("app_status")
    except Exception:
        pass
    app.user_key("ctrl+alt+Escape")
    time.sleep(0.3)
    app.user_key("ctrl+q")
    import subprocess

    def running():
        return subprocess.run(["pgrep", "-g", str(app.proc.pid), "-x", "xournalpp"], capture_output=True).returncode == 0

    for _ in range(100):
        if not running():
            break
        time.sleep(0.1)
    log = app.read_log()
    assert "Crash Handler" not in log and "CRITICAL" not in log, log[-3000:]
    if running():
        print("    windows:", [w["title"] for w in c.call("ui_windows")["windows"]])
    assert not running(), "the app did not quit"
