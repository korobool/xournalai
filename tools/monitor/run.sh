#!/usr/bin/env bash
# Start the xournalai progress board (if not already running) and open it in the browser.
set -euo pipefail
PORT="${1:-8765}"
DIR="$(cd "$(dirname "$0")" && pwd)"
if ! curl -fs "http://127.0.0.1:${PORT}/api/state" >/dev/null 2>&1; then
    mkdir -p "$DIR/state"
    nohup python3 "$DIR/server.py" --port "$PORT" >"$DIR/state/server.log" 2>&1 &
    sleep 0.5
fi
echo "Progress board: http://127.0.0.1:${PORT}"
command -v xdg-open >/dev/null && xdg-open "http://127.0.0.1:${PORT}" >/dev/null 2>&1 || true
