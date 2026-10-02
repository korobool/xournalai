"""Where the owner's captured handwriting (glyphs_user_raw.json) lives. It is personal, so it is not in the repo:
$XOURNALAI_GLYPHS, else ~/.local/share/xournalai/glyphs_user_raw.json, else next to this library (git-ignored).
capture_glyphs.py writes to the same place."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "glyphs_user_raw.json"


def candidates():
    env = os.environ.get("XOURNALAI_GLYPHS")
    data = os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return [p for p in (env, os.path.join(data, "xournalai", NAME), os.path.join(HERE, NAME)) if p]


def user_glyphs():
    """The owner's glyph file, or None when there is none (then the designed v2 font is used)"""
    for p in candidates():
        if os.path.exists(p):
            return p
    return None


def write_target():
    """Where captured glyphs are saved: $XOURNALAI_GLYPHS, else the per-user data folder"""
    data = os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return os.environ.get("XOURNALAI_GLYPHS") or os.path.join(data, "xournalai", NAME)
