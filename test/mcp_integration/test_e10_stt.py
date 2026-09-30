"""E10: the local speech-to-text helper (xournalai-stt, whisper.cpp) transcribes speech and ignores silence."""

import json
import math
import os
import pathlib
import random
import struct
import subprocess
import tempfile
import wave

import xoai

BIN = pathlib.Path(xoai.BINARY).with_name("xournalai-stt")
MODEL = pathlib.Path(os.environ.get("XOURNALAI_STT_MODEL",
                                    pathlib.Path.home() / ".local/share/xournalai/models/ggml-base.en.bin"))
SAMPLE = next(pathlib.Path(xoai.ROOT, "build").glob("_deps/whisper-src/samples/jfk.wav"), None)


def run(*commands):
    """Sends the commands to a fresh helper; returns its answers (after "ready")."""
    assert BIN.exists(), f"{BIN} not built (ENABLE_STT)"
    assert MODEL.exists(), f"speech model missing: {MODEL}"
    lines = "".join(json.dumps(c) + "\n" for c in [*commands, {"cmd": "quit"}])
    out = subprocess.run([str(BIN), "--model", str(MODEL)], input=lines, capture_output=True, text=True, timeout=120)
    answers = [json.loads(l) for l in out.stdout.splitlines() if l.strip()]
    assert answers and answers[0]["event"] == "ready", out.stdout + out.stderr
    return answers[1:]


def write_wav(path, samples):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(16000)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s)) * 32767)) for s in samples))


def test_1_it_transcribes_speech(app):
    assert SAMPLE, "whisper.cpp sample not found in build/_deps"
    (r,) = run({"cmd": "transcribe", "wav": str(SAMPLE)})
    assert r["event"] == "text" and not r["silent"]
    assert "what your country can do for you" in r["text"].lower(), r


def test_2_silence_and_hiss_are_not_speech(app):
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="xoai-stt-"))
    rnd = random.Random(1)
    write_wav(tmp / "silence.wav", [0.0] * 32000)
    write_wav(tmp / "hiss.wav", [rnd.gauss(0, 0.004) for _ in range(32000)])
    for r in run({"cmd": "transcribe", "wav": str(tmp / "silence.wav")},
                 {"cmd": "transcribe", "wav": str(tmp / "hiss.wav")}):
        assert r["event"] == "text" and r["silent"] and r["text"] == "", r


def test_3_start_stop_with_a_fake_microphone(app):
    env = dict(os.environ, XOURNALAI_STT_FAKE_MIC=str(SAMPLE))
    lines = "".join(json.dumps(c) + "\n" for c in [{"cmd": "start"}, {"cmd": "stop"}, {"cmd": "quit"}])
    out = subprocess.run([str(BIN), "--model", str(MODEL)], input=lines, capture_output=True, text=True,
                         timeout=120, env=env)
    answers = [json.loads(l) for l in out.stdout.splitlines()]
    assert [a["event"] for a in answers] == ["ready", "listening", "text"], answers
    assert "ask" in answers[2]["text"].lower()
