#!/usr/bin/env bash
# Runs xournalai with its X11 traffic recorded, to see exactly which request the X server left unanswered when the
# app freezes. Keeps only the last lines (the log of a busy app grows fast):
#   tools/xournalai-xtrace.sh [xournalpp args…]      → ~/.cache/xournalai/xtrace.log (last 20000 lines)
# Needs xtrace (sudo apt install xtrace). Pair it with the stall trace's HANG lines: "X: last request sent #N,
# last answered #M" — the request numbers are the sequence numbers in this log.
set -euo pipefail
command -v xtrace >/dev/null || { echo "xtrace is not installed: sudo apt install xtrace" >&2; exit 2; }
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${XOURNALAI_BIN:-$HERE/build/install/bin/xournalpp}"
LOG="${XDG_CACHE_HOME:-$HOME/.cache}/xournalai/xtrace.log"
KEEP="${XOURNALAI_XTRACE_LINES:-20000}"
mkdir -p "$(dirname "$LOG")"
# xtrace prints the protocol to stdout; a small ring buffer rewrites the log every second with the newest lines
xtrace --timestamps -n -- "$BIN" "$@" | python3 -u -c '
import collections, sys, time
log, keep = sys.argv[1], int(sys.argv[2])
ring, last = collections.deque(maxlen=keep), 0.0
def flush():
    with open(log + ".tmp", "w") as f: f.writelines(ring)
    import os; os.replace(log + ".tmp", log)
for line in sys.stdin:
    ring.append(line)
    if time.time() - last > 1: flush(); last = time.time()
flush()
' "$LOG" "$KEEP"
