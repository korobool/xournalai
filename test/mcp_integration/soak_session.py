"""Soak (not part of the suite; run.py --file test/mcp_integration/soak_session.py): the mix of the owner's session
of 2026-10-17 that ended in heap corruption, for SOAK_SECONDS (default 360), to run against an AddressSanitizer build
(XOAI_BINARY=build-asan/install/bin/xournalpp). The floating see-through terminal with scrolling output, automatic
colours flipping between light and dark pages, opacity / dock / resize, the AI highlighter, zoom and scroll over
pages full of text (the render thread draws text while the UI thread draws the window), thinking zones."""

import os
import random
import time

APP_TERMINAL_FLOATING = True
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
SECONDS = float(os.environ.get("SOAK_SECONDS", "360"))

WORDS = ("multi-agent workflow benchmark attack prompt injection byzantine colluding agents impersonation "
         "tool outputs funding streams deep research related work").split()


def widgets(c, name):
    return [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == name]


def stroke(c, x1, y, x2, n=10):
    c.call("test_pen", op="hover", x=x1, y=y)
    c.call("test_pen", op="tip_down", x=x1, y=y)
    for i in range(1, n + 1):
        c.call("test_pen", op="move", x=x1 + (x2 - x1) * i / n, y=y + (i % 3) * 4)
    c.call("test_pen", op="tip_up", x=x2, y=y)


def test_1_the_session_mix(app):
    rnd = random.Random(17)
    c = app.client()
    # Pages full of text, some of them dark (automatic colours flip)
    for p in range(1, 7):
        if p > 1:
            c.call("page_manage", op="insert")
        texts = [{"text": " ".join(rnd.choice(WORDS) for _ in range(8)), "x": 40 + (k % 2) * 260, "y": 40 + k * 22}
                 for k in range(30)]
        c.call("create_text", page=p, texts=texts)
        c.call("create_shapes", page=p, shapes=[{"type": "circle", "center": [300, 600], "r": 60}], animate=False)
        if p % 2 == 0:
            c.call("page_manage", op="background", page=p, color="#202020")
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    time.sleep(1)
    # Output that keeps the terminal redrawing
    term = widgets(c, "aiTerminalDock")[0]["bbox"]
    app._xdo("mousemove", term[0] + term[2] // 2, term[1] + term[3] - 30)
    app._xdo("click", "1")
    app._xdo("type", "--delay", "5", "while true; do date; ls -la /usr/lib | head -40; sleep 0.05; done\n")
    end = time.time() + SECONDS
    i = 0
    while time.time() < end:
        i += 1
        page = 1 + i % 6
        c.call("view", op="scroll", page=page)
        c.call("view", op="zoom", factor=rnd.choice([0.6, 0.9, 1.2, 1.7, 2.3]))
        if i % 3 == 0:
            c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalOpacity")[0]["id"],
                   value=rnd.choice([30, 50, 70, 90]))
        if i % 7 == 0:
            c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalAutoColors")[0]["id"],
                   value=bool(i % 2))
        if i % 11 == 0:
            c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=True)
            time.sleep(0.3)
            c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=False)
        if i % 5 == 0:
            x, y, w, h = widgets(c, "aiTerminalHandle")[0]["bbox"]
            step = rnd.choice([-8, 8])
            app.user_drag([(x + w // 2, y + h // 2 + k * step) for k in range(6)], step_delay=0.02)
        if i % 4 == 0:  # the AI highlighter: marks, then discard (Escape) or let it count down
            c.call("action_run", action="win.ai-highlighter", state=True)
            stroke(c, 200, 300 + (i % 5) * 20, 360)
            time.sleep(0.4)
            app.user_key("Escape")
        if i % 6 == 0:
            z = c.call("thinking", op="start", page=page, region=[60, 60, 300, 120], text="soak " + str(i))["id"]
            c.call("thinking", op="update", id=z, text="still soaking " + str(i))
            c.call("thinking", op="done", id=z)
        if i % 9 == 0:
            c.call("create_text", page=page, texts=[{"text": "added " + str(i), "x": 60, "y": 700}])
        time.sleep(0.15)
    assert c.call("app_status") is not None  # (still alive)
