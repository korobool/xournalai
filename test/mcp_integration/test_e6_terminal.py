"""E6: the AI terminal dock runs programs inside the app and hides without stopping them."""

import os
import tempfile
import time


def dock_visible(c):
    ws = [w for w in c.call("ui_inspect", max_depth=60, all=True)["widgets"] if w.get("name") == "aiTerminalDock"]
    return bool(ws) and ws[0].get("visible", True)


def wait_file(path, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        if os.path.exists(path) and open(path).read().strip():
            return open(path).read().strip()
        time.sleep(0.1)
    return None


def test_terminal_dock_runs_and_hides(app):
    c = app.client()
    assert not dock_visible(c)  # collapsed until used
    menu = {e["path"] for e in c.call("ui_menu_tree")["entries"]}
    assert "AI Agent/Show or hide AI terminal" in menu and "AI Agent/New Claude Code tab" in menu

    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    time.sleep(1.5)
    assert dock_visible(c)
    out = os.path.join(tempfile.mkdtemp(prefix="xoai-term-"), "out.txt")
    # a real shell runs in the tab: type a command like the user would
    for ch in f"echo $((6*7)) > {out}":
        app.user_key({" ": "space", "$": "dollar", "(": "parenleft", ")": "parenright", "*": "asterisk",
                      ">": "greater", "/": "slash", "-": "minus", ".": "period", "_": "underscore"}.get(ch, ch))
    app.user_key("Return")
    assert wait_file(out) == "42"

    app.user_key("ctrl+grave")  # hides from inside the terminal
    time.sleep(0.3)
    assert not dock_visible(c)
    c.call("action_run", action="win.ai-terminal")  # and shows again; the shell kept running
    time.sleep(0.3)
    assert dock_visible(c)
    for ch in "exit":
        app.user_key(ch)
    app.user_key("Return")
    time.sleep(0.5)
    c.call("action_run", action="win.ai-terminal")
    assert not dock_visible(c)
