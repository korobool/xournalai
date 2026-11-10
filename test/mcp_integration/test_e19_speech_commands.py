"""E19: the switch «Speech commands» (1.20.0; the brainstorm of 10 Nov: the Record menu's tiles say what appears on
paper, one switch says whether his speech may steer things: 🛡 off, 🔤 only «Hey Assistant», 💬 conversation). With 📝
Transcript and 🔤 the app itself wakes the session only for a phrase that starts with the trigger phrase; during a
recording a chip next to ✂ shows the level and changes it at once."""

import json
import os
import pathlib
import time

from test_e15_live_notes import AGENT, DIR, wait, wav

LOG = DIR / "commands-agent.log"
TEXTS = DIR / "commands-texts.json"
RECOGNIZER = DIR / "commands-recognizer.py"
COUNTER = DIR / "commands-n.txt"
RECOGNIZER.write_text(f"""import json, os, sys
texts = json.load(open({str(TEXTS)!r}))
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake phrases", "audio": "path"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    n = int(open({str(COUNTER)!r}).read()) if os.path.exists({str(COUNTER)!r}) else 0
    open({str(COUNTER)!r}, "w").write(str(n + 1))
    text = texts[n] if n < len(texts) else "и ещё немного"
    print(json.dumps({{"type": "result", "id": req["id"], "text": text, "language": "ru"}}), flush=True)
""")
TEXTS.write_text(json.dumps(["Вот здесь у нас формула Шрёдингера", "Hey Assistant, подпиши оси",
                             "просто думаю вслух", "и ещё немного"], ensure_ascii=False))
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}
PIECE = [("voice", 2), ("noise", 2)]


def wakes():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Live notes (recording" in l]


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def state(app, mode, commands):
    f = pathlib.Path(app.config_file).parent / "ai-live-modes.json"
    f.write_text(json.dumps({"last_mode": mode, "options": {"commands": commands}}))


def written(c, n):
    return wait(lambda: c.call("live_notes", op="status")["written"] >= n, 15)


def test_1_a_transcript_with_hey_assistant_wakes_only_for_that_phrase(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    state(app, "text", "trigger")
    before = len(wakes())
    c.call("test_live_audio", op="start", name="trig.ogg", mode="text")
    c.call("test_live_audio", op="feed", file=wav(DIR / "trig1.wav", [("noise", 1)] + PIECE))
    assert written(c, 1)
    time.sleep(1.5)
    assert len(wakes()) == before, wakes()[before:]  # (only context: nobody woken)
    c.call("test_live_audio", op="feed", file=wav(DIR / "trig2.wav", PIECE))
    assert wait(lambda: len(wakes()) > before, 15), wakes()
    line = wakes()[-1]
    assert "Mode «Transcript» (text: annotate=none, commands=trigger" in line and "trigger words:" in line, line
    assert "modes/drawing.md" in line and "Only a phrase that starts with the trigger phrase is a request." in line
    c.call("live_notes", op="read")
    c.call("test_recording", file=str(DIR / "trig.ogg"), name="trig.ogg", duration_ms=8000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])


def test_2_the_chip_changes_the_level_during_the_recording(app):
    c = app.client()
    state(app, "conspect", "talk")
    c.call("test_live_audio", op="start", name="chip.ogg", mode="conspect")
    ws = wait(lambda: (lambda ws: ws if ws.get("aiLiveLevelChip", {}).get("visible", True) and
                       "aiLiveLevelChip" in ws else None)(widgets(c)), 5)
    assert ws and ws["aiLiveLevelChip"]["label"] == "💬", ws.get("aiLiveLevelChip")
    c.call("ui_interact", op="click", target=ws["aiLiveLevelChip"]["id"])
    ws = wait(lambda: (lambda ws: ws if "aiLiveLevel-off" in ws else None)(widgets(c)), 5)
    assert ws["aiLiveLevel-off"]["label"] == "🛡 Commands off\nothers talking, a video"
    c.call("ui_interact", op="click", target=ws["aiLiveLevel-off"]["id"])  # (someone came in)
    assert wait(lambda: widgets(c)["aiLiveLevelChip"]["label"] == "🛡", 5)
    f = pathlib.Path(app.config_file).parent / "ai-live-modes.json"
    assert json.loads(f.read_text())["options"]["commands"] == "off"  # (remembered)
    before = len(wakes())
    c.call("test_live_audio", op="feed", file=wav(DIR / "chip.wav", [("noise", 1)] + PIECE))
    assert wait(lambda: len(wakes()) > before, 15), wakes()
    line = wakes()[-1]
    assert "Mode «Notes» (conspect: annotate=visual, commands=off" in line and "modes/safe.md" in line, line
    assert "nothing said is ever carried out" in line, line
    c.call("live_notes", op="read")
    c.call("test_recording", file=str(DIR / "chip.ogg"), name="chip.ogg", duration_ms=5000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])
    assert wait(lambda: not widgets(c).get("aiLiveLevelChip", {}).get("visible", False), 5)


def test_3_an_old_state_file_still_means_the_same(app):
    """(written before 1.20.0: the 🛡 tile was a mode, the trigger phrase an option)"""
    c = app.client()
    f = pathlib.Path(app.config_file).parent / "ai-live-modes.json"
    f.write_text(json.dumps({"last_mode": "safe", "options": {"summary": True}}))
    c.call("test_live_audio", op="start", name="old.ogg", mode="")
    st = c.call("live_notes", op="status")
    assert st["mode"] == "conspect", st
    c.call("test_recording", file=str(DIR / "old.ogg"), name="old.ogg", duration_ms=2000)
    wait(lambda: c.call("live_notes", op="status")["done"])
    f.write_text(json.dumps({"last_mode": "voice", "options": {"trigger": True}}))
    # (the menu shows it as 🎙 with 🔤)
    from test_e15_live_modes import long_press
    long_press(app, widgets(c)["aiRecordButton"])
    ws = widgets(c)
    assert ws["aiRecordMode-voice-badges"]["label"].startswith("🔤"), ws["aiRecordMode-voice-badges"]
    app.user_key("Escape")
