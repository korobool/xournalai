"""E24: «🎤 Only my voice · experimental» (1.24.0, the owner, 11 Nov 2026: «Распознавание команд именно моим голосом –
это должна быть опциональная фишка … по умолчанию выключена и просто работает на голос»). Off by default and apart
from «👥 Speakers». When on, the app learns his voice from what he says with «Hold» (fingerprints only, in its own
data folder) and takes spoken commands only in that voice: «Hey Assistant» in another voice wakes nobody. Until his
voice is known, nothing said is a command.

Here the fake recognizer's voices are tones: 220 Hz is the owner, 330 Hz someone else; every phrase is
«Hey Assistant, voice …»."""

import json
import pathlib
import time

from test_e15_live_modes import long_press, state_file, widgets
from test_e15_live_notes import AGENT, DIR, wait
from test_e23_speakers import tones

LOG = DIR / "myvoice-agent.log"
RECOGNIZER = DIR / "myvoice_recognizer.py"
RECOGNIZER.write_text("""import json, math, struct, sys, wave
print(json.dumps({"type": "hello", "protocol": 1, "engine": "fake", "audio": "path",
                  "voices": {"model": "fake-voices", "dim": 8}}), flush=True)
VOICE = {220: [1, 0.1, 0, 0, 0, 0, 0, 0], 330: [0.1, 1, 0, 0, 0, 0, 0, 0]}
def unit(v):
    n = math.sqrt(sum(x * x for x in v)); return [x / n for x in v]
for line in sys.stdin:
    req = json.loads(line)
    with wave.open(req["audio_path"]) as w:
        rate = w.getframerate(); a = struct.unpack(f"<{w.getnframes()}h", w.readframes(w.getnframes()))
    frame = rate // 50
    loud = [a[i:i + frame] for i in range(0, len(a) - frame, frame)
            if math.sqrt(sum(x * x for x in a[i:i + frame]) / frame) >= 600]
    if not loud:
        print(json.dumps({"type": "result", "id": req["id"], "text": ""}), flush=True)
        continue
    zc = sum(1 for j in range(1, frame) if (loud[0][j - 1] < 0) != (loud[0][j] < 0))
    tone = 220 if zc < 11 else 330
    secs = len(loud) * 0.02
    text = f"Hey Assistant, voice {tone}"
    seg = {"start": 0.3, "end": round(0.3 + secs, 2), "text": text}
    result = {"type": "result", "id": req["id"], "text": text, "language": "en", "segments": [seg]}
    if req.get("voices") is True:
        seg["voice"] = unit(VOICE[tone])
        if secs >= 1:
            result["voice"] = unit(VOICE[tone])
    print(json.dumps(result), flush=True)
""")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def wakes(name):
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if f"recording {name}" in l]


def options(app, **opts):
    state_file(app).write_text(json.dumps({"last_mode": "text", "options": {"commands": "trigger", **opts}}))


def owner_file(app):
    return pathlib.Path(app.home) / "data" / "xournalai" / "voice" / "owner.json"


def piece(c, name, tone, n):
    c.call("test_live_audio", op="feed", file=tones(DIR / f"{name}-{n}.wav", [(tone, 2), ("noise", 2)]))
    assert wait(lambda: c.call("live_notes", op="status")["written"] >= n), c.call("live_notes", op="status")


def end(c, name):
    c.call("test_recording", file=str(DIR / name), name=name, duration_ms=12000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])


