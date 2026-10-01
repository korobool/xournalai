"""E12: "Connect an Agent…" shows ready-made commands and configs with the token; it is for the user only: agents
can neither open it nor read it."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def test_1_agents_cannot_open_it(app):
    c = app.client()
    err = c.call_error("action_run", action="win.mcp-connect")
    assert "user" in err.lower(), err


def test_2_agents_cannot_read_it_once_open(app):
    c = app.client()
    assert c.call("test_connect_dialog")["opened"]
    time.sleep(0.5)
    wins = c.call("ui_windows")["windows"]
    dialog = [w for w in wins if w.get("title") == "Connect an Agent"]
    assert dialog, wins
    err = c.call_error("ui_inspect", target=dialog[0]["id"])
    assert "for the user only" in err, err
