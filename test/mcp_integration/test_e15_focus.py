"""E15: the keyboard stays where the user types. Saves and pages becoming current (the user's scrolling, an agent's
page work) took it from the AI terminal and the AI input fields (2026-10-20: a space held for dictation then scrolled
the canvas a screen per repeat). The user's own strokes on the canvas still take it.

Keys are typed as the user (xdotool, on the app's Xvfb) and the test looks where they arrived."""

import os
import tempfile
import time

from test_e10_stt import MODEL, SAMPLE

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_FAKE_MIC": str(SAMPLE),
           "XOURNALAI_STT_MODELS": str(MODEL.parent)}
# (speech on: Command… then opens the AI input field over the canvas, as the AI lasso does)
APP_CONFIG = {"assistant": {"speech": True, "speech_model": MODEL.name[len("ggml-"):-len(".bin")]}}
DIR = tempfile.mkdtemp(prefix="xoai-focus-")
KEY_NAMES = {" ": "space", "/": "slash", "-": "minus", ".": "period", "_": "underscore"}


def widgets(c, name):
    return [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == name]


def wait(pred, timeout=8):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def current_page(c):
    return c.call("view")["current_page"]


def terminal_has_keys(app, tag):
    """Types `touch <file>` + Return as the user: whether the shell in the terminal got it"""
    f = os.path.join(DIR, tag)
    for ch in f"touch {f}":
        app.user_key(KEY_NAMES.get(ch, ch))
    app.user_key("Return")
    return wait(lambda: os.path.exists(f), 3)


def entry_text(c):
    return widgets(c, "ask-text")[0].get("value") or ""


def setup_document(c):
    if c.call("file_info").get("untitled", True):
        for _ in range(4):
            c.call("page_manage", op="insert")
        c.call("file_save_as", path=os.path.join(DIR, "doc.xopp"))
    c.call("page_manage", op="goto", page=1)
    time.sleep(0.3)


def open_terminal(app, c):
    if not widgets(c, "aiTerminalCloseTab"):
        c.call("action_run", action="win.ai-terminal-open", parameter="shell")
        assert wait(lambda: widgets(c, "aiTerminalCloseTab"))
        time.sleep(2)  # the shell starts
    else:
        c.call("ui_interact", op="focus", target=[w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]
                                                  if w["type"] == "VteTerminal"][0]["id"])
    assert terminal_has_keys(app, "ready-%d" % time.monotonic_ns())


def test_1_a_save_leaves_the_keyboard_in_the_terminal(app):
    c = app.client()
    setup_document(c)
    open_terminal(app, c)
    c.call("create_strokes", strokes=[{"points": [[100, 100], [200, 200]]}], animate=False)
    c.call("file_save")
    time.sleep(0.5)
    assert terminal_has_keys(app, "after-save")


def test_2_scrolling_to_another_page_leaves_it_in_the_terminal(app):
    c = app.client()
    setup_document(c)
    open_terminal(app, c)
    x, y = app.canvas_center(c)
    app._xdo("mousemove", x, y)
    for _ in range(80):  # the wheel over the page, no click: until the next page is the current one
        app._xdo("click", "5")
        if current_page(c) > 1:
            break
    assert current_page(c) > 1
    assert terminal_has_keys(app, "after-scroll")


def test_3_the_ai_input_field_keeps_it_through_saves_and_page_work(app):
    c = app.client()
    setup_document(c)
    c.call("ui_interact", op="click", target=widgets(c, "aiAct-command")[0]["id"])
    assert wait(lambda: widgets(c, "ask-text") and widgets(c, "ask-text")[0].get("visible", True))
    time.sleep(0.3)
    app.user_key("a")
    assert wait(lambda: entry_text(c) == "a"), entry_text(c)
    steps = [("a save", lambda: c.call("file_save")),
             ("a page inserted", lambda: c.call("page_manage", op="insert", page=5)),
             ("a background changed", lambda: c.call("page_manage", op="background", page=3, background="graph"))]
    typed = "a"
    for what, step in steps:
        c.call("create_strokes", page=2, strokes=[{"points": [[100, 100], [200, 210]]}], animate=False)
        step()
        time.sleep(0.5)
        app.user_key("b")
        typed += "b"
        # (all of it: a focus given back by GTK would select the text, and the key would replace it)
        assert wait(lambda: entry_text(c) == typed), (what, entry_text(c))
    c.call("ui_interact", op="click", target=widgets(c, "ask-close")[0]["id"])


