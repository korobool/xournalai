"""E15: worker sessions in tabs of the AI terminal (the owner's choice, 2026-11-09): the serving session starts a
Claude Code worker per heavy job with `workers`; it runs in <companion>/workers with only the safe list's tools
(dontAsk), reads its brief as its first message, is not taken for the serving session (its hooks are ignored), and is
closed with its tab. A fake `claude` on PATH records how it is called. A worker's thinking zone is its own: it lasts
through the serving session's turns until the worker ends it or is closed (1.16.4)."""

import json
import os
import pathlib
import re
import stat
import subprocess
import tempfile
import time

ROOT = pathlib.Path(tempfile.mkdtemp(prefix="xoai-workers-"))
BIN, LOG, HOME = ROOT / "bin", ROOT / "calls.jsonl", ROOT / "home"
BIN.mkdir()
HOME.mkdir()
FAKE = BIN / "claude"
FAKE.write_text(f"""#!/usr/bin/env python3
import json, os, sys, time
with open({str(LOG)!r}, "a") as f:
    f.write(json.dumps({{"args": sys.argv[1:], "cwd": os.getcwd(), "pid": os.getpid(),
                        "worker": os.environ.get("XOURNALAI_WORKER"),
                        "handoff_off": os.environ.get("CLAUDE_CODE_DISABLE_BG_EXIT_HANDOFF")}}) + "\\n")
print("fake worker", sys.argv[-1][:40], flush=True)
time.sleep(600)
""")
FAKE.chmod(FAKE.stat().st_mode | stat.S_IEXEC)
# HOME of its own: the tabs run through the login shell, and the owner's ~/.profile puts ~/.local/bin (the real
# claude) first on PATH
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "HOME": str(HOME), "PATH": f"{BIN}:{os.environ['PATH']}"}
APP_CONFIG = {"assistant": {"autostart": False, "max_parallel": 2}}


def calls():
    return [json.loads(l) for l in LOG.read_text().splitlines()] if LOG.exists() else []


def wait_calls(n, timeout=10):
    end = time.time() + timeout
    while time.time() < end and len(calls()) < n:
        time.sleep(0.2)
    return calls()


def alive(pid):
    return subprocess.run(["kill", "-0", str(pid)], capture_output=True).returncode == 0


