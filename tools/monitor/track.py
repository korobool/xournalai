#!/usr/bin/env python3
"""Progress tracking CLI for the xournalai MCP roadmap.

Task status lives in docs/mcp/tasks.json (committed with each task).
Live, uncommitted activity (log lines, build and test results) lives in tools/monitor/state/ (git-ignored).

Usage:
  track.py start  <task> [note]    mark a task as in progress
  track.py review <task> [note]    mark a task as being verified (build/test)
  track.py done   <task> [note]    mark a task as done (its commit is found by the "(<task>)" suffix)
  track.py block  <task> <note>    mark a task as blocked
  track.py log    <message>        append a line to the activity feed
  track.py build  ok|fail|running [detail]
  track.py test   ok|fail|running [detail]
  track.py version <x.y.z>         set meta.version
  track.py roadmap                 regenerate docs/mcp/ROADMAP.md from tasks.json
  track.py next                    print the next task that is not done
"""

import datetime as dt
import json
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
TASKS = ROOT / "docs/mcp/tasks.json"
ROADMAP = ROOT / "docs/mcp/ROADMAP.md"
STATE = ROOT / "tools/monitor/state"
ACTIVITY = STATE / "activity.jsonl"
STATUS = STATE / "status.json"


def now():
    return dt.datetime.now().astimezone().isoformat(timespec="seconds")


def load():
    return json.loads(TASKS.read_text())


def save(data):
    TASKS.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def iter_tasks(data):
    for epoch in data["epochs"]:
        for stage in epoch["stages"]:
            for task in stage["tasks"]:
                yield epoch, stage, task


def find(data, task_id):
    for _, _, task in iter_tasks(data):
        if task["id"] == task_id:
            return task
    sys.exit(f"unknown task {task_id}")


def log(kind, message, task=None):
    STATE.mkdir(parents=True, exist_ok=True)
    entry = {"time": now(), "kind": kind, "message": message}
    if task:
        entry["task"] = task
    with ACTIVITY.open("a") as f:
        f.write(json.dumps(entry, ensure_ascii=False) + "\n")


def set_status(task_id, status, note=None):
    data = load()
    task = find(data, task_id)
    task["status"] = status
    if status == "doing" and "started" not in task:
        task["started"] = now()
    if status == "done":
        task["finished"] = now()
    if note:
        task["note"] = note
    save(data)
    log("status", f"{task_id} → {status}" + (f": {note}" if note else ""), task_id)


def update_state(key, value, detail):
    STATE.mkdir(parents=True, exist_ok=True)
    state = json.loads(STATUS.read_text()) if STATUS.exists() else {}
    state[key] = {"status": value, "detail": detail, "time": now()}
    STATUS.write_text(json.dumps(state, indent=2))
    log(key, f"{key} {value}" + (f": {detail}" if detail else ""))


def task_commits():
    """Map task id -> short sha, using the "(Txx)" suffix of commit messages."""
    out = subprocess.run(["git", "-C", str(ROOT), "log", "--format=%h %s"], capture_output=True, text=True).stdout
    commits = {}
    for line in out.splitlines():
        sha, _, subject = line.partition(" ")
        m = re.search(r"\((T\d+\.\d+\.\d+)\)$", subject)
        if m:
            commits.setdefault(m.group(1), sha)
    return commits


def render_roadmap(data):
    commits = task_commits()
    status_icon = {"done": "✅", "doing": "🔨", "review": "🔍", "blocked": "⛔"}
    out = [
        "# xournalai MCP — implementation roadmap",
        "",
        "> Generated from `docs/mcp/tasks.json` by `tools/monitor/track.py roadmap`. Do not edit by hand.",
        "> Design: [PLAN.md](PLAN.md). Live board: `tools/monitor/run.sh` → http://127.0.0.1:8765",
        "",
        "## Versioning & commits",
        "",
    ]
    out += [f"- {rule}" for rule in data["meta"]["versioning"]]
    out += ["", f"Current version: **{data['meta']['version']}**", ""]
    for epoch in data["epochs"]:
        out += [f"## {epoch['id']} — {epoch['title']} (release {epoch['release']})", "", epoch["goal"], ""]
        for stage in epoch["stages"]:
            out += [f"### {stage['id']} — {stage['title']} → v{stage['version']}", ""]
            for story in stage["stories"]:
                out += [f"**{story['id']}** — {story['text']}", ""]
                out += [f"- [ ] {a}" for a in story["acceptance"]]
                out += [""]
            out += ["| Task | Story | Title | Commit | Status |", "|---|---|---|---|---|"]
            for task in stage["tasks"]:
                status = task.get("status", "todo")
                sha = f" `{commits[task['id']]}`" if task["id"] in commits else ""
                out.append(
                    f"| {task['id']} | {task['story']} | **{task['title']}** — {task['details']} "
                    f"| `{task['commit']} ({task['id']})` | {status_icon.get(status, '⬜')} {status}{sha} |"
                )
            out += [""]
    ROADMAP.write_text("\n".join(out))


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    cmd, args = argv[1], argv[2:]
    if cmd in ("start", "review", "done", "block"):
        status = {"start": "doing", "block": "blocked"}.get(cmd, cmd)
        set_status(args[0], status, " ".join(args[1:]) or None)
        render_roadmap(load())
    elif cmd == "log":
        log("note", " ".join(args))
    elif cmd in ("build", "test"):
        update_state(cmd, args[0], " ".join(args[1:]))
    elif cmd == "version":
        data = load()
        data["meta"]["version"] = args[0]
        save(data)
        render_roadmap(data)
        log("version", f"version → {args[0]}")
    elif cmd == "roadmap":
        render_roadmap(load())
    elif cmd == "next":
        for _, _, task in iter_tasks(load()):
            if task.get("status") != "done":
                print(task["id"], task["title"])
                break
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main(sys.argv)
