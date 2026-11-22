"""E25: speech in Russian and English everywhere (1.25.0; the owner, 22 Nov: «speech recognition in xournalai must
ALWAYS be multilingual (Russian and English well), both remote and local, as the default forever»). The local fallback
is a multilingual model that hears the language (ru or en) itself; the remote recognizer starts as he begins to speak
and gets the time of its cold start, so a phrase does not go to the local model only because the server was waking
up (talk-3, 22 Nov: a Russian «Hold to talk» phrase came back «(speaking in foreign language)»).

The Russian check needs a Russian phrase (16 kHz mono WAV) in XOAI_RU_SAMPLE that says «… нарисуй … схему …» (the
release check makes one); without it the Russian parts run on the English sample and say so."""

import json
import os
import pathlib
import subprocess
import time

from test_e10_stt import BIN, CACHE, MODEL, SAMPLE  # noqa: F401  (CACHE: the helper's diagnostics stay in scratch)
from test_e15_live_notes import AGENT, DIR, wait

ML_MODEL = pathlib.Path(os.environ.get("XOURNALAI_STT_ML_MODEL", MODEL.parent / "ggml-small.bin"))
RU = pathlib.Path(os.environ["XOAI_RU_SAMPLE"]) if os.environ.get("XOAI_RU_SAMPLE") else None
LOG = DIR / "ml-agent.log"
MODE = DIR / "ml-remote-mode.txt"  # ok | error
REMOTE = DIR / "ml-remote.py"
REMOTE.write_text(f"""import json, sys, time
time.sleep(14)  # (a cold start: the owner's server takes ~17 s to load large-v3)
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake remote", "audio": "path"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    if open({str(MODE)!r}).read().strip() == "ok":
        print(json.dumps({{"type": "result", "id": req["id"], "text": "Привет из удалённой модели",
                           "language": "ru"}}), flush=True)
    else:
        print(json.dumps({{"type": "error", "id": req["id"], "message": "the server is down"}}), flush=True)
""")
MODE.write_text("ok")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_MODELS": str(ML_MODEL.parent)}
APP_CONFIG = {"assistant": {"speech": True, "autostart": True,  # (speech_model: the default, multilingual)
                            "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}", "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {REMOTE}"}}}}


def helper(*wavs):
    """The local helper with the multilingual model on these WAVs: its ready line and answers"""
    assert ML_MODEL.exists(), f"speech model missing: {ML_MODEL}"
    lines = "".join(json.dumps({"cmd": "transcribe", "wav": str(w)}) + "\n" for w in wavs) + '{"cmd":"quit"}\n'
    out = subprocess.run([str(BIN), "--model", str(ML_MODEL)], input=lines, capture_output=True, text=True,
                         timeout=600)
    answers = [json.loads(l) for l in out.stdout.splitlines() if l.strip()]
    assert answers and answers[0]["event"] == "ready", out.stdout + out.stderr
    return answers[0], answers[1:]


def commands():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Voice command («Hold to talk»" in l]


def test_1_the_remote_recognizer_starts_with_the_phrase_and_gets_its_cold_start(app):
    c = app.client()
    time.sleep(1)  # (the fake agent)
    MODE.write_text("ok")
    n = len(commands())
    c.call("test_talk", op="press")  # (the remote recognizer starts now: its hello in 14 s)
    time.sleep(6)
    c.call("test_talk", op="feed", file=str(SAMPLE))
    c.call("test_talk", op="release")
    t = time.time()
    assert wait(lambda: len(commands()) > n, 60), LOG.read_text() if LOG.exists() else ""
    assert "Привет из удалённой модели" in commands()[-1], commands()[-1]  # (not the local model after 10 s)
    assert time.time() - t < 13, time.time() - t  # (it was started at the press, not at the release)


def test_2_the_app_uses_a_multilingual_local_model_by_default(app):
    c = app.client()
    assert wait(lambda: c.call("app_status")["speech"]["state"] == "ready", 120), c.call("app_status")["speech"]
    s = c.call("app_status")["speech"]
    assert s["model"] == "small" and s["languages"] == "ru,en", s


def test_3_a_failing_remote_gives_way_to_the_local_model_in_russian(app):
    c = app.client()
    MODE.write_text("error")
    n = len(commands())
    c.call("test_talk", op="press")
    c.call("test_talk", op="feed", file=str(RU or SAMPLE))
    c.call("test_talk", op="release")
    assert wait(lambda: len(commands()) > n, 240), LOG.read_text() if LOG.exists() else ""
    line = commands()[-1].lower()
    if RU:
        assert "нарисуй" in line or "схем" in line, line  # (Russian, not «(speaking in foreign language)»)
    else:
        print("    (no XOAI_RU_SAMPLE: checked on the English sample)")
        assert "americans" in line, line
    MODE.write_text("ok")


def test_4_the_local_helper_hears_russian_and_english(app):  # noqa: ARG001  (the helper alone)
    ready, answers = helper(*([RU] if RU else []), SAMPLE)
    assert ready["multilingual"] is True and ready["languages"] == ["ru", "en"], ready
    en = answers[-1]
    assert en["language"] == "en" and "americans" in en["text"].lower(), en
    if RU:
        ru = answers[0]
        assert ru["language"] == "ru" and not ru["silent"], ru
        assert "нарисуй" in ru["text"].lower() or "схем" in ru["text"].lower(), ru  # (Cyrillic is speech)
    else:
        print("    (no XOAI_RU_SAMPLE: the Russian phrase not checked)")
