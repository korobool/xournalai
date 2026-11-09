"""E15: big keys to the right of the AI terminal, for the tablet without the on-screen keyboard (2026-11-09, the
owner's request): Space (held like the space bar: Claude Code's hold-to-talk), Enter, Backspace and Escape go to the
tab shown. The serving tab runs a program that logs the bytes it gets, raw."""

import pathlib
import tempfile
import time

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-keys-"))
LOG, LOGGER = DIR / "keys.bin", DIR / "logger.py"
LOGGER.write_text("""import os, sys, tty
tty.setraw(0)
out = open(sys.argv[1], "ab", buffering=0)
print("ready", flush=True)
while True:
    b = os.read(0, 64)
    if not b:
        break
    out.write(b)
""")
APP_CONFIG = {"assistant": {"autostart": True, "command": f"python3 -u {LOGGER} {LOG}"}}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def got(expect=None, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        b = LOG.read_bytes() if LOG.exists() else b""
        if expect is None or expect(b):
            return b
        time.sleep(0.1)
    return LOG.read_bytes() if LOG.exists() else b""


def shown(c):
    ws = widgets(c)
    if not ws.get("aiTerminalDock", {}).get("visible", True) or "aiTerminalDock" not in ws:
        c.call("action_run", action="win.ai-terminal")
        time.sleep(0.5)
        ws = widgets(c)
    return ws


def press(app, w, hold=0.0):
    x, y, width, height = w["bbox"]
    app._xdo("mousemove", x + width // 2, y + height // 2)
    app._xdo("mousedown", 1)
    time.sleep(hold)
    app._xdo("mouseup", 1)
    time.sleep(0.3)


def test_1_the_keys_are_right_of_the_terminal(app):
    c = app.client()
    time.sleep(1.5)  # (the logger starts)
    ws = shown(c)
    panel, dock = ws["aiTerminalKeys"], ws["aiTerminalDock"]
    assert panel.get("visible", True), panel
    px, _, pw, _ = panel["bbox"]
    dx, _, dw, _ = dock["bbox"]
    assert abs((px + pw) - (dx + dw)) < 12 and 0.10 * dw <= pw <= max(0.2 * dw, 120), (panel["bbox"], dock["bbox"])
    for k in ("Space", "Enter", "Backspace", "Escape"):
        assert ws["aiTerminalKey" + k].get("visible", True)


def test_2_taps_send_the_keys(app):
    c = app.client()
    ws = shown(c)
    start = len(got())
    for k in ("Space", "Backspace", "Escape", "Enter"):
        press(app, ws["aiTerminalKey" + k])
    assert got(lambda b: len(b) >= start + 4)[start:] == b" \x7f\x1b\r"


def test_3_a_held_space_repeats_like_the_space_bar_until_released(app):
    c = app.client()
    ws = shown(c)
    start = len(got())
    press(app, ws["aiTerminalKeySpace"], hold=1.5)
    b = got()[start:]
    assert set(b) == {ord(" ")} and len(b) >= 10, b  # (the first at once, then the keyboard's repeat)
    time.sleep(0.5)
    assert got()[start:] == b, "it went on after the release"


def test_4_an_agent_can_press_them_too(app):
    c = app.client()
    ws = shown(c)
    start = len(got())
    c.call("ui_interact", op="click", target=ws["aiTerminalKeyEnter"]["id"])
    assert got(lambda b: len(b) > start)[start:] == b"\r"


def test_5_they_can_be_hidden(app):
    c = app.client()
    ws = shown(c)
    c.call("ui_interact", op="click", target=ws["aiTerminalKeysToggle"]["id"])
    time.sleep(0.3)
    assert not widgets(c)["aiTerminalKeys"].get("visible", True)
    c.call("ui_interact", op="click", target=ws["aiTerminalKeysToggle"]["id"])
    time.sleep(0.3)
    assert widgets(c)["aiTerminalKeys"].get("visible", True)
