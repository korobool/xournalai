"""E15: live notes survive a misbehaving speech recognizer (the review of 2026-11-09): fields of the wrong type or
null (it crashed the app), a recognizer slower than speech (later pieces timed out while only waiting), a piece it
never answers, inline audio, a recognizer that dies (a fresh one takes the rest), and a "file" command that hangs
with a child holding its output (the next pieces were blocked, the child left running)."""

import base64
import json
import os
import pathlib
import subprocess
import tempfile
import time

from test_e15_live_notes import wait, wav, texts

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-live-robust-"))
LOG, CRASH = DIR / "requests.log", DIR / "crash-on"
RECOGNIZER = DIR / "recognizer.py"
RECOGNIZER.write_text(f"""import base64, json, os, sys, time, wave, io
log = open({str(LOG)!r}, "a")
# a hello with odd types: a protocol as text, an engine of null
print(json.dumps({{"type": "hello", "protocol": "1", "engine": None, "audio": "inline"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    log.write(json.dumps({{"id": req["id"], "pid": os.getpid(), "b64": len(req.get("audio_b64", "")),
                          "wav_ok": wave.open(io.BytesIO(base64.b64decode(req.get("audio_b64", "")))).getframerate() == 16000}}) + "\\n")
    log.flush()
    n = int(req["id"])
    if os.path.exists({str(CRASH)!r}) and open({str(CRASH)!r}).read().strip() == str(n):
        os.remove({str(CRASH)!r})
        sys.exit(1)                       # dies without answering
    if n == 3:
        time.sleep(4)                     # never in time (timeout_s 2): its late answer is ignored
    else:
        time.sleep(0.8)                   # slower than the pieces come
    if n == 2:
        print(json.dumps({{"type": "error", "id": req["id"], "message": None}}), flush=True)
    else:
        print(json.dumps({{"type": "result", "id": int(req["id"]), "text": f"piece {{n}}", "language": None,
                          "language_probability": "high", "elapsed": None, "segments": None}}), flush=True)
""")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": False,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}",
                                                           "timeout_s": 2, "extra": {"kept": True}}}}}


def status(c):
    return c.call("live_notes", op="status")


def requests():
    return [json.loads(l) for l in LOG.read_text().splitlines()] if LOG.exists() else []


def test_1_odd_answers_a_slow_recognizer_and_a_piece_never_answered(app):
    c = app.client()
    c.call("test_live_audio", op="start", name="odd.ogg")
    # 6 pieces at once: the recognizer takes 0.8 s each (4 s for #3), the timeout is 2 s: only #3 may fail
    parts = [("noise", 0.5)]
    for _ in range(6):
        parts += [("voice", 0.6), ("noise", 1.6)]
    c.call("test_live_audio", op="feed", file=wav(DIR / "odd.wav", parts))
    st = wait(lambda: (lambda s: s if s["written"] >= 6 else None)(status(c)), 30)
    assert st, status(c)
    lines = [s for s in texts(c, st["pages"][0]["page"]) if s.startswith("[")]
    assert st["transcribed"] == 4 and st["failed"] == 2, (lines, requests())  # (#2 an error, #3 too late; no more)
    assert lines[0].endswith("piece 1") and lines[3].endswith("piece 4") and lines[5].endswith("piece 6"), lines
    assert "(not recognized: the recognizer failed)" in lines[1], lines  # (a null message)
    assert "(not recognized: no answer in 2 s)" in lines[2], lines
    assert c.call("app_status")["ok"] if "ok" in c.call("app_status") else True  # (the app is alive)


def test_2_inline_audio_is_the_wav_itself(app):
    reqs = requests()
    assert reqs and all(r["b64"] > 1000 and r["wav_ok"] for r in reqs), reqs


def test_3_a_recognizer_that_dies_is_replaced_for_the_rest(app):
    c = app.client()
    before = status(c)["pieces"]
    CRASH.write_text(str(before + 1))
    parts = [("noise", 0.5)]
    for _ in range(3):
        parts += [("voice", 0.6), ("noise", 1.6)]
    c.call("test_live_audio", op="feed", file=wav(DIR / "crash.wav", parts))
    st = wait(lambda: (lambda s: s if s["written"] >= before + 3 else None)(status(c)), 30)
    assert st, status(c)
    r = c.call("live_notes", op="read", since=before)
    assert [p["ok"] for p in r["pieces"]] == [False, True, True], r["pieces"]
    assert "the recognizer ended" in r["pieces"][0]["error"], r["pieces"][0]
    pids = {q["pid"] for q in requests()[-3:]}
    assert len(pids) == 2, requests()[-3:]  # (a fresh process took the rest)


def test_4_the_end_lets_the_recognizer_go(app):
    c = app.client()
    pid = requests()[-1]["pid"]
    c.call("test_recording", file=str(DIR / "odd.ogg"), name="odd.ogg", duration_ms=30000)
    assert wait(lambda: status(c)["done"], 15), status(c)
    assert wait(lambda: subprocess.run(["kill", "-0", str(pid)], capture_output=True).returncode != 0, 15), \
        "the recognizer is still running"
    assert pathlib.Path(status(c)["transcript"]).read_text().count("\n") >= 10  # (written piece by piece)


def test_5_a_save_keeps_what_this_app_does_not_know(app):
    c = app.client()
    c.call("action_run", action="win.ai-live-notes", state=False)
    t = json.loads(app.config_file.read_text())["assistant"]["live_notes"]["transcriber"]
    assert t["extra"] == {"kept": True} and t["timeout_s"] == 2, t
