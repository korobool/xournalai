"""E11: audio_transcribe — the local, English-only fallback for recordings (the remote transcriber comes first)."""

import pathlib
import shutil
import subprocess
import tempfile
import time

from test_e10_stt import MODEL, SAMPLE

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-rec-"))
REC = DIR / "2026-09-30_10-00-00.ogg"
if shutil.which("ffmpeg"):  # the recorder's format: Ogg Vorbis, 44.1 kHz
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", str(SAMPLE), "-ar", "44100", "-c:a", "libvorbis",
                    str(REC)], check=True)
else:
    REC = DIR / "2026-09-30_10-00-00.wav"
    shutil.copy(SAMPLE, REC)
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_STT_MODELS": str(MODEL.parent)}
APP_CONFIG = {"assistant": {"speech_model": MODEL.name[len("ggml-"):-len(".bin")]}}


def test_1_it_transcribes_a_recording_with_times(app):
    c = app.client()
    r = c.call("audio_transcribe", file=str(REC), timeout=300)
    assert "americans" in r["text"].lower() and r["source"].startswith("local"), r
    assert r["segments"] and r["segments"][0]["end"] > r["segments"][0]["start"] >= 0
    txt = pathlib.Path(r["transcript_file"])
    assert txt.exists() and txt.read_text().startswith("[00:00]"), txt
    assert (txt.parent / (REC.stem + ".local.json")).exists()


def test_2_an_existing_transcript_is_not_redone(app):
    c = app.client()
    t0 = time.time()
    r = c.call("audio_transcribe", file=str(REC))
    assert r["source"] == "local transcript (already there)" and time.time() - t0 < 1, r


def test_3_the_remote_transcript_comes_first(app):
    c = app.client()
    remote = DIR / "transcripts" / (REC.stem + ".txt")
    remote.write_text("[large-v3] And so, my fellow Americans...\n")
    r = c.call("audio_transcribe", file=str(REC))
    assert r["source"].startswith("remote") and "large-v3" in r["text"], r


def test_4_a_missing_file_is_a_clear_error(app):
    c = app.client()
    assert "No such recording" in c.call_error("audio_transcribe", file=str(DIR / "nope.ogg"))
