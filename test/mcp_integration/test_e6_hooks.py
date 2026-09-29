"""E6: the serving session's Claude Code hooks tell the app whether it is idle, busy or waiting."""

import json
import subprocess


def hook(app, event, payload):
    env = dict(app.app_env(), XOURNALAI_PORT=str(app.port))
    r = subprocess.run([app.binary, "--ai-hook", event], input=json.dumps(payload), env=env, text=True,
                       capture_output=True, timeout=10)
    assert r.returncode == 0


def state(c):
    return c.call("app_status")["mcp"]["serving"]


def test_hooks_drive_the_serving_state(app):
    c = app.client()
    assert state(c)["state"] == "not_running"
    hook(app, "SessionStart", {"session_id": "s-1", "source": "startup"})
    assert state(c) == {"state": "idle", "session_id": "s-1"}
    hook(app, "UserPromptSubmit", {"session_id": "s-1", "prompt": "[xournalai] 1 event"})
    assert state(c)["state"] == "busy"
    hook(app, "PreToolUse", {"session_id": "s-1", "tool_name": "mcp__xournalai__create_latex"})
    assert state(c)["tool"] == "create_latex"
    hook(app, "Notification", {"session_id": "s-1", "message": "Claude needs your permission to use Bash"})
    assert state(c)["state"] == "waiting"
    hook(app, "Stop", {"session_id": "s-1"})
    assert state(c)["state"] == "idle" and "tool" not in state(c)


def test_hook_needs_the_token_and_never_fails(app):
    import urllib.request
    req = urllib.request.Request(f"http://127.0.0.1:{app.port}/ai-hook", data=b'{"event":"Stop"}', method="POST")
    try:
        urllib.request.urlopen(req, timeout=5)
        assert False, "must need the token"
    except urllib.error.HTTPError as e:
        assert e.code == 401
    # a hook for a port nobody listens on still exits 0 quickly (it must never block Claude)
    r = subprocess.run([app.binary, "--ai-hook", "Stop"], input="{}", text=True, capture_output=True, timeout=10,
                       env=dict(app.app_env(), XOURNALAI_PORT="1"))
    assert r.returncode == 0


def test_companion_settings_have_the_hooks(app):
    d = app.home / "data" / "xournalai" / "companion" / ".claude" / "settings.local.json"
    hooks = json.loads(d.read_text())["hooks"]
    for ev in ("SessionStart", "UserPromptSubmit", "PreToolUse", "Stop", "Notification"):
        cmd = hooks[ev][0]["hooks"][0]["command"]
        assert "--ai-hook " + ev in cmd and "xournalpp" in cmd
