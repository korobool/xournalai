"""E10: the app runs the speech helper (warm, restarted when needed) and gets transcripts from it."""

import time

from test_e10_stt import MODEL, SAMPLE

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_FAKE_MIC": str(SAMPLE),
           "XOURNALAI_STT_MODELS": str(MODEL.parent)}
APP_CONFIG = {"assistant": {"speech": True, "speech_model": MODEL.name[len("ggml-"):-len(".bin")]}}


def speech(c):
    return c.call("app_status")["speech"]


def wait_state(c, want, timeout=20):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if speech(c)["state"] in want:
            return speech(c)["state"]
        time.sleep(0.2)
    return speech(c)


def test_1_the_helper_is_warm_soon_after_start(app):
    c = app.client()
    assert wait_state(c, ("ready",)) == "ready", speech(c)


def test_2_start_stop_gives_the_transcript(app):
    c = app.client()
    wait_state(c, ("ready",))
    assert c.call("test_speech", op="start")["state"] == "listening"
    time.sleep(3)  # "speaking" (the fake microphone plays in real time)
    r = c.call("test_speech", op="stop")
    assert not r["silent"] and "americans" in r["text"].lower(), r
    assert wait_state(c, ("ready",)) == "ready"


def test_3_autostart_stays_off_in_tests(app):
    # the harness merges a test's own assistant settings: it must never start a real Claude Code
    import json
    cfg = json.loads(app.config_file.read_text())
    assert cfg["assistant"]["autostart"] is False and cfg["assistant"]["speech"] is True


def test_4_a_late_transcript_does_not_hide_a_new_recording(app):
    c = app.client()
    wait_state(c, ("ready",))
    c.call("test_speech", op="start")
    time.sleep(2.5)
    import threading
    first = {}
    threading.Thread(target=lambda: first.update(c.call("test_speech", op="stop"))).start()
    time.sleep(0.05)
    c2 = app.client()
    c2.call("test_speech", op="start")  # while the first one is being transcribed
    deadline = time.time() + 10
    while time.time() < deadline and not first:
        time.sleep(0.1)
    assert first and not first["silent"], first
    assert speech(c2)["state"] == "listening"  # the first transcript arrived, the new recording goes on
    time.sleep(2)
    r = c2.call("test_speech", op="stop")
    assert not r["silent"], r
