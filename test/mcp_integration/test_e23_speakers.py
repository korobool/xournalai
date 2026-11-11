"""E23: «👥 Speakers · experimental» (1.23.0, the owner, 11 Nov 2026: «начинаем работать над экспериментальной
галочкой разделения речи собеседников»). Off by default. When on, each piece asks the recognizer for voice
fingerprints (docs/mcp/TRANSCRIBERS.md); the app tells the voices apart (A, B … in the order they first speak), writes
each turn as a line of its own with its speaker, in the speaker's colour, and gives the agents who said what. A
recognizer without fingerprints: the transcript goes on without speakers, and says why.

Here the fake recognizer's "voices" are tones: 220 Hz is one voice, 330 Hz another."""

import json
import math
import pathlib
import random
import struct
import time

from test_e15_live_modes import long_press, state_file, widgets
from test_e15_live_notes import AGENT, DIR, texts, wait

LOG = DIR / "speakers-agent.log"
REC_LOG = DIR / "speakers-recognizer.log"
RECOGNIZER = DIR / "voices_recognizer.py"
RECOGNIZER.write_text(f"""import json, math, struct, sys, wave
NO_VOICES = "--no-voices" in sys.argv
log = open({str(REC_LOG)!r}, "a")
hello = {{"type": "hello", "protocol": 1, "engine": "fake recognizer with voices", "audio": "path"}}
if not NO_VOICES:
    hello["voices"] = {{"model": "fake-voices", "dim": 8}}
print(json.dumps(hello), flush=True)
VOICE = {{220: [1, 0.1, 0, 0, 0, 0, 0, 0], 330: [0.1, 1, 0, 0, 0, 0, 0, 0]}}
def unit(v):
    n = math.sqrt(sum(x * x for x in v)); return [x / n for x in v]
for line in sys.stdin:
    req = json.loads(line)
    log.write(line); log.flush()
    with wave.open(req["audio_path"]) as w:
        rate = w.getframerate(); a = struct.unpack(f"<{{w.getnframes()}}h", w.readframes(w.getnframes()))
    frame = rate // 50
    runs = []  # (tone, start, end): 20 ms frames, voiced when loud, the tone from its zero crossings
    for i in range(0, len(a) - frame, frame):
        f = a[i:i + frame]
        if math.sqrt(sum(x * x for x in f) / len(f)) < 600:
            continue
        zc = sum(1 for j in range(1, len(f)) if (f[j - 1] < 0) != (f[j] < 0))
        tone = 220 if zc < 11 else 330
        t = i / rate
        if runs and runs[-1][0] == tone and t - runs[-1][2] < 0.05:
            runs[-1][2] = t + 0.02
        else:
            runs.append([tone, t, t + 0.02])
    segs = [{{"start": round(s, 2), "end": round(e, 2), "text": f"voice {{tone}} for {{e - s:.0f}} s"}}
            for tone, s, e in runs if e - s >= 0.3]
    result = {{"type": "result", "id": req["id"], "text": " ".join(s["text"] for s in segs), "language": "en",
               "segments": segs}}
    if req.get("voices") is True and not NO_VOICES:
        for s, (tone, _, _) in zip(segs, [r for r in runs if r[2] - r[1] >= 0.3]):
            s["voice"] = unit(VOICE[tone])
    print(json.dumps(result), flush=True)
""")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream",
                                                           "command": f"python3 -u {RECOGNIZER}"}}}}


def tones(path, parts, rate=16000):
    """parts: [("noise" | 220 | 330, seconds)]: a quiet microphone, or a voice (a tone)"""
    rnd = random.Random(1)
    samples = []
    for kind, sec in parts:
        for i in range(int(sec * rate)):
            v = 0.1 * math.sin(2 * math.pi * kind * i / rate) if kind != "noise" else rnd.uniform(-1, 1) * 0.0017
            samples.append(int(v * 32767))
    data = struct.pack(f"<{len(samples)}h", *samples)
    pathlib.Path(path).write_bytes(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
                                   struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" +
                                   struct.pack("<I", len(data)) + data)
    return str(path)


def set_option(app, key, on):
    f = state_file(app)
    st = json.loads(f.read_text()) if f.exists() else {"last_mode": "conspect", "options": {}}
    st["options"][key] = on
    f.write_text(json.dumps(st))


def requests():
    return [json.loads(l) for l in REC_LOG.read_text().splitlines()] if REC_LOG.exists() else []


def end(c, name):
    c.call("test_recording", file=str(DIR / name), name=name, duration_ms=8000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])


