"""E5: agents stay connected across app restarts: a session id the server does not know is adopted."""

import json


def test_unknown_session_is_resumed(app):
    c = app.client()
    c.call("app_status")
    c.session = "0123456789abcdef0123456789abcdef"  # as if the app had restarted since this client initialized
    assert c.call("app_status")["app"] == "xournalai"
    assert c.session == "0123456789abcdef0123456789abcdef"
    assert "resumed session" in app.read_log()
    c.session = "x" * 200  # nonsense ids are still refused
    status, body = c._post({"jsonrpc": "2.0", "id": 9, "method": "tools/call",
                            "params": {"name": "app_status", "arguments": {}}})
    assert status == 404


def test_resumed_session_survives_settings_restart(app):
    import time
    c = app.client()
    c.call("app_status")
    app.user_key("alt+a")
    time.sleep(0.4)
    app.user_key("s")
    time.sleep(0.8)
    app.user_key("alt+s", window="AI Agent Settings")  # save: the server restarts, sessions are dropped
    time.sleep(0.8)
    assert c.call("doc_info")["page_count"] >= 1  # same client, same session id: just works
