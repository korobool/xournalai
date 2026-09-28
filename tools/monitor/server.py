#!/usr/bin/env python3
"""Local web server for the xournalai MCP progress board.

Serves index.html and /api/state (tasks.json + git history + live activity + build/test status + log tails).
Standard library only. Binds to 127.0.0.1.

    python3 tools/monitor/server.py [--port 8765]
"""

import argparse
import http.server
import json
import pathlib
import re
import subprocess
from collections import deque

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
TASKS = ROOT / "docs/mcp/tasks.json"
STATE = HERE / "state"
LOGS = {
    "configure": ROOT / "build/configure.log",
    "build": ROOT / "build/build.log",
    "tests-build": ROOT / "build/test-build.log",
    "tests": ROOT / "build/test.log",
    "integration": ROOT / "build/integration.log",
}
TASK_RE = re.compile(r"\((T\d+\.\d+\.\d+)\)$")


def git(*args):
    return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True).stdout


def tail(path, n):
    try:
        with path.open(errors="replace") as f:
            return list(deque(f, maxlen=n))
    except OSError:
        return []


def commits():
    out = git("log", "--max-count=60", "--date=iso-strict", "--format=%h%x1f%ad%x1f%s%x1f%D", "--shortstat")
    result, current = [], None
    for line in out.splitlines():
        if "\x1f" in line:
            sha, date, subject, refs = line.split("\x1f")
            m = TASK_RE.search(subject)
            current = {"sha": sha, "date": date, "subject": subject, "refs": refs, "task": m.group(1) if m else None}
            result.append(current)
        elif line.strip() and current is not None:
            current["stat"] = line.strip()
    return result


def state():
    data = json.loads(TASKS.read_text())
    log_mtimes = {k: p.stat().st_mtime for k, p in LOGS.items() if p.exists()}
    status_file = STATE / "status.json"
    return {
        "tasks": data,
        "commits": commits(),
        "activity": [json.loads(l) for l in tail(STATE / "activity.jsonl", 200) if l.strip()],
        "status": json.loads(status_file.read_text()) if status_file.exists() else {},
        "git": {
            "branch": git("rev-parse", "--abbrev-ref", "HEAD").strip(),
            "head": git("rev-parse", "--short", "HEAD").strip(),
            "dirty": [l for l in git("status", "--porcelain").splitlines() if l.strip()],
            "tags": [t for t in git("tag", "--list", "ai-v*").split() if t],
        },
        "logs": {k: {"mtime": log_mtimes[k], "tail": tail(LOGS[k], 40)} for k in log_mtimes},
    }


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path.startswith("/index.html"):
            self.send(200, (HERE / "index.html").read_bytes(), "text/html; charset=utf-8")
        elif self.path.startswith("/api/state"):
            try:
                body = json.dumps(state()).encode()
                self.send(200, body, "application/json")
            except Exception as e:  # keep the board alive while files are mid-write
                self.send(500, json.dumps({"error": str(e)}).encode(), "application/json")
        else:
            self.send(404, b"not found", "text/plain")

    def send(self, code, body, ctype):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"xournalai progress board: http://127.0.0.1:{args.port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
