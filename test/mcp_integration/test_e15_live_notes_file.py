"""E15: live notes with a "file" recognizer (docs/mcp/TRANSCRIBERS.md): a command run once per piece, its stdout the
text ({audio}, {language}, {prompt} replaced); a piece it fails on is marked in the column with the reason."""

import pathlib
import tempfile
import time

from test_e15_live_notes import wait, wav, texts

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-live-file-"))
CALLS = DIR / "calls.log"
SCRIPT = DIR / "recognize.sh"
SCRIPT.write_text(f"""#!/bin/sh
echo "$1|$2|$3" >> {CALLS}
case "$1" in
  *piece-0002.wav) echo "the model is out of memory" >&2; exit 3;;
esac
echo "  text of $(basename "$1")  "
""")
SCRIPT.chmod(0o755)
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": False,
                            "live_notes": {"enabled": True, "language": "ru",
                                           "transcriber": {"kind": "file",
                                                           "command": f"{SCRIPT} {{audio}} {{language}} {{prompt}}"}}}}


def test_1_a_command_per_piece_and_a_failed_piece(app):
    c = app.client()
    c.call("test_live_audio", op="start", name="memo.ogg")
    for i in range(3):
        c.call("test_live_audio", op="feed", file=wav(DIR / f"{i}.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
        assert wait(lambda: c.call("live_notes", op="status")["written"] >= i + 1), c.call("live_notes", op="status")
    st = c.call("live_notes", op="status")
    assert st["transcribed"] == 2 and st["failed"] == 1 and "engine" not in st, st
    lines = [s for s in texts(c, st["pages"][0]["page"]) if s.startswith("[")]
    assert lines[0] == "[0:00] text of piece-0001.wav", lines  # (stdout, trimmed)
    assert lines[1].endswith("(not recognized: the model is out of memory)"), lines  # (the last line of stderr)
    assert lines[2].endswith("text of piece-0003.wav"), lines  # (the next pieces go on)
    calls = CALLS.read_text().splitlines()
    assert calls[0].endswith("piece-0001.wav|ru|"), calls  # (the language set; no prompt yet)
    assert calls[2].endswith("|ru|text of piece-0001.wav"), calls  # (the last text that came back)
    r = c.call("live_notes", op="read")
    assert [p["ok"] for p in r["pieces"]] == [True, False, True] and "memory" in r["pieces"][1]["error"], r
