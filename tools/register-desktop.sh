#!/usr/bin/env bash
# Registers the installed xournalai (build/install) as a desktop application for the current user:
# a "xournalai" launcher with its own icon, separate from a regular Xournal++. Undo with --remove.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APPS="${XDG_DATA_HOME:-$HOME/.local/share}/applications"
ICONS="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps"
ID=io.github.korobool.xournalai
if [[ "${1:-}" == "--remove" ]]; then
    rm -f "$APPS/$ID.desktop" "$ICONS/$ID.svg"
else
    DESKTOP="$ROOT/build/install/share/applications/$ID.desktop"
    [[ -f "$DESKTOP" ]] || { echo "Build and install first: cmake --build build --target install" >&2; exit 1; }
    mkdir -p "$APPS" "$ICONS"
    install -m 644 "$DESKTOP" "$APPS/$ID.desktop"
    install -m 644 "$ROOT/build/install/share/icons/hicolor/scalable/apps/$ID.svg" "$ICONS/$ID.svg"
fi
update-desktop-database "$APPS" 2>/dev/null || true
gtk-update-icon-cache -f -t "${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor" 2>/dev/null || true
echo "${1:-registered}: $APPS/$ID.desktop"
