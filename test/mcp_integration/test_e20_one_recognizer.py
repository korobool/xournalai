"""E20: one speech recognition for every tool (1.21.0; the owner: «если доступна онлайн модель — используют её в
мультиязычном режиме, если нет — все используют локальный фолбэк на английском»). The AI lasso's and highlighter's
dictation (SpeechToText), «Hold to talk» and live notes: the configured remote transcriber first; when it fails, isn't
there or doesn't answer in time, the same audio goes to the local model. Here a fake remote transcriber that answers,
fails or keeps silent, and the real local helper with its English model on the jfk sample."""

import json
import time

from test_e10_stt import MODEL, SAMPLE
from test_e15_live_notes import AGENT, DIR, wait

LOG = DIR / "one-agent.log"
MODE = DIR / "remote-mode.txt"   # ok | error | silent
REMOTE_LOG = DIR / "remote-requests.log"
REMOTE = DIR / "one-remote.py"
REMOTE.write_text(f"""import json, sys
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake remote", "audio": "path"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    open({str(REMOTE_LOG)!r}, "a").write(line)
    mode = open({str(MODE)!r}).read().strip()
    if mode == "ok":
        print(json.dumps({{"type": "result", "id": req["id"], "text": "Привет из удалённой модели",
                           "language": "ru"}}), flush=True)
    elif mode == "error":
        print(json.dumps({{"type": "error", "id": req["id"], "message": "the server is down"}}), flush=True)
""")
MODE.write_text("ok")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_FAKE_MIC": str(SAMPLE),
           "XOURNALAI_STT_MODELS": str(MODEL.parent)}
APP_CONFIG = {"assistant": {"speech": True, "speech_model": MODEL.name[len("ggml-"):-len(".bin")],
                            "autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {REMOTE}"}}}}


def speech(c):
    return c.call("app_status")["speech"]


def ready(c):
    return wait(lambda: speech(c)["state"] == "ready", 60)


def dictate(c):
    assert c.call("test_speech", op="start")["state"] == "listening"
    time.sleep(3)  # ("speaking": the fake microphone plays the jfk sample in real time)
    return c.call("test_speech", op="stop", timeout=60)


def commands():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Voice command («Hold to talk»" in l]


def test_1_dictation_goes_to_the_remote_transcriber_first(app):
    c = app.client()
    assert ready(c), speech(c)
    MODE.write_text("ok")
    r = dictate(c)
    assert not r["silent"] and r["text"] == "Привет из удалённой модели", r  # (multilingual: Russian)
    assert REMOTE_LOG.exists() and "ask-" in REMOTE_LOG.read_text()


def test_2_a_failing_remote_gives_way_to_the_local_model(app):
    c = app.client()
    ready(c)
    MODE.write_text("error")
    r = dictate(c)
    assert not r["silent"] and "americans" in r["text"].lower(), r  # (the local English model on the same audio)
    assert wait(lambda: speech(c)["state"] == "ready", 10)


def test_3_a_remote_that_keeps_silent_gives_way_in_time(app):
    c = app.client()
    ready(c)
    MODE.write_text("silent")
    t = time.time()
    r = dictate(c)
    assert "americans" in r["text"].lower() and time.time() - t < 3 + 10 + 25, r


def test_4_hold_to_talk_falls_back_too(app):
    c = app.client()
    MODE.write_text("error")
    time.sleep(1)  # (the fake agent)
    n = len(commands())
    c.call("test_talk", op="press")
    c.call("test_talk", op="feed", file=str(SAMPLE))
    c.call("test_talk", op="release")
    assert wait(lambda: len(commands()) > n, 40), LOG.read_text() if LOG.exists() else ""
    assert "americans" in commands()[-1].lower(), commands()[-1]


def test_5_live_notes_fall_back_too(app):
    c = app.client()
    MODE.write_text("error")
    c.call("test_live_audio", op="start", name="one.ogg", mode="text")
    c.call("test_live_audio", op="feed", file=str(SAMPLE))
    # (the sample ends in speech: its last piece goes when the recording ends)
    c.call("test_recording", file=str(DIR / "one.ogg"), name="one.ogg", duration_ms=12000)
    assert wait(lambda: c.call("live_notes", op="status")["written"] >= 1, 60), c.call("live_notes", op="status")
    text = json.dumps(c.call("live_notes", op="read", since=0), ensure_ascii=False).lower()
    assert "americans" in text or "country" in text, text[:800]
    MODE.write_text("ok")
