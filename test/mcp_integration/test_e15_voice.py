"""E15: 🎙 voice control (1.17.1, the owner's mode): nothing is written on the document; what is heard pops up for a
moment; a piece said while the big talk button is held, or (with «Просьбы по слову») one starting with a trigger word,
is a command and wakes the serving session with the context said since the last command; the rest is context."""

import json
import pathlib
import time

from test_e15_live_notes import AGENT, DIR, wait, wav

LOG = DIR / "voice-agent.log"
TEXTS = DIR / "voice-texts.json"
RECOGNIZER = DIR / "voice-recognizer.py"
RECOGNIZER.write_text(f"""import json, sys
texts = json.load(open({str(TEXTS)!r}))
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake voice", "audio": "path"}}), flush=True)
n = 0
for line in sys.stdin:
    req = json.loads(line)
    text = texts[n] if n < len(texts) else "…"
    n += 1
    print(json.dumps({{"type": "result", "id": req["id"], "text": text, "language": "ru"}}), flush=True)
""")
TEXTS.write_text(json.dumps(["Вот здесь у нас формула Шрёдингера", "выдели её красным",
                             "Клод, добавь страницу с сеткой", "клодовый суп был вкусный"], ensure_ascii=False))
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def commands():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Voice command (recording" in l]


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def heard(c):
    return c.call("live_notes", op="status").get("heard", "")


def written(c, n):
    return wait(lambda: c.call("live_notes", op="status")["written"] >= n)


def test_1_no_page_and_what_is_heard_pops_up_as_context(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    state = pathlib.Path(app.config_file).parent / "ai-live-modes.json"
    state.write_text(json.dumps({"last_mode": "voice", "options": {"trigger": True}}))
    pages = len(c.call("doc_info")["pages"])
    c.call("test_live_audio", op="start", name="voice.ogg", mode="voice")
    ws = widgets(c)
    assert ws["aiVoiceTalkButton"].get("visible", True) and not ws["aiLiveCutButton"].get("visible", False), ws.get(
        "aiLiveCutButton")
    c.call("test_live_audio", op="feed", file=wav(DIR / "v1.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    assert written(c, 1)
    assert heard(c) == "🎙 Вот здесь у нас формула Шрёдингера", heard(c)
    time.sleep(0.8)
    assert commands() == [] and len(c.call("doc_info")["pages"]) == pages  # (context; nothing on the document)
    st = c.call("live_notes", op="status")
    assert st["mode"] == "voice" and st["commands"] == 0 and st["pages"] == [], st


def test_2_held_talk_button_makes_a_command_with_the_context_before(app):
    c = app.client()
    x, y, w, h = widgets(c)["aiVoiceTalkButton"]["bbox"]
    app._xdo("mousemove", x + w // 2, y + h // 2)
    app._xdo("mousedown", 1)
    time.sleep(0.4)
    assert wait(lambda: c.call("live_notes", op="status")["talking"], 3)
    c.call("test_live_audio", op="feed", file=wav(DIR / "v2.wav", [("voice", 2)]))
    time.sleep(0.2)
    app._xdo("mouseup", 1)
    time.sleep(0.3)
    # (the cut acts on the next sound frame: a real recording goes on; here a moment of quiet)
    c.call("test_live_audio", op="feed", file=wav(DIR / "v2b.wav", [("noise", 0.3)]))
    assert wait(lambda: commands(), 10), LOG.read_text() if LOG.exists() else ""
    line = commands()[-1]
    assert "Voice command (recording voice.ogg, n 2, " in line and "«выдели её красным»" in line, line
    assert "Context since the last command: «Вот здесь у нас формула Шрёдингера»." in line, line
    assert "voice-control skill" in line, line
    assert heard(c) == "▶ выдели её красным", heard(c)
    assert not c.call("live_notes", op="status")["talking"]


def test_3_a_trigger_word_makes_a_command_too(app):
    c = app.client()
    c.call("test_live_audio", op="feed", file=wav(DIR / "v3.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    assert wait(lambda: len(commands()) == 2, 10), commands()
    line = commands()[-1]
    assert "«добавь страницу с сеткой»" in line and "Context since the last command: none." in line, line
    # a word that only starts like the trigger is not it
    c.call("test_live_audio", op="feed", file=wav(DIR / "v4.wav", [("voice", 2), ("noise", 2)]))
    assert written(c, 4)
    time.sleep(0.8)
    assert len(commands()) == 2, commands()


def test_4_the_pieces_say_what_they_were_and_the_end_wakes_nobody(app):
    c = app.client()
    r = c.call("live_notes", op="read", since=0)
    assert [p["kind"] for p in r["pieces"]] == ["context", "command", "command", "context"], r["pieces"]
    assert r["commands"] == 2
    n = len(commands())
    c.call("test_recording", file=str(DIR / "voice.ogg"), name="voice.ogg", duration_ms=20000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])
    time.sleep(1)
    assert len(commands()) == n and "voice.ogg" not in "".join(
        l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Live notes (recording" in l)
    assert not widgets(c)["aiVoiceTalkButton"].get("visible", False)
