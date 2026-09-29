#!/usr/bin/env python3
"""Checks that installed MCP clients can connect to xournalai, without touching their user configuration.

    python3 tools/mcpdoc/client_matrix.py

A headless xournalai (installed build, Xvfb, isolated config) is started; each client gets a project-local config in
a temporary directory and is asked for its MCP server status ("mcp list"), which connects and lists tools but makes
no model calls. The official MCP Python SDK is covered by test/mcp_integration/test_e5_sdk_client.py.
"""

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "test" / "mcp_integration"))
import xoai  # noqa: E402


def run(cmd, cwd, env=None, timeout=90):
    try:
        p = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=timeout,
                           stdin=subprocess.DEVNULL)
        return p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired as e:
        return None, (e.stdout or "") + (e.stderr or "") if isinstance(e.stdout, str) else "timeout"


def version(cmd):
    code, out = run([cmd, "--version"], cwd=tempfile.gettempdir(), timeout=30)
    return out.strip().splitlines()[-1] if out.strip() else "?"


def gemini(url, token, d):
    # Project settings of an untrusted folder are ignored, so the server goes into a temporary *system* settings
    # file (GEMINI_CLI_SYSTEM_SETTINGS_PATH); the user's ~/.gemini is untouched
    system = d / "gemini-system.json"
    system.write_text(json.dumps(
        {"mcpServers": {"xournalai": {"httpUrl": url, "headers": {"Authorization": f"Bearer {token}"}}}}))
    env = dict(os.environ, GEMINI_CLI_SYSTEM_SETTINGS_PATH=str(system))
    # Gemini disables MCP servers in untrusted folders; run from the repository (usually trusted), writing nothing
    code, out = run(["gemini", "mcp", "list"], ROOT, env=env)
    line = next((l for l in out.splitlines() if "xournalai" in l), out.strip()[-200:])
    return ("Connected" in line or "✓" in line), line.strip()


def opencode(url, token, d):
    (d / "opencode.json").write_text(json.dumps({"$schema": "https://opencode.ai/config.json", "mcp": {
        "xournalai": {"type": "remote", "url": url, "headers": {"Authorization": f"Bearer {token}"},
                      "enabled": True}}}))
    code, out = run(["opencode", "mcp", "list"], d)
    out = re.sub(r"\x1b\[[0-9;]*m", "", out)
    line = next((l for l in out.splitlines() if "xournalai" in l), out.strip()[-200:])
    return ("connected" in line.lower() and "fail" not in line.lower()), line.strip()


def claude(url, token, d):
    config = d / "mcp.json"
    config.write_text(json.dumps({"mcpServers": {"xournalai": {
        "type": "http", "url": url, "headers": {"Authorization": f"Bearer {token}"}}}}))
    # `claude mcp list` health-checks approved servers; project .mcp.json servers need the user's approval first
    (d / ".mcp.json").write_text(config.read_text())
    code, out = run(["claude", "mcp", "list"], d)
    line = next((l for l in out.splitlines() if "xournalai" in l), out.strip()[-200:])
    if "Pending approval" in line:
        # Approvals live in ~/.claude.json, which this check never edits: the configuration was read and accepted
        return None, "config accepted; project servers need a one-time approval in `claude` (not given by this check)"
    return ("Connected" in line or "✓" in line), line.strip()


def codex(url, token, d):
    # Codex connects when a session starts; `mcp get` validates the configuration only. The config lives in an
    # isolated CODEX_HOME so the user's ~/.codex is untouched.
    home = d / "codex-home"
    home.mkdir()
    (home / "config.toml").write_text(
        f'[mcp_servers.xournalai]\nurl = "{url}"\nbearer_token_env_var = "XOURNALAI_TOKEN"\n')
    env = dict(os.environ, CODEX_HOME=str(home), XOURNALAI_TOKEN=token)
    code, out = run(["codex", "mcp", "get", "xournalai"], d, env=env)
    if code == 0 and url in out:
        return None, "config accepted (Codex connects when a session starts; `codex mcp get` shows it as enabled)"
    return False, " ".join(out.split())[:160]


CLIENTS = [("Gemini CLI", "gemini", gemini), ("OpenCode", "opencode", opencode), ("Claude Code", "claude", claude),
           ("OpenAI Codex CLI", "codex", codex)]


def main():
    rows = []
    with xoai.App() as app:
        url = f"http://127.0.0.1:{app.port}/mcp"
        for label, cmd, check in CLIENTS:
            if not shutil.which(cmd):
                rows.append((label, "not installed", "—", ""))
                continue
            with tempfile.TemporaryDirectory(prefix="xoai-client-") as tmp:
                try:
                    ok, detail = check(url, app.token, pathlib.Path(tmp))
                except Exception as e:  # noqa: BLE001
                    ok, detail = False, f"error: {e}"
            mark = "✅" if ok else ("⚠️" if ok is None else "❌")
            rows.append((label, version(cmd), mark, detail.replace("|", "/")))
    print("| Client | Version | HTTP | Details |")
    print("|---|---|---|---|")
    for r in rows:
        print(f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} |")


if __name__ == "__main__":
    main()
