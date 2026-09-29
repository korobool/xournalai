#!/usr/bin/env python3
"""A stand-in for Claude Code in the AI terminal: records its folder and every line typed into it."""
import os
import sys

log = os.environ.get("FAKE_AGENT_LOG") or os.path.join(os.getcwd(), "fake-agent.log")
with open(log, "a") as f:
    f.write(f"START cwd={os.getcwd()} port={os.environ.get('XOURNALAI_PORT')}\n")
print("fake agent ready", flush=True)
for line in sys.stdin:
    with open(log, "a") as f:
        f.write("GOT " + line)