def test_1_a_worker_runs_its_brief_in_its_own_tab_with_only_the_safe_tools(app):
    c = app.client()
    brief = 'Draw "T1" on page 2 in [24, 40, 180, 120].\nQuote: $(echo not run) `nor this`'
    r = c.call("workers", op="start", name="T1", brief=brief, effort="xhigh")
    assert r["tab"] == "⚙ T1" and r["effort"] == "xhigh" and r["cap"] == 2, r
    w = wait_calls(1)[0]
    companion = app.home / "data" / "xournalai" / "companion"
    assert w["cwd"] == str(companion / "workers") and w["worker"] == "T1" and w["handoff_off"] == "1", w
    a = w["args"]
    assert a[a.index("--permission-mode") + 1] == "dontAsk", a
    assert a[a.index("--allowedTools") + 1] == "mcp__xournalai,mcp__claude-in-chrome,WebSearch,WebFetch,Read," \
        "Bash(python3 *)", a
    assert "--chrome" in a, a  # (the research browser's extension: clicks, typing, tabs)
    assert a[a.index("--mcp-config") + 1] == str(companion / ".mcp.json") and "--strict-mcp-config" in a, a
    assert a[a.index("--effort") + 1] == "xhigh" and a[a.index("--name") + 1] == "live T1", a
    assert a[-1] == brief, a[-1]  # (the brief exactly: nothing in it is run by the shell)
    hooks_file = companion / "workers" / "hooks.json"
    assert a[a.index("--settings") + 1] == str(hooks_file), a  # (its own hooks: the app follows it)
    hooks = json.loads(hooks_file.read_text())["hooks"]
    assert sorted(hooks) == ["PostToolUse", "PreToolUse", "SessionEnd", "Stop", "UserPromptSubmit"], hooks
    assert hooks["Stop"][0]["hooks"][0]["command"].endswith("--ai-hook Stop")
    assert "A worker of xournalai's serving session" in (companion / "workers" / "CLAUDE.md").read_text()
    assert (companion / "workers" / "T1.brief.md").read_text() == brief
    tabs = [x.get("label") for x in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert "⚙ T1" in tabs


def test_2_the_cap_and_one_name_at_a_time(app):
    c = app.client()
    c.call("workers", op="start", name="T2", brief="second")
    assert "is still running" in c.call_error("workers", op="start", name="T1", brief="again")
    assert "2 workers are running" in c.call_error("workers", op="start", name="T3", brief="third")
    for bad, why in [({"name": "T 4", "brief": "x"}, "name"), ({"name": "T4", "brief": ""}, "brief"),
                     ({"name": "T4", "brief": "x", "effort": "huge"}, "effort")]:
        assert why in c.call_error("workers", op="start", **bad), bad


def test_3_a_worker_s_hooks_are_not_the_serving_session_s(app):
    """The worker runs Claude Code too, with hooks of its own: XOURNALAI_WORKER makes `xournalpp --ai-hook` report
    them as the worker's, never as the serving session's (busy, idle)."""
    c = app.client()
    before = c.call("app_status")["mcp"]["serving"]
    env = dict(app.app_env(), XOURNALAI_WORKER="T1", XOURNALAI_PORT=str(app.port))
    subprocess.run([app.binary, "--ai-hook", "UserPromptSubmit"], input=b"{}", env=env, timeout=20)
    time.sleep(0.5)
    assert c.call("app_status")["mcp"]["serving"] == before


def test_4_closing_ends_the_worker_and_its_tab(app):
    c = app.client()
    pids = {w["worker"]: w["pid"] for w in wait_calls(2)}
    r = c.call("workers", op="close", name="T1")
    assert r["closed"] == ["T1"] and [w["name"] for w in r["workers"]] == ["T2"], r
    end = time.time() + 5
    while time.time() < end and alive(pids["T1"]):
        time.sleep(0.2)
    assert not alive(pids["T1"]), "the worker still runs"
    c.call("workers", op="start", name="T3", brief="now there is room")
    r = c.call("workers", op="close", all=True)
    assert sorted(r["closed"]) == ["T2", "T3"] and r["workers"] == [], r
    labels = [x.get("label") for x in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    assert not any(l and l.startswith("⚙ ") for l in labels), labels


def zones(c):
    return {z["id"]: z for z in c.call("thinking_list")["zones"]}


def hook(app, event):
    subprocess.run([app.binary, "--ai-hook", event], input='{"session_id": "s"}', text=True, timeout=20, check=True,
                   env=dict(app.app_env(), XOURNALAI_PORT=str(app.port)))


def test_5_a_worker_s_zone_lasts_through_the_coordinator_s_turns(app):
    """9 Nov, 19:51-19:54: the workers' zones vanished at the coordinator's next idle (it wakes for every transcript
    piece), while the workers thought on for minutes."""
    c = app.client()
    r = c.call("workers", op="start", name="T5", brief="draw topic T5", page=1, region=[20, 30, 200, 100],
               color="#8b3a62", status="«Notations»: thinking…")
    zid = r["zone"]
    z = zones(c)[zid]
    assert z["worker"] == "T5" and z["state"] == "thinking" and z["text"] == "⚙ T5 · «Notations»: thinking…", z
    brief = (app.home / "data" / "xournalai" / "companion" / "workers" / "T5.brief.md").read_text()
    assert brief.startswith("draw topic T5") and f"thinking zone id {zid}, yours" in brief
    # the worker opens one more of its own (its bridge names it), the coordinator one of its own
    w = app.client(headers={"X-Xournalai-Worker": "T5"})
    own = w.call("thinking", op="start", page=1, region=[20, 140, 200, 60], text="searching…", color="teal")["id"]
    mine = c.call("thinking", op="start", page=1, region=[300, 30, 100, 60], text="coordinator")["id"]
    assert zones(c)[own]["text"] == "⚙ T5 · searching…"
    w.call("thinking", op="update", id=own, text="drawing…")
    assert zones(c)[own]["text"] == "⚙ T5 · drawing…"
    hook(app, "SessionStart")
    hook(app, "UserPromptSubmit")
    hook(app, "Stop")  # the coordinator's turn ends
    time.sleep(0.5)
    zs = zones(c)
    assert zs[zid]["state"] == "thinking" and zs[own]["state"] == "thinking", zs
    # (a zone an agent opens itself lasts until it ends it, the coordinator's too: the owner, 2026-11-09)
    assert zs[mine]["state"] == "thinking", zs
    c.call("thinking", op="done", id=mine)
    assert sorted(next(x for x in c.call("workers", op="list")["workers"] if x["name"] == "T5")["zones"]) == \
        sorted([zid, own])
    w.call("thinking", op="done", id=own)
    c.call("workers", op="close", name="T5")  # before it ended its first zone: that one failed, not done
    zs = zones(c)
    assert zs[zid]["state"] == "failed" and zs[zid]["text"] == "⚙ T5 · closed before it finished", zs
    assert zs.get(own, {"state": "gone"})["state"] in ("done", "gone"), zs
    assert "give its region too" in c.call_error("workers", op="start", name="T6", brief="x", color="#000000")


def test_6_the_bridge_of_a_worker_names_it(app):
    """A worker's Claude Code reaches the app through `xournalpp --mcp-stdio`, which inherits XOURNALAI_WORKER."""
    env = dict(app.env, XOURNALAI_WORKER="T7")
    env.pop("DISPLAY", None)  # (the bridge must never start an app by itself here)
    env.pop("WAYLAND_DISPLAY", None)
    p = subprocess.Popen([app.binary, "--mcp-stdio", f"--mcp-port={app.port}"], env=env, text=True,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    try:
        def ask(i, method, params):
            p.stdin.write(json.dumps({"jsonrpc": "2.0", "id": i, "method": method, "params": params}) + "\n")
            p.stdin.flush()
            while True:
                m = json.loads(p.stdout.readline())
                if m.get("id") == i:
                    return m
        ask(1, "initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                              "clientInfo": {"name": "claude-code", "version": "1"}})
        p.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
        p.stdin.flush()
        r = ask(2, "tools/call", {"name": "thinking", "arguments": {"op": "start", "page": 1,
                                                                     "region": [10, 10, 50, 50], "text": "hi"}})
        zid = r["result"]["structuredContent"]["id"]
        z = zones(app.client())[zid]
        assert z["worker"] == "T7" and z["text"] == "⚙ T7 · hi", z
    finally:
        p.stdin.close()
        p.wait(timeout=10)


def worker_hook(app, name, event, payload=None):
    subprocess.run([app.binary, "--ai-hook", event], input=json.dumps(payload or {}), text=True, timeout=20,
                   check=True, env=dict(app.app_env(), XOURNALAI_WORKER=name, XOURNALAI_PORT=str(app.port)))


def test_7_the_app_follows_a_worker_by_its_own_hooks(app):
    """The app knows what it delegated: from the worker's own hooks it shows what the worker does under its zone,
    and when the worker's turn ends (its one job is over) it closes its zones itself."""
    c = app.client()
    serving = c.call("app_status")["mcp"]["serving"]
    zid = c.call("workers", op="start", name="T8", brief="draw T8", page=1, region=[20, 300, 200, 100],
                 color="#4b6584", status="«Analytics»: thinking…")["zone"]

    def status():
        return next(w for w in c.call("workers", op="list")["workers"] if w["name"] == "T8")

    assert zones(c)[zid]["detail"] == "starting" and status()["state"] == "starting"
    for event, payload, activity in [
            ("UserPromptSubmit", {}, "reading its task"),
            ("PreToolUse", {"tool_name": "WebSearch", "tool_input": {"query": "smiles notation"}}, "searching the web"),
            ("PostToolUse", {"tool_name": "WebSearch"}, "thinking"),
            ("PreToolUse", {"tool_name": "Bash", "tool_input": {"command": "python3 lib/inbox.py wait /x T8 100"}},
             "waiting for more talk"),
            ("PreToolUse", {"tool_name": "mcp__xournalai__create_text", "tool_input": {"texts": []}}, "drawing"),
            ("PreToolUse", {"tool_name": "mcp__xournalai__page_render"}, "looking at the page")]:
        worker_hook(app, "T8", event, payload)
        s = status()
        assert s["state"] == "busy" and s["activity"] == activity, (event, s)
        assert zones(c)[zid]["detail"] == activity, zones(c)[zid]
    worker_hook(app, "T5", "PreToolUse", {"tool_name": "WebSearch"})  # (a worker this window no longer has)
    worker_hook(app, "T8", "Stop")  # its turn ended: its job is over
    assert status()["state"] == "finished", status()
    assert zones(c).get(zid, {"state": "gone"})["state"] in ("done", "gone"), zones(c)
    assert c.call("app_status")["mcp"]["serving"] == serving  # (none of it was the serving session's)
    c.call("workers", op="close", name="T8")
