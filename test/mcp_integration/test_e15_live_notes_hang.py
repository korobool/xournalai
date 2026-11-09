"""E15: a "file" recognizer that hangs, with a child holding its output (like scp/ssh in a script): after its
timeout the piece is failed, the next pieces go on at once, and the hung command is ended with its child (the
review of 2026-11-09: only the shell was killed; the child kept the pipe open and blocked every later piece)."""

import pathlib
import subprocess
import tempfile
import time

from test_e15_live_notes import wait, wav, texts

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-live-hang-"))
MARK = f"xoai-hang-{DIR.name}"
SCRIPT = DIR / "recognize.sh"
SCRIPT.write_text(f"""#!/bin/bash
case "$1" in
  *piece-0001.wav) bash -c 'exec -a {MARK} sleep 60' ; echo never;;   # hangs (a child holding stdout)
  *) echo "text of $(basename "$1")";;
esac
""")
SCRIPT.chmod(0o755)
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": False,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "file", "command": f"{SCRIPT} {{audio}}",
                                                           "timeout_s": 2}}}}


def test_1_a_hung_run_is_ended_with_its_child_and_the_rest_goes_on(app):
    c = app.client()
    c.call("test_live_audio", op="start", name="hang.ogg")
    parts = [("noise", 0.5)]
    for _ in range(3):
        parts += [("voice", 0.6), ("noise", 1.6)]
    c.call("test_live_audio", op="feed", file=wav(DIR / "h.wav", parts))
    start = time.time()
    st = wait(lambda: (lambda s: s if s["written"] >= 3 else None)(c.call("live_notes", op="status")), 20)
    assert st and time.time() - start < 8, (st, time.time() - start)  # (not blocked by the hung child)
    lines = [s for s in texts(c, st["pages"][0]["page"]) if s.startswith("[")]
    assert "(not recognized: no answer in 2 s)" in lines[0] and lines[1].endswith("text of piece-0002.wav"), lines
    assert wait(lambda: subprocess.run(["pgrep", "-f", MARK], capture_output=True).returncode != 0, 5), \
        "the hung command's child is still running"
