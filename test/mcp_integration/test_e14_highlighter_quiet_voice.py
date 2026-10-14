"""E14: talking keeps the AI highlighter on, even through a quiet built-in microphone (speech at ~0.01 RMS, which a
fixed threshold of 0.03 never heard); the last 5 s before it finishes by itself are counted down on Done."""

import math
import pathlib
import tempfile
import time
import wave
import struct

from test_e10_ask import APP_CONFIG, MODEL, ready, widget  # noqa: F401
from test_e10_stt import SAMPLE, write_wav
from test_e14_ai_highlighter import stroke


def quiet_speech(seconds=16.0, rms=0.01):
    """The sample speech, repeated to `seconds` and scaled so that its loud parts average `rms`."""
    with wave.open(str(SAMPLE)) as w:
        raw = w.readframes(w.getnframes())
    s = [v / 32768 for v in struct.unpack("<%dh" % (len(raw) // 2), raw)]
    loud = sorted(abs(v) for v in s)[len(s) // 2:]
    level = math.sqrt(sum(v * v for v in loud) / len(loud))
    n = int(seconds * 16000)
    return [s[i % len(s)] * rms / level for i in range(n)]


MIC = pathlib.Path(tempfile.mkdtemp(prefix="xoai-quiet-mic-")) / "mic.wav"
write_wav(MIC, quiet_speech())
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_FAKE_MIC": str(MIC),
           "XOURNALAI_STT_MODELS": str(MODEL.parent)}


def test_1_quiet_talking_keeps_it_on_then_it_counts_down(app):
    c = app.client()
    ready(c)
    before = c.call("app_status")["ask"].get("last")
    c.call("action_run", action="win.ai-highlighter", state=True)
    stroke(c, 220, 300, 380)  # (starts the "microphone": 16 s of quiet speech)
    t0 = time.time()
    label = lambda: (widget(c, "aiHighlightDone") or {}).get("label") or ""
    # 10 s without marks: still on, because of the voice (and no countdown while it talks)
    while time.time() - t0 < 13:
        assert c.call("app_status")["ask"].get("last") == before, f"finished at {time.time() - t0:.1f} s while talking"
        assert not label().startswith("Done in"), f"counting down at {time.time() - t0:.1f} s while talking"
        time.sleep(0.3)
    # after the speech ends: the countdown, then Done by itself
    counted = False
    while time.time() - t0 < 40 and c.call("app_status")["ask"].get("last") == before:
        counted = counted or label().startswith("Done in")
        time.sleep(0.2)
    assert counted
    assert c.call("app_status")["ask"].get("last") != before
