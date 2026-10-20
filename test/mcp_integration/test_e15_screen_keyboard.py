"""E15: GNOME's on-screen keyboard only when called. In tablet mode GNOME Shell 42 opens its keyboard whenever a text
field tells IBus it has the keyboard, and the window slid up and down with every focus change (2026-10-20). In
manual mode the app's text fields use GTK's own input method, so GNOME never hears of them."""

import json
import os
import tempfile
import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "GTK_IM_MODULE": "ibus"}  # (the owner's session)
DIR = tempfile.mkdtemp(prefix="xoai-osk-")
KEY_NAMES = {" ": "space", "/": "slash", "-": "minus", ".": "period", "_": "underscore", "$": "dollar",
             ">": "greater"}


def wait(pred, timeout=8):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def status(c):
    return c.call("app_status")["screen_keyboard"]


def type_line(app, text):
    for ch in text:
        app.user_key(KEY_NAMES.get(ch, ch))
    app.user_key("Return")


def test_1_manual_by_default(app):
    c = app.client()
    s = status(c)
    assert s["manual"] is True, s
    assert s["input_method"] == "gtk-im-context-simple", s
    assert s["env_GTK_IM_MODULE"] is None, s  # (it would override the app's choice)
    assert s["session_input_method"] == "ibus", s


def test_2_typing_works_and_the_terminal_keeps_the_sessions_input_method(app):
    c = app.client()
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    time.sleep(2.5)  # the shell starts
    out = os.path.join(DIR, "im.txt")
    type_line(app, f"echo $GTK_IM_MODULE > {out}")  # (typed through GTK's own input method)
    assert wait(lambda: os.path.exists(out) and open(out).read().strip() == "ibus"), \
        open(out).read() if os.path.exists(out) else "nothing typed"


def test_3_the_switch_takes_effect_at_once_and_is_remembered(app):
    c = app.client()
    saved = lambda: json.load(open(app.config_file))["assistant"].get("screen_keyboard")
    c.call("action_run", action="win.ai-screen-keyboard-manual", state=False)
    assert wait(lambda: status(c)["input_method"] == "ibus"), status(c)
    assert status(c)["manual"] is False and saved() == "auto"
    c.call("action_run", action="win.ai-screen-keyboard-manual", state=True)
    assert wait(lambda: status(c)["input_method"] == "gtk-im-context-simple"), status(c)
    assert saved() == "manual"
