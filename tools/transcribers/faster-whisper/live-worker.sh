#!/usr/bin/env bash
# Runs live_worker.py in the faster-whisper setup it is installed into: <root>/bin/live-worker.sh, with the venv in
# <root>/.venv and the models in <root>/models (the layout of the owner's transcription server).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT/.venv/bin/python" -u "$ROOT/bin/live_worker.py" "$@"