def test_1_the_option_is_off_by_default_and_explained(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    long_press(app, widgets(c)["aiRecordButton"])
    ws = widgets(c)
    chip = ws["aiRecordOpt-my_voice"]
    assert chip["label"] == "☐ 🎤 Only my voice · experimental", chip
    assert "Experimental" in chip["tooltip"] and "«Hold»" in chip["tooltip"]
    c.call("ui_interact", op="click", target=chip["id"])
    ws = widgets(c)
    assert json.loads(state_file(app).read_text())["options"]["my_voice"] is True
    assert "🎤" in ws["aiRecordMode-conspect-badges"]["label"]
    assert not json.loads(state_file(app).read_text())["options"].get("speakers")  # (apart from 👥 Speakers)
    app.user_key("Escape")


def test_2_while_his_voice_is_unknown_nothing_said_is_a_command(app):
    c = app.client()
    options(app, my_voice=True)
    assert not owner_file(app).exists()
    c.call("test_live_audio", op="start", name="unknown.ogg", mode="text")
    piece(c, "unknown", 220, 1)
    time.sleep(2)
    assert wakes("unknown.ogg") == []  # (his trigger phrase, but his voice isn't known: no command)
    st = c.call("live_notes", op="status")
    assert st["my_voice"]["known"] is False and "not known" in st["my_voice"]["problem"], st
    assert c.call("live_notes", op="read", since=0)["pieces"][0]["owner_text"] == ""
    end(c, "unknown.ogg")


def test_3_hold_teaches_his_voice_fingerprints_only(app):
    c = app.client()
    options(app, my_voice=True)
    c.call("test_talk", op="press")
    c.call("test_talk", op="feed", file=tones(DIR / "hold.wav", [(220, 2)]))
    c.call("test_talk", op="release")
    assert wait(lambda: owner_file(app).exists())
    kept = json.loads(owner_file(app).read_text())
    assert kept["model"] == "fake-voices" and len(kept["fingerprints"]) == 1 and len(kept["fingerprints"][0]) == 8
    assert set(kept) == {"model", "fingerprints"}  # (no sound, no text)


def test_4_with_his_voice_known_only_his_trigger_phrase_commands(app):
    c = app.client()
    options(app, my_voice=True)
    c.call("test_live_audio", op="start", name="known.ogg", mode="text")
    piece(c, "known", 330, 1)  # (someone else: «Hey Assistant, voice 330»)
    time.sleep(2)
    assert wakes("known.ogg") == [], wakes("known.ogg")
    piece(c, "known", 220, 2)  # (him)
    assert wait(lambda: wakes("known.ogg")), LOG.read_text() if LOG.exists() else ""
    assert "owner_text" in wakes("known.ogg")[0]  # (the session is told: only his own voice)
    got = c.call("live_notes", op="read", since=0)["pieces"]
    assert [p["owner_text"] for p in got] == ["", "Hey Assistant, voice 220"], got
    assert c.call("live_notes", op="status")["my_voice"]["known"] is True
    assert "speakers" not in got[0]  # (👥 is off: no labels on the paper)
    end(c, "known.ogg")


def test_5_off_anyone_s_trigger_phrase_commands_as_before(app):
    c = app.client()
    options(app, my_voice=False)
    c.call("test_live_audio", op="start", name="anyone.ogg", mode="text")
    piece(c, "anyone", 330, 1)
    assert wait(lambda: wakes("anyone.ogg"))
    assert "owner_text" not in c.call("live_notes", op="read", since=0)["pieces"][0]
    end(c, "anyone.ogg")


def test_6_voice_only_takes_commands_only_in_his_voice(app):
    c = app.client()
    state_file(app).write_text(json.dumps({"last_mode": "voice", "options": {"commands": "trigger", "my_voice": True}}))
    c.call("test_live_audio", op="start", name="voice.ogg", mode="voice")
    piece(c, "voice", 330, 1)  # (someone else's «Hey Assistant»: context, no command)
    time.sleep(2)
    assert not [l for l in wakes("voice.ogg") if l.startswith("Voice command")], wakes("voice.ogg")
    piece(c, "voice", 220, 2)
    assert wait(lambda: [l for l in wakes("voice.ogg") if "Voice command" in l]), wakes("voice.ogg")
    assert "«voice 220»" in [l for l in wakes("voice.ogg") if "Voice command" in l][0]
    end(c, "voice.ogg")
