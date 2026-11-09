#!/usr/bin/env bash
# Installs xournalai's skills for Claude Code (links, so `git pull` updates them) and the companion's transcriber.
#   skills/install.sh
# - ~/.claude/skills/<skill> -> this folder's skills (an existing copy is kept as <skill>.bak-<date>)
# - your handwriting glyphs stay private: an existing copy is moved to ~/.local/share/xournalai/glyphs_user_raw.json
# - transcribe-remote.sh goes to the companion's bin; its server comes from ~/.config/xournalai/transcribe.conf
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILLS="$HOME/.claude/skills"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}/xournalai"
CONF="$HOME/.config/xournalai/transcribe.conf"
STAMP="$(date +%Y%m%d-%H%M%S)"
mkdir -p "$SKILLS" "$DATA"

for skill in xournal-conspect conversation-topic-map; do
  dest="$SKILLS/$skill"
  if [[ -L "$dest" ]]; then
    rm "$dest"
  elif [[ -e "$dest" ]]; then
    glyphs="$dest/lib/glyphs_user_raw.json"
    if [[ -f "$glyphs" && ! -f "$DATA/glyphs_user_raw.json" ]]; then
      cp "$glyphs" "$DATA/glyphs_user_raw.json" && echo "your glyphs: $DATA/glyphs_user_raw.json"
    fi
    mv "$dest" "$dest.bak-$STAMP" && echo "kept the old $skill as $dest.bak-$STAMP"
  fi
  ln -s "$HERE/$skill" "$dest" && echo "$dest -> $HERE/$skill"
done

bin="$DATA/companion/bin"
mkdir -p "$bin"
old="$bin/transcribe-remote.sh"
if [[ ! -f "$CONF" && -f "$old" && ! -L "$old" ]]; then  # carry an existing server setting over
  host="$(grep -oE '^HOST=[^ #]+' "$old" | head -1 || true)"
  if [[ -n "$host" ]]; then
    mkdir -p "$(dirname "$CONF")"
    printf '%s\nREMOTE_DIR=transcript\nAUDIO_DIR=~/Music\n' "$host" > "$CONF" && chmod 600 "$CONF"
    echo "remote transcriber settings: $CONF"
  fi
fi
[[ -f "$old" && ! -L "$old" ]] && mv "$old" "$old.bak-$STAMP"
ln -sf "$HERE/companion-bin/transcribe-remote.sh" "$old" && echo "$old -> companion-bin/transcribe-remote.sh"
[[ -f "$CONF" ]] || echo "no remote transcriber yet: write $CONF (see companion-bin/transcribe-remote.sh)"