def set_recognizer(app, command):
    cfg = json.loads(pathlib.Path(app.config_file).read_text())
    cfg["assistant"]["live_notes"]["transcriber"]["command"] = command
    pathlib.Path(app.config_file).write_text(json.dumps(cfg))


def test_1_the_option_is_off_by_default_and_explained(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    long_press(app, widgets(c)["aiRecordButton"])
    ws = widgets(c)
    chip = ws["aiRecordOpt-speakers"]
    assert chip["label"] == "☐ 👥 Speakers · experimental", chip
    assert "Experimental" in chip["tooltip"] and "A, B" in chip["tooltip"]
    c.call("ui_interact", op="click", target=chip["id"])
    ws = widgets(c)
    assert json.loads(state_file(app).read_text())["options"]["speakers"] is True
    assert "👥" in ws["aiRecordMode-conspect-badges"]["label"]
    assert "👥" not in ws["aiRecordMode-voice-badges"]["label"]  # (🎙 has no transcript to label)
    c.call("ui_interact", op="click", target=ws["aiRecordOpt-speakers"]["id"])  # (off again)
    assert json.loads(state_file(app).read_text())["options"]["speakers"] is False
    app.user_key("Escape")


def test_2_off_no_fingerprints_are_asked_for_and_no_speakers_written(app):
    c = app.client()
    set_option(app, "speakers", False)
    c.call("test_live_audio", op="start", name="plain.ogg", mode="conspect")
    c.call("test_live_audio", op="feed", file=tones(DIR / "plain.wav", [("noise", 1), (220, 2), ("noise", 2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 1 else None)(c.call("live_notes", op="status")))
    assert st and "speakers" not in st, st
    assert requests() and "voices" not in requests()[-1]
    page = st["pages"][0]["page"]
    assert [s for s in texts(c, page) if s.startswith("[0:00] voice 220")], texts(c, page)
    end(c, "plain.ogg")


def test_3_two_voices_two_letters_each_turn_a_line_in_its_colour(app):
    c = app.client()
    set_option(app, "speakers", True)
    c.call("test_live_audio", op="start", name="talk.ogg", mode="conspect")
    c.call("test_live_audio", op="feed", file=tones(DIR / "t1.wav", [("noise", 1), (220, 2), (330, 2), ("noise", 2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 1 else None)(c.call("live_notes", op="status")))
    assert st and st["speakers"]["voices"] == "fake-voices" and st["speakers"]["heard"] == ["A", "B"], st
    assert requests()[-1].get("voices") is True
    c.call("test_live_audio", op="feed", file=tones(DIR / "t2.wav", [(330, 2), ("noise", 2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 2 else None)(c.call("live_notes", op="status")))
    assert st, c.call("live_notes", op="status")
    page = st["pages"][0]["page"]
    lines = {e["text"]: e["color"] for e in c.call("page_elements", page=page, detail="full")["elements"]
             if e.get("type") == "text" and e.get("text", "").startswith("[")}
    assert list(lines) == ["[0:01] A: voice 220 for 2 s", "[0:03] B: voice 330 for 2 s", "[0:07] B: voice 330 for 2 s"], lines
    assert lines["[0:01] A: voice 220 for 2 s"].lower().startswith("#1f5fa8")
    assert lines["[0:03] B: voice 330 for 2 s"].lower().startswith("#2e7d32")
    pieces = c.call("live_notes", op="read", since=0)["pieces"]
    assert [t["speaker"] for t in pieces[0]["speakers"]] == ["A", "B"] and "speaker" not in pieces[0], pieces[0]
    assert pieces[1]["speaker"] == "B" and pieces[1]["speakers"][0]["text"] == "voice 330 for 2 s", pieces[1]
    assert wait(lambda: "speakers" in (LOG.read_text() if LOG.exists() else ""))  # (named in the wake-up line)
    end(c, "talk.ogg")


def test_4_a_recognizer_without_fingerprints_goes_on_without_speakers(app):
    c = app.client()
    set_option(app, "speakers", True)
    set_recognizer(app, f"python3 -u {RECOGNIZER} --no-voices")
    c.call("test_live_audio", op="start", name="novoice.ogg", mode="conspect")
    c.call("test_live_audio", op="feed", file=tones(DIR / "n1.wav", [("noise", 1), (220, 2), ("noise", 2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 1 else None)(c.call("live_notes", op="status")))
    assert st and st["speakers"]["voices"] is None and "no voice fingerprints" in st["speakers"]["problem"], st
    assert "voices" not in requests()[-1]  # (not asked: it offered none)
    page = st["pages"][0]["page"]
    assert [s for s in texts(c, page) if s.startswith("[0:00] voice 220")], texts(c, page)  # (the text as before)
    end(c, "novoice.ogg")
