"""E4: the in-app AI controls: status strip, pause switch, AI layer actions, menu."""

import time


def status_text(c):
    widgets = c.call("ui_inspect", max_depth=40, all=True)["widgets"]
    return [w for w in widgets if w.get("name") == "mcpStatus"][0]["label"]


def layers(c):
    return c.call("doc_info")["pages"][0]["layers"]


def test_status_strip_and_menu(app):
    c = app.client()
    assert status_text(c).startswith("AI: ")
    paths = {e["path"]: e for e in c.call("ui_menu_tree")["entries"]}
    assert paths["AI Agent/Pause AI agent"]["action"] == "win.mcp-paused"
    assert "checked" in paths["AI Agent/Pause AI agent"]
    for p in ("Accept AI layer (merge down)", "Show or hide AI layer", "Clear AI layer"):
        assert "AI Agent/" + p in paths


def test_pause_blocks_agent_until_resumed(app):
    c = app.client()
    assert "user only" in c.call_error("ui_menu_select", path="AI Agent/Pause AI agent")
    assert "user only" in c.call_error("action_run", action="win.mcp-paused", state="true")
    assert "user only" in c.call_error("ui_keys", op="shortcut", keys="<Ctrl><Alt>Escape")
    app.user_key("ctrl+alt+Escape")  # only the user pauses
    time.sleep(0.3)
    assert c.call("app_status")["mcp"]["paused_by_user"] is True
    assert "paused" in c.call_error("create_shapes", shapes=[{"type": "rect", "x": 10, "y": 10, "w": 5, "h": 5}])
    assert "paused" in c.call_error("doc_info")
    app.user_key("ctrl+alt+Escape")  # the user resumes with the shortcut
    time.sleep(0.3)
    assert c.call("app_status")["mcp"]["paused_by_user"] is False
    assert "paused" not in status_text(c)
    c.call("doc_info")


def test_ai_layer_accept_toggle_clear(app):
    c = app.client()
    before = len(layers(c))
    c.call("create_shapes", shapes=[{"type": "circle", "center": [200, 200], "r": 20}], animate=False)
    ls = layers(c)
    ai = [l for l in ls if l["name"] == "AI"][0]
    assert len(ls) == before + 1 and ai["elements"] == 1
    c.call("ui_menu_select", path="AI Agent/Show or hide AI layer")
    assert [l for l in layers(c) if l["name"] == "AI"][0]["visible"] is False
    c.call("ui_menu_select", path="AI Agent/Show or hide AI layer")
    assert [l for l in layers(c) if l["name"] == "AI"][0]["visible"] is True
    c.call("ui_menu_select", path="AI Agent/Clear AI layer")
    ls = layers(c)
    assert len(ls) == before and not any(l["name"] == "AI" for l in ls)
    c.call("action_run", action="undo")
    assert any(l["name"] == "AI" for l in layers(c))
    c.call("ui_menu_select", path="AI Agent/Accept AI layer (merge down)")
    ls = layers(c)
    assert len(ls) == before and sum(l["elements"] for l in ls) >= 1


def test_pause_stops_the_pen_mid_stroke(app):
    import math
    import threading

    c = app.client()
    before = c.call("app_status")["user_tool"]
    out = {}

    def draw():
        wave = [[60 + i * 2, 400 + 40 * math.sin(i / 9)] for i in range(240)]
        out["r"] = app.client().call_raw("pen_draw", strokes=[{"points": wave}] * 3, speed=0.3)

    th = threading.Thread(target=draw)
    th.start()
    time.sleep(1.0)
    app.user_key("ctrl+alt+Escape")  # pause
    th.join(timeout=20)
    assert not th.is_alive()
    r = out["r"]
    assert r.get("isError") and "paused" in r["content"][0]["text"]
    assert c.call("app_status")["user_tool"] == before  # the pen gave the user's tool back
    app.user_key("ctrl+alt+Escape")  # resume
    time.sleep(0.3)
    assert c.call("app_status")["mcp"]["paused_by_user"] is False


def test_settings_dialog_is_user_only_and_applies(app):
    import json

    c = app.client()
    assert "user only" in c.call_error("ui_menu_select", path="AI Agent/AI Agent Settings…")
    assert "user only" in c.call_error("action_run", action="mcp-settings")
    # The user opens it from the menu (Alt+A, then S)
    app.user_key("alt+a")
    time.sleep(0.4)
    app.user_key("s")
    time.sleep(0.8)
    wins = c.call("ui_windows")["windows"]
    dlg = [w for w in wins if w.get("title") == "AI Agent Settings"]
    assert dlg, wins
    assert "user only" in c.call_error("ui_inspect", target=dlg[0]["id"])
    assert "user only" in c.call_error("ui_interact", target=dlg[0]["id"], op="close")
    # The user turns off animation and saves
    cfg_before = json.loads(app.config_file.read_text())
    assert cfg_before["animate"] is True
    app.user_key("alt+a", window="AI Agent Settings")  # "_Animate agent drawing" mnemonic toggles it
    time.sleep(0.3)
    app.user_key("alt+s", window="AI Agent Settings")  # "_Save"
    time.sleep(1.0)
    cfg = json.loads(app.config_file.read_text())
    assert cfg["animate"] is False and cfg["token"] == cfg_before["token"]
    assert cfg["permissions"] == cfg_before["permissions"]
    # The server restarted with the same port and token: a new session works
    c2 = app.client()
    assert c2.call("app_status")["app"] == "xournalai"


def test_settings_restart_with_a_call_in_flight(app):
    import threading

    c = app.client()
    out = {}

    def waiter():
        try:
            out["r"] = app.client().call_raw("wait_for_user", idle_ms=300, timeout_s=4)
        except Exception as e:  # the connection may be cut by the restart: that is fine
            out["error"] = str(e)

    th = threading.Thread(target=waiter)
    th.start()
    time.sleep(0.5)
    app.user_key("alt+a")
    time.sleep(0.4)
    app.user_key("s")
    time.sleep(0.8)
    app.user_key("alt+s", window="AI Agent Settings")  # save unchanged: the server restarts
    th.join(timeout=15)
    assert not th.is_alive()
    time.sleep(4.5)  # the interrupted wait_for_user times out and answers into the closed connection
    c2 = app.client()
    assert c2.call("app_status")["app"] == "xournalai"
    assert c2.call("wait_for_user", idle_ms=100, timeout_s=1)["timed_out"]
