"""E15: live notes: a recording transcribed while it goes on (2026-11-09, the owner's request). The recorded sound
(here a synthetic one through the recorder's sample tap) is cut at pauses, a speech recognizer (a fake "stream" one,
docs/mcp/TRANSCRIBERS.md) answers each piece, the text goes into a narrow column of a live-notes page inserted after
the current page, and the serving session (a fake agent) is woken to read it with live_notes."""

import json
import math
import pathlib
import random
import struct
import tempfile
import time

DIR = pathlib.Path(tempfile.mkdtemp(prefix="xoai-live-"))
AGENT_LOG, RECOGNIZER_LOG = DIR / "agent.log", DIR / "recognizer.log"
AGENT = pathlib.Path(__file__).with_name("fake_agent.py")
RECOGNIZER = DIR / "recognizer.py"
RECOGNIZER.write_text(f"""import json, sys, wave
log = open({str(RECOGNIZER_LOG)!r}, "a")
print(json.dumps({{"type": "hello", "protocol": 1, "engine": "fake recognizer", "audio": "path"}}), flush=True)
for line in sys.stdin:
    req = json.loads(line)
    log.write(line); log.flush()
    with wave.open(req["audio_path"]) as w:
        seconds = w.getnframes() / w.getframerate()
        ok = (w.getsampwidth(), w.getnchannels(), w.getframerate()) == (2, 1, 16000)
    print(json.dumps({{"type": "result", "id": req["id"], "text": f"piece {{req['id']}} lasts {{seconds:.1f}} s" +
                       ("" if ok else " (bad format)"), "language": "ru", "language_probability": 0.9}}), flush=True)
""")
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={AGENT_LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def wav(path, parts, rate=16000):
    """parts: [("noise" | "voice", seconds)]: a quiet microphone (~0.001 RMS) and a voice (a 0.1 tone)"""
    rnd = random.Random(1)
    samples = []
    for kind, sec in parts:
        for i in range(int(sec * rate)):
            v = 0.1 * math.sin(2 * math.pi * 220 * len(samples) / rate) if kind == "voice" else rnd.uniform(-1, 1) * 0.0017
            samples.append(int(max(-1, min(1, v)) * 32767))
    data = struct.pack(f"<{len(samples)}h", *samples)
    pathlib.Path(path).write_bytes(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
                                   struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" +
                                   struct.pack("<I", len(data)) + data)
    return str(path)


def wait(pred, timeout=15):
    end = time.time() + timeout
    while time.time() < end:
        v = pred()
        if v:
            return v
        time.sleep(0.2)
    return pred()


def texts(c, page):
    return [e.get("text", "") for e in c.call("page_elements", page=page, detail="full")["elements"]
            if e.get("type") == "text"]


def got():
    return AGENT_LOG.read_text() if AGENT_LOG.exists() else ""


def test_1_speech_becomes_text_in_the_column_of_a_new_page(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    assert c.call("live_notes", op="status")["active"] is False
    pages = len(c.call("doc_info")["pages"])
    c.call("test_live_audio", op="start", name="talk.ogg")
    # (in two parts: in real time the first piece's text is back before the second piece ends)
    c.call("test_live_audio", op="feed", file=wav(DIR / "a.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    assert wait(lambda: c.call("live_notes", op="status")["written"] >= 1)
    c.call("test_live_audio", op="feed", file=wav(DIR / "a2.wav", [("voice", 3), ("noise", 2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 2 else None)(c.call("live_notes", op="status")))
    assert st and st["pieces"] == 2 and st["transcribed"] == 2 and st["listening"], st
    assert st["engine"] == "fake recognizer", st
    assert len(c.call("doc_info")["pages"]) == pages + 1  # (inserted after the current page)
    live = st["pages"][0]
    assert live["page"] == 2
    x, y, w, h = live["transcript_column"]
    fx, fy, fw, fh = live["free_area"]
    assert fx + fw < x and w < 0.3 * (x + w), live  # (a narrow column on the right; the rest is free)
    t = texts(c, 2)
    assert any(s.startswith("Live notes") and "talk.ogg" in s for s in t), t
    lines = [s for s in t if s.startswith("[")]
    # (a piece starts 0.3 s before the voice and keeps 0.3 s of the pause after it)
    assert lines == ["[0:00] piece 1 lasts 2.6 s", "[0:04] piece 2 lasts 3.7 s"], lines
    fonts = {e["font"]["size"] for e in c.call("page_elements", page=2, detail="full")["elements"]
             if e.get("type") == "text" and e.get("text", "").startswith("[")}
    assert fonts == {3.8}, fonts  # (half the first 7.5: more of the talk on a page, the owner's wish 2026-11-09)
    styles = {e["font"]["name"] for e in c.call("page_elements", page=2, detail="full")["elements"]
              if e.get("type") == "text" and e.get("text", "").startswith("[")}
    assert styles == {"Sans Italic"}, styles  # (italic as written, not redrawn afterwards)
    assert fx <= 12.5 and fy <= 18 and x - (fx + fw) <= 8 and w <= 0.2 * (x + w + 12), live  # (a dense page)
    assert st["folder"].startswith(str(app.home)), st["folder"]  # (no audio folder set: the app's data folder)


def test_2_the_serving_session_is_woken_and_reads_at_its_pace(app):
    c = app.client()
    assert wait(lambda: 'live_notes(op=\\"read\\")' in got() or 'live_notes(op="read")' in got()), got()
    r = c.call("live_notes", op="read")
    assert [p["n"] for p in r["pieces"]] == [1, 2] and r["pieces"][0]["at"] == "0:00", r
    assert r["pieces"][1]["text"] == "piece 2 lasts 3.7 s" and r["pieces"][1]["language"] == "ru"
    assert r["cursor"] == 2 and c.call("live_notes", op="read")["pieces"] == []  # (nothing new)
    assert got().count("Live notes (recording") == 1  # (one wake-up until it has read)


def test_3_each_piece_carries_the_language_and_the_words_before(app):
    reqs = [json.loads(l) for l in RECOGNIZER_LOG.read_text().splitlines()]
    assert reqs[0]["language"] == "auto" and reqs[0]["language_hint"] == "" and reqs[0]["prompt"] == ""
    assert reqs[1]["language_hint"] == "ru" and reqs[1]["prompt"] == "piece 1 lasts 2.6 s", reqs[1]
    assert "audio_b64" not in reqs[0]  # (the recognizer reads the file: it said "path")


def test_4_the_scissors_send_the_piece_being_spoken(app):
    c = app.client()
    c.call("test_live_audio", op="feed", file=wav(DIR / "b.wav", [("voice", 4)]))
    assert c.call("live_notes", op="status")["pieces"] == 2  # (no pause yet)
    c.call("live_notes", op="cut")
    c.call("test_live_audio", op="feed", file=wav(DIR / "c.wav", [("voice", 0.2)]))
    assert wait(lambda: c.call("live_notes", op="status")["written"] == 3)
    assert any(s.startswith("[0:09] piece 3 lasts 4.") for s in texts(c, 2)), texts(c, 2)


def test_5_the_end_of_the_recording_sends_the_rest_and_keeps_the_text(app):
    c = app.client()
    c.call("test_live_audio", op="feed", file=wav(DIR / "d.wav", [("voice", 2)]))
    c.call("test_recording", file=str(DIR / "talk.ogg"), name="talk.ogg", duration_ms=17000)
    st = wait(lambda: (lambda s: s if s["done"] else None)(c.call("live_notes", op="status")))
    assert st and not st["listening"] and st["pieces"] == 4, st
    lines = pathlib.Path(st["transcript"]).read_text().splitlines()
    assert lines[0].startswith("# Live notes: talk.ogg") and len(lines) == 5, lines
    assert pathlib.Path(st["folder"], "piece-0004.wav").exists()
    c.call("live_notes", op="read")
    assert wait(lambda: "Live notes (recording talk.ogg, page 2) ended" in got()), got()


def test_6_a_long_talk_goes_on_a_next_page(app):
    c = app.client()
    c.call("test_live_audio", op="start", name="long.ogg")
    parts = [("noise", 0.5)]
    for _ in range(150):  # (one short line each: at 3.8 pt a page holds over a hundred)
        parts += [("voice", 1), ("noise", 1.6)]
    c.call("test_live_audio", op="feed", file=wav(DIR / "e.wav", parts))
    st = wait(lambda: (lambda s: s if s["written"] >= 150 else None)(c.call("live_notes", op="status")), 60)
    assert st and len(st["pages"]) >= 2, st
    assert st["pages"][1]["page"] == st["pages"][0]["page"] + 1
    assert any("(continued)" in s for s in texts(c, st["pages"][1]["page"]))
    c.call("test_recording", file=str(DIR / "long.ogg"), name="long.ogg", duration_ms=190000)


def test_7_the_live_notes_are_not_in_the_user_s_undo_history(app):
    """2026-11-09: an Undo meant for the owner's stroke removed the live-notes page ("Page 2 deleted by Undo (it had
    been inserted)"): the app's own pages and transcript are no longer in the user's undo history, and the change log
    says "app", not "user"."""
    c = app.client()
    before = len(c.call("doc_info")["pages"])
    cursor = c.call("changes_get")["cursor"]
    c.call("test_live_audio", op="start", name="undo.ogg")
    c.call("test_live_audio", op="feed", file=wav(DIR / "u.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    assert wait(lambda: c.call("live_notes", op="status")["written"] >= 1)
    assert len(c.call("doc_info")["pages"]) == before + 1
    hist = c.call("history")["undo"]
    assert not any("Page inserted" in h for h in hist[-3:]), hist
    try:
        c.call("undo")  # (whatever the user did before; never the live page)
    except Exception:
        pass  # (nothing to undo: fine)
    assert len(c.call("doc_info")["pages"]) == before + 1, hist  # (the Undo didn't take the live page)
    events = c.call("changes_get", since=cursor)["events"]
    mine = [e for e in events if e["type"] in ("page_inserted", "element_added")]
    assert mine and all(e["origin"] == "app" for e in mine), mine
    c.call("test_recording", file=str(DIR / "undo.ogg"), name="undo.ogg", duration_ms=5000)
