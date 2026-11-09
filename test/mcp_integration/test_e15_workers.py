"""E15: worker sessions in tabs of the AI terminal (the owner's choice, 2026-11-09): the serving session starts a
Claude Code worker per heavy job with `workers`; it runs in <companion>/workers with only the safe list's tools
(dontAsk), reads its brief as its first message, is not taken for the serving session (its hooks are ignored), and is
closed with its tab. A fake `claude` on PATH records how it is called."""

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
    assert a[a.index("--allowedTools") + 1] == "mcp__xournalai,WebSearch,WebFetch,Read,Bash(python3 *)", a
    assert a[a.index("--mcp-config") + 1] == str(companion / ".mcp.json") and "--strict-mcp-config" in a, a
    assert a[a.index("--effort") + 1] == "xhigh" and a[a.index("--name") + 1] == "live T1", a
    assert a[-1] == brief, a[-1]  # (the brief exactly: nothing in it is run by the shell)
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
    """The worker runs Claude Code too: if a hook of it reached the app, the app would take it for the serving
    session (busy, idle). XOURNALAI_WORKER makes `xournalpp --ai-hook` report nothing."""
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
