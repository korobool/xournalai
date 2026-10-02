#!/usr/bin/env bash
# Transcribe a xournalai audio recording on your own GPU server (e.g. faster-whisper large-v3), for the assistant's
# "remote transcriber first" rule.
#   transcribe-remote.sh [file.ogg] [-l ru|en ...]   (default: the newest recording)
# Prints the transcript; saves <stem>.txt/.srt/.json next to the recording, in <audio folder>/transcripts/.
#
# Settings (not in the repo): ~/.config/xournalai/transcribe.conf, a shell file with
#   HOST=user@gpu-server               # ssh/scp target (key-based login)
#   REMOTE_DIR=transcript              # on the server: in/, out/, logs/ and bin/transcribe.sh <file> [args]
#   AUDIO_DIR=~/Music                  # xournalai settings: audio folder
set -euo pipefail
CONF="${XOURNALAI_TRANSCRIBE_CONF:-$HOME/.config/xournalai/transcribe.conf}"
[[ -f "$CONF" ]] || { echo "no remote transcriber configured ($CONF); use audio_transcribe instead" >&2; exit 2; }
# shellcheck source=/dev/null
source "$CONF"
: "${HOST:?HOST missing in $CONF}"
REMOTE_DIR="${REMOTE_DIR:-transcript}"
AUDIO_DIR="${AUDIO_DIR:-$HOME/Music}"
AUDIO_DIR="${AUDIO_DIR/#\~/$HOME}"
OUT_DIR="$AUDIO_DIR/transcripts"

f="${1:-}"
if [[ -z "$f" || "$f" == -* ]]; then
  shopt -s nullglob
  files=("$AUDIO_DIR"/*.{ogg,wav,mp3,m4a})
  f="$( (( ${#files[@]} )) && ls -t "${files[@]}" | head -1 || true)"
else
  shift
fi
[[ -f "$f" ]] || { echo "no recording found" >&2; exit 1; }
stem="$(basename "${f%.*}")"
mkdir -p "$OUT_DIR"

scp -q -o BatchMode=yes "$f" "$HOST:$REMOTE_DIR/in/"
ssh -o BatchMode=yes "$HOST" "cd ~/$REMOTE_DIR && bin/transcribe.sh 'in/$(basename "$f")' $* >> 'logs/$stem.log' 2>&1"
for ext in txt srt json; do
  scp -q -o BatchMode=yes "$HOST:$REMOTE_DIR/out/$stem.$ext" "$OUT_DIR/" || true
done
cat "$OUT_DIR/$stem.txt"
