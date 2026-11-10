"""E15: «Hold to talk» («Держи и говори», the owner's wish, 2026-11-09): a button next to Record, always there (recording or not, in
any mode). Held, it records on its own; let go, the phrase goes to the speech recognizer (the live notes' one) and,
as a command, to the serving session, with the region the owner drew in meanwhile. Here without a microphone: a test
hook presses it and feeds the sound."""

import json
import time

from test_e15_live_notes import AGENT, DIR, wait, wav

LOG = DIR / "talk-agent.log"
RECOGNIZER = DIR / "talk-recognizer.py"
RECOGNIZER.write_text("""import json, sys
print(json.dumps({"type": "hello", "protocol": 1, "engine": "fake talk", "audio": "path"}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    print(json.dumps({"type": "result", "id": req["id"], "text": "нарисуй рядом график синуса", "language": "ru"}),
          flush=True)
""")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def commands():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Voice command («Hold to talk»" in l]


def test_1_the_button_is_next_to_record_without_any_recording(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    ws = widgets(c)
    assert ws["aiVoiceTalkButton"].get("visible", True) and ws["aiRecordButton"].get("visible", True), ws.get(
        "aiVoiceTalkButton")
    assert c.call("live_notes", op="status")["active"] is False  # (nothing records)


def test_2_held_it_records_and_the_phrase_becomes_a_command(app):
    c = app.client()
    w = [x for x in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if x["type"] == "GtkXournal"][0]
    x, y, ww, hh = w["bbox"]
    c.call("test_talk", op="press")
    for yy in range(y + 30, y + hh - 20, 50):  # (he draws while he speaks)
        app.user_drag([(x + 10, yy), (x + ww // 2, yy), (x + ww - 10, yy)])
    c.call("test_talk", op="feed", file=wav(DIR / "t1.wav", [("voice", 2), ("noise", 0.5)]))
    c.call("test_talk", op="release")
    assert wait(lambda: commands(), 15), LOG.read_text() if LOG.exists() else ""
    line = commands()[-1]
    assert "«нарисуй рядом график синуса»" in line and "voice-control skill" in line, line
    assert "While saying it, the owner drew on page " in line, line


def test_3_a_tap_is_not_a_phrase(app):
    c = app.client()
    n = len(commands())
    c.call("test_talk", op="press")
    c.call("test_talk", op="feed", file=wav(DIR / "t2.wav", [("voice", 0.1)]))
    c.call("test_talk", op="release")
    time.sleep(1.5)
    assert len(commands()) == n  # (0.1 s: nothing sent)
