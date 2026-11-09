"""E15: 🎙 voice control (the owner's rule, 2026-11-09): nothing is written on the document; what is heard pops up for
a moment; without «Просьбы по слову» every phrase goes to the serving session as a dialogue that may hold commands;
with it only a phrase starting with the trigger phrase («Hey Assistant», «Эй, ассистент») is a command, the rest is
context. The region the owner draws in while speaking goes with the phrase. A zone the agent opens lasts until it ends
it; its short answers are tips (thinking op=say)."""

import json
import pathlib
import subprocess
import time

from test_e15_live_notes import AGENT, DIR, wait, wav

LOG = DIR / "voice-agent.log"
TEXTS = DIR / "voice-texts.json"
RECOGNIZER = DIR / "voice-recognizer.py"
COUNTER = DIR / "voice-n.txt"
RECOGNIZER.write_text(f"""import json, os, sys
texts = json.load(open({str(TEXTS)!r}))
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake voice", "audio": "path"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    n = int(open({str(COUNTER)!r}).read()) if os.path.exists({str(COUNTER)!r}) else 0  # (across recordings)
    open({str(COUNTER)!r}, "w").write(str(n + 1))
    text = texts[n] if n < len(texts) else "…"
    print(json.dumps({{"type": "result", "id": req["id"], "text": text, "language": "ru"}}), flush=True)
""")
TEXTS.write_text(json.dumps(["Вот здесь у нас формула Шрёдингера", "Эй, ассистент, добавь страницу с сеткой",
                             "Hey assistants are everywhere now", "выдели её красным", "подпиши оси"],
                            ensure_ascii=False))
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def lines(prefix):
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if prefix in l]


def commands():
    return lines("Voice command (recording")


def dialogue():
    return lines("Voice (recording")


def heard(c):
    return c.call("live_notes", op="status").get("heard", "")


def written(c, n):
    return wait(lambda: c.call("live_notes", op="status")["written"] >= n)


def state(app, trigger):
    f = pathlib.Path(app.config_file).parent / "ai-live-modes.json"
    f.write_text(json.dumps({"last_mode": "voice", "options": {"trigger": trigger}}))


def hook(app, event):
    subprocess.run([app.binary, "--ai-hook", event], input='{"session_id": "s"}', text=True, timeout=20, check=True,
                   env=dict(app.app_env(), XOURNALAI_PORT=str(app.port)))


def test_1_with_the_trigger_phrase_on_talk_is_context_and_pops_up(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    state(app, True)
    pages = len(c.call("doc_info")["pages"])
    c.call("test_live_audio", op="start", name="voice.ogg", mode="voice")
    c.call("test_live_audio", op="feed", file=wav(DIR / "v1.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    assert written(c, 1)
    assert heard(c) == "🎙 Вот здесь у нас формула Шрёдингера", heard(c)
    time.sleep(0.8)
    assert commands() == [] and dialogue() == [] and len(c.call("doc_info")["pages"]) == pages
    st = c.call("live_notes", op="status")
    assert st["mode"] == "voice" and st["commands"] == 0 and st["pages"] == [], st


def test_2_the_trigger_phrase_makes_a_command_with_the_talk_before(app):
    c = app.client()
    c.call("test_live_audio", op="feed", file=wav(DIR / "v2.wav", [("voice", 2), ("noise", 2)]))
    assert wait(lambda: commands(), 10), LOG.read_text() if LOG.exists() else ""
    line = commands()[-1]
    assert "«добавь страницу с сеткой»" in line and "Said since the last one: «Вот здесь у нас формула" in line, line
    assert "voice-control skill" in line, line
    assert heard(c) == "▶ добавь страницу с сеткой", heard(c)
    # a phrase that only starts like the trigger is not it
    c.call("test_live_audio", op="feed", file=wav(DIR / "v3.wav", [("voice", 2), ("noise", 2)]))
    assert written(c, 3)
    time.sleep(0.8)
    assert len(commands()) == 1, commands()
    c.call("test_recording", file=str(DIR / "voice.ogg"), name="voice.ogg", duration_ms=12000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])
    r = c.call("live_notes", op="read", since=0)
    assert [p["kind"] for p in r["pieces"]] == ["context", "command", "context"], r["pieces"]


def test_3_without_it_every_phrase_is_a_dialogue_with_the_region_drawn_meanwhile(app):
    c = app.client()
    state(app, False)
    c.call("test_live_audio", op="start", name="talk.ogg", mode="voice")
    w = [x for x in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if x["type"] == "GtkXournal"][0]
    x, y, ww, hh = w["bbox"]
    for yy in range(y + 30, y + hh - 20, 50):  # (he draws while he speaks; as test_e7 draws: some land on the page)
        app.user_drag([(x + 10, yy), (x + ww // 2, yy), (x + ww - 10, yy)])
    c.call("test_live_audio", op="feed", file=wav(DIR / "v4.wav", [("noise", 0.5), ("voice", 2), ("noise", 2)]))
    assert wait(lambda: dialogue(), 10), LOG.read_text() if LOG.exists() else ""
    line = dialogue()[-1]
    assert "Voice (recording talk.ogg, n 1, " in line and "«выдели её красным»" in line, line
    assert "if this holds a command, carry it out" in line and "only talk" in line, line
    assert "While saying it, the owner drew on page 1 around [" in line, line
    assert heard(c) == "🎙 выдели её красным", heard(c)


def test_4_an_agent_s_zone_lasts_and_its_answer_is_a_tip(app):
    c = app.client()
    z = c.call("thinking", op="start", page=1, region=[40, 40, 300, 200], text="подписываю оси на графике справа")["id"]
    hook(app, "UserPromptSubmit")
    hook(app, "Stop")  # (its turn ends after each phrase in voice control: the zone stays)
    time.sleep(0.6)
    zones = {x["id"]: x for x in c.call("thinking_list")["zones"]}
    assert zones[z]["state"] == "thinking", zones
    c.call("thinking", op="say", text="Подписал оси; какой шрифт?", page=1, region=[40, 40, 300, 200])
    assert heard(c) == "💬 Подписал оси; какой шрифт?", heard(c)
    c.call("thinking", op="done", id=z)
    c.call("test_recording", file=str(DIR / "talk.ogg"), name="talk.ogg", duration_ms=6000)