def test_4_a_stroke_on_the_canvas_still_takes_the_keyboard(app):
    c = app.client()
    setup_document(c)
    open_terminal(app, c)
    x, y = app.canvas_center(c)
    app.user_drag([(x - 60 + i * 6, y + (i % 5)) for i in range(20)])
    time.sleep(0.3)
    assert not terminal_has_keys(app, "after-stroke")


def saved_strokes(path):
    """The strokes in a saved .xopp, as lists of numbers"""
    import gzip
    import re
    xml = gzip.open(path).read().decode()
    return [[float(v) for v in m.split()] for m in re.findall(r"<stroke[^>]*>([^<]*)</stroke>", xml)]


def test_5_an_agents_save_keeps_the_users_selection_and_writes_it(app):
    c = app.client()
    setup_document(c)
    sid = c.call("create_strokes", page=1, strokes=[{"points": [[321, 321], [333, 345]]}],
                 animate=False)["created"][0]["id"]
    c.call("elements_select", element_ids=[sid])  # (as if the user had selected it: it is out of its layer now)
    assert c.call("view").get("selection")
    c.call("file_save")
    assert c.call("view").get("selection"), "the save dropped the user's selection"
    path = c.call("file_info")["path"]
    assert any(s[:2] == [321, 321] for s in saved_strokes(path)), "the selected stroke is missing from the file"
    assert not c.call("file_info")["modified"]


def test_6_the_canvas_stays_usable_while_an_agent_saves(app):
    import random
    import threading
    import xoai
    c = app.client()
    heavy = os.path.join(DIR, "heavy.xopp")
    rnd = random.Random(7)
    xoai.make_xopp(heavy, [[xoai.stroke([(30 + i * 9 + rnd.uniform(-1, 1), 30 + k * 0.85) for i in range(60)])
                            for k in range(900)] for _ in range(12)])
    c.call("file_open", path=heavy, on_unsaved="save")
    c.call("create_strokes", page=2, strokes=[{"points": [[10, 10], [20, 20]]}], animate=False)
    page = c.call("view")["current_page"]
    before = len(c.call("page_elements", page=page, detail="bbox", limit=5000)["elements"])
    agent = app.client()
    th = threading.Thread(target=lambda: agent.call("file_save"))
    th.start()
    seen = []
    while th.is_alive() and len(seen) < 4:
        canvas = [w for w in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if w["type"] == "GtkXournal"]
        seen.append(canvas[0].get("enabled", True))
        time.sleep(0.05)
    x, y = app.canvas_center(c)
    app.user_drag([(x - 50 + i * 5, y + (i % 4)) for i in range(20)], step_delay=0.005)  # the user draws meanwhile
    th.join()
    assert len(seen) >= 3, seen  # (the save took long enough to look)
    assert all(seen), seen  # a blocked window makes the canvas insensitive
    assert len(c.call("page_elements", page=page, detail="bbox", limit=5000)["elements"]) == before + 1
    # (the stroke came after the save began: it is not in the file yet)
    assert c.call("file_info")["modified"]


def test_7_a_stroke_while_a_key_is_held_in_the_terminal_leaves_the_keyboard_there(app):
    c = app.client()
    if (c.call("file_info").get("path") or "").endswith("heavy.xopp"):
        c.call("file_open", path=os.path.join(DIR, "doc.xopp"), on_unsaved="save")
    setup_document(c)
    open_terminal(app, c)
    page = c.call("view")["current_page"]
    before = len(c.call("page_elements", page=page, detail="bbox", limit=5000)["elements"])
    x, y = app.canvas_center(c)
    app._xdo("keydown", "space")  # push-to-talk: held while drawing
    try:
        app.user_drag([(x - 40 + i * 8, y + (i % 3)) for i in range(10)], step_delay=0.005)
    finally:
        app._xdo("keyup", "space")
    assert len(c.call("page_elements", page=page, detail="bbox", limit=5000)["elements"]) == before + 1
    app.user_key("BackSpace")  # (the spaces the shell got)
    assert terminal_has_keys(app, "after-held-stroke")
    time.sleep(1)  # nothing held any more: a stroke takes the keyboard, as always
    app.user_drag([(x - 40 + i * 8, y + 20 + (i % 3)) for i in range(10)])
    assert not terminal_has_keys(app, "after-plain-stroke")
