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
# the helper's diagnostics (speech.log, last-silent.wav) go to a scratch cache, never the owner's
CACHE = tempfile.mkdtemp(prefix="xoai-stt-cache-")
os.makedirs(os.path.join(CACHE, "xournalai"), exist_ok=True)
os.environ["XDG_CACHE_HOME"] = CACHE
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
    import time
    env = dict(os.environ, XOURNALAI_STT_FAKE_MIC=str(SAMPLE))
    p = subprocess.Popen([str(BIN), "--model", str(MODEL)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
                         env=env)
    p.stdin.write(json.dumps({"cmd": "start"}) + "\n"), p.stdin.flush()
    time.sleep(3)  # "speaking" (the fake microphone plays in real time)
    out, _ = p.communicate(json.dumps({"cmd": "stop"}) + "\n" + json.dumps({"cmd": "quit"}) + "\n", timeout=60)
    answers = [json.loads(l) for l in out.splitlines() if '"level"' not in l]
    assert [a["event"] for a in answers] == ["ready", "listening", "text"], answers
    assert "americans" in answers[2]["text"].lower(), answers


def read_wav(path):
    with wave.open(str(path)) as w:
        frames = w.readframes(w.getnframes())
    return [s / 32768 for s in struct.unpack("<%dh" % (len(frames) // 2), frames)]


def test_4_continuous_and_quiet_speech_are_speech(app):
    # speaking from the first to the last moment (no pause to learn the background from), and speaking softly,
    # were judged silent: "nothing heard" while the level bars moved
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="xoai-stt-"))
    pcm = read_wav(SAMPLE)
    speech = pcm[int(0.35 * 16000):int(2.3 * 16000)]  # "And so my fellow Americans", no silence around it
    write_wav(tmp / "continuous.wav", speech)
    write_wav(tmp / "quiet.wav", [s * 0.1 for s in pcm[: 5 * 16000]])  # -20 dB
    for r in run({"cmd": "transcribe", "wav": str(tmp / "continuous.wav")},
                 {"cmd": "transcribe", "wav": str(tmp / "quiet.wav")}):
        assert not r["silent"] and "americans" in r["text"].lower(), r


def test_5_the_microphone_opens_at_once_after_a_previous_request(app):
    # a new "start" used to wait until the previous request was transcribed: the start of the phrase was lost
    import threading
    env = dict(os.environ, XOURNALAI_STT_FAKE_MIC=str(SAMPLE))
    p = subprocess.Popen([str(BIN), "--model", str(MODEL)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
                         env=env, bufsize=1)
    events = []

    def reader():
        for line in p.stdout:
            e = json.loads(line)
            if e["event"] != "level":
                events.append(e["event"])

    threading.Thread(target=reader, daemon=True).start()
    send = lambda c: (p.stdin.write(json.dumps({"cmd": c}) + "\n"), p.stdin.flush())
    import time
    time.sleep(0.5)
    send("start"), time.sleep(2.0), send("stop"), time.sleep(0.05), send("start"), time.sleep(1.5), send("stop")
    time.sleep(4)
    send("quit")
    p.wait(timeout=20)
    assert events[:3] == ["ready", "listening", "listening"], events  # the 2nd listening before the 1st text
    assert events.count("text") == 2, events
