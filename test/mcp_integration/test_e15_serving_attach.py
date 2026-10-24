"""E15: the serving terminal finds the serving conversation also when Claude Code keeps it running in the background.
On 2026-10-24 the window closed while the serving session worked; Claude Code (2.1) kept that conversation running in
the background and then refused --continue ("Your most recent conversation is running in the background"): after the
restart the terminal could not run Claude. A fake `claude` on PATH records how it is called."""

import os
import pathlib
import re
import stat
import tempfile
import time

ROOT = pathlib.Path(tempfile.mkdtemp(prefix="xoai-attach-"))
HOME, DATA, BIN = ROOT / "home", ROOT / "data", ROOT / "bin"
LOG, BACKGROUND = ROOT / "claude.log", ROOT / "background"  # (while BACKGROUND exists, a session runs there)
COMPANION = str(DATA / "xournalai" / "companion")
for d in (HOME / ".claude" / "projects" / re.sub(r"[^A-Za-z0-9-]", "-", COMPANION), BIN):
    d.mkdir(parents=True, exist_ok=True)
(HOME / ".claude" / "projects" / re.sub(r"[^A-Za-z0-9-]", "-", COMPANION) / "earlier.jsonl").write_text("{}\n")
BACKGROUND.write_text("")
FAKE = BIN / "claude"
FAKE.write_text(f"""#!/bin/sh
echo "$@" >> {LOG}
if [ "$1" = agents ]; then
    if [ -e {BACKGROUND} ]; then
        printf '[\\n  {{\\n    "pid": 4242,\\n    "id": "1bc62f83",\\n    "cwd": "%s",\\n    "kind": "background",\\n' "$PWD"
        printf '    "sessionId": "1bc62f83-d2f2-4e75-a5a9-6d4d05a32847"\\n  }}\\n]\\n'
    else
        echo "[]"
    fi
    exit 0
fi
echo "fake claude: $@"
sleep 600
""")
FAKE.chmod(FAKE.stat().st_mode | stat.S_IEXEC)
APP_ENV = {"HOME": str(HOME), "XDG_DATA_HOME": str(DATA), "PATH": f"{BIN}:{os.environ['PATH']}"}
APP_CONFIG = {"assistant": {"autostart": True}}


def calls(timeout=15, expect=lambda lines: lines):
    end = time.time() + timeout
    while time.time() < end:
        lines = LOG.read_text().splitlines() if LOG.exists() else []
        if expect(lines):
            return lines
        time.sleep(0.2)
    return LOG.read_text().splitlines() if LOG.exists() else []


def test_1_it_attaches_to_the_conversation_running_in_the_background(app):
    lines = calls(expect=lambda l: any(x.startswith("attach") for x in l))
    assert lines[:1] == [f"agents --json --cwd {COMPANION}"], lines
    assert "attach 1bc62f83" in lines and not any("--continue" in x for x in lines), lines


def test_2_without_one_it_continues_the_last_conversation(app):
    c = app.client()
    BACKGROUND.unlink()
    LOG.unlink()
    restart = [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]
               if w.get("tooltip") == "Restart the program in this tab"][0]
    c.call("ui_interact", op="click", target=restart["id"])
    lines = calls(expect=lambda l: any("--continue" in x for x in l))
    assert lines[:1] == [f"agents --json --cwd {COMPANION}"], lines
    assert "--dangerously-skip-permissions --continue" in lines and not any(x.startswith("attach") for x in lines)
