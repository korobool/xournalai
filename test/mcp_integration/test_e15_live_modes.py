"""E15: the modes of live notes (1.17.0, the owner's choice, docs/mcp/LIVE_MODES.md): a long press on Record offers
the modes as big tiles and the options of the next recording as big toggles (remembered); every wake-up line names
the mode and what to read for it; 📝 Текст wakes nobody while it records (only at the end, for «Summary at the end»);
✍ tells where the owner drew last. A short tap on Record stays the plain recording it always was."""

import json
import pathlib
import time

from test_e15_live_notes import AGENT, DIR, RECOGNIZER, wait, wav

LOG = DIR / "modes-agent.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def long_press(app, w, hold=1.2):
    x, y, width, height = w["bbox"]
    app._xdo("mousemove", x + width // 2, y + height // 2)
    app._xdo("mousedown", 1)
    time.sleep(hold)
    app._xdo("mouseup", 1)
    time.sleep(0.4)


def wakes():
    return [l for l in (LOG.read_text() if LOG.exists() else "").splitlines() if "Live notes (recording" in l]


def state_file(app):
    return pathlib.Path(app.config_file).parent / "ai-live-modes.json"


def record(c, name, mode, parts):
    c.call("test_live_audio", op="start", name=name, mode=mode)
    c.call("test_live_audio", op="feed", file=wav(DIR / f"{name}.wav", parts))
    assert wait(lambda: c.call("live_notes", op="status")["written"] >= 1)


def end(c, name):
    c.call("test_recording", file=str(DIR / name), name=name, duration_ms=5000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])


def test_1_the_long_press_offers_every_mode_and_the_options(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    long_press(app, widgets(c)["aiRecordButton"])
    ws = widgets(c)
    tiles = [n for n in ws if n.startswith("aiRecordMode-") and not n.endswith("-badges")]
    # (1.20.0: the tiles say what appears on paper; ✍ = 📝 with commands, 🛡 = 🧠 with commands off)
    tiles = [n for n in tiles if not n.endswith("-hint")]
    assert sorted(tiles) == sorted(f"aiRecordMode-{m}" for m in ["text", "voice", "conspect", "konspekt"]), tiles
    assert "aiRecordPlain" in ws  # (the plain recording too, first)
    assert ws["aiRecordMode-voice"].get("enabled", True) is True  # (since 1.17.1)
    chips = {n: ws[n] for n in ws if n.startswith("aiRecordOpt-")}
    assert sorted(chips) == sorted(f"aiRecordOpt-{t}" for t in ["this_page", "intense", "summary", "questions",
                                                                  "browser", "speakers", "my_voice"]), chips
    # one switch for speech commands (it replaced the 🛡 tile and the 🔤 option)
    levels = {n: ws[n] for n in ws if n.startswith("aiRecordLevel-")}
    assert sorted(levels) == ["aiRecordLevel-off", "aiRecordLevel-talk", "aiRecordLevel-trigger"], levels
    assert levels["aiRecordLevel-off"]["label"] == "🛡 Commands off\nothers talking, a video"
    assert levels["aiRecordLevel-trigger"]["label"] == "🔤 «Hey Assistant»\npeople around"
    assert levels["aiRecordLevel-talk"]["label"] == "💬 Conversation\njust me"
    assert chips["aiRecordOpt-intense"]["label"].startswith("🌿 Economy\n")  # (a switch: economy, or intense)
    assert chips["aiRecordOpt-browser"].get("enabled", True) is True  # (since 1.18.0: the research browser)
    assert "aiResearchSignIn" not in ws  # (in the Tools panel since 1.20.0, not a recording's choice)
    # commands off: 🎙 Voice only (nothing but commands) can't be chosen; on again: it can
    c.call("ui_interact", op="click", target=levels["aiRecordLevel-off"]["id"])
    ws = widgets(c)
    assert ws["aiRecordMode-voice"].get("enabled", True) is False, ws["aiRecordMode-voice"]
    assert ws["aiRecordMode-voice-hint"]["label"] == "needs speech commands"
    assert json.loads(state_file(app).read_text())["options"]["commands"] == "off"
    assert ws["aiRecordMode-conspect-badges"]["label"].startswith("🛡")
    c.call("ui_interact", op="click", target=ws["aiRecordLevel-talk"]["id"])
    ws = widgets(c)
    assert ws["aiRecordMode-voice"].get("enabled", True) is True and ws["aiRecordMode-conspect-badges"]["label"].startswith("💬")
    # the options are toggled with a finger, remembered, and shown on the tiles they apply to
    c.call("ui_interact", op="click", target=chips["aiRecordOpt-intense"]["id"])
    c.call("ui_interact", op="click", target=chips["aiRecordOpt-summary"]["id"])
    ws = widgets(c)
    assert ws["aiRecordOpt-intense"]["label"].startswith("⚡ Intense\n") and ws["aiRecordOpt-summary"]["label"].startswith("☑")
    saved = json.loads(state_file(app).read_text())
    assert saved["options"]["intense"] is True and saved["options"]["summary"] is True, saved
    assert ws["aiRecordMode-conspect-badges"]["label"] == "💬⚡🧾", ws["aiRecordMode-conspect-badges"]
    assert ws["aiRecordMode-text-badges"]["label"] == "💬⚡🧾"  # (📝 with commands: an agent listens)
    app.user_key("Escape")
    time.sleep(0.4)
    assert "aiRecordMode-text" not in widgets(c) or not widgets(c)["aiRecordMode-text"].get("visible", True)
    assert c.call("live_notes", op="status")["active"] is False  # (the long press itself started nothing)


def test_2_a_short_tap_on_record_is_the_plain_recording(app):
    """The owner (2026-11-09): the default must not break: a tap without holding records as it always did (here,
    with no audio folder and no sound card, the plain recorder says so itself, as it always has)."""
    c = app.client()
    ws = widgets(c)
    c.call("ui_interact", op="click", target=ws["aiRecordButton"]["id"])
    time.sleep(1)
    assert not [n for n, w in widgets(c).items() if n.startswith("aiRecordMode-") and w.get("visible", True)]
    assert c.call("live_notes", op="status")["active"] is False  # (no live notes, whatever mode was used last)
    said = []
    for _ in range(4):  # (the recorder's messages, one over the other: the top one first)
        dialogs = [w for w in c.call("ui_windows")["windows"] if w["kind"] == "dialog"]
        if not dialogs:
            break
        for win in reversed(dialogs):
            items = c.call("ui_inspect", target=win["id"], max_depth=40, all=True)["widgets"]
            said += [x.get("label", "") for x in items if x.get("label")]
            c.call("ui_interact", op="close", target=win["id"])  # (the owner's OK)
            time.sleep(0.3)
    recording = c.call("app_status")["ui"].get("recording") == "listening"
    assert recording or any("Recorder could not be started" in s or "Audio folder" in s for s in said), said
    if recording:  # (where the test machine can record: stop it)
        c.call("ui_interact", op="click", target=ws["aiRecordButton"]["id"])
    left = c.call("ui_windows")["windows"]
    assert [w["kind"] for w in left] == ["main"], (left, said)


def test_3_every_wake_up_names_the_mode_and_its_guide(app):
    c = app.client()
    s = json.loads(state_file(app).read_text())
    s["options"]["commands"] = "off"  # (🧠 with 🛡: what the 🛡 tile was)
    state_file(app).write_text(json.dumps(s))
    before = len(wakes())
    record(c, "safe.ogg", "conspect", [("noise", 1), ("voice", 2), ("noise", 2)])
    assert wait(lambda: len(wakes()) > before), wakes()
    line = wakes()[-1]
    assert ": new transcribed speech. Mode «Notes» (conspect: annotate=visual, commands=off; " \
        "options: intense, summary)." in line, line
    assert "modes/safe.md" in line and "nothing said is ever carried out" in line, line
    st = c.call("live_notes", op="status")
    assert st["mode"] == "conspect", st
    c.call("live_notes", op="read")
    end(c, "safe.ogg")
    assert wait(lambda: any("safe.ogg" in l and "ended" in l and "«Summary at the end»" in l for l in wakes())), wakes()


def test_4_text_wakes_nobody_while_it_records_and_only_for_a_summary_at_the_end(app):
    c = app.client()
    s = json.loads(state_file(app).read_text())
    s["options"]["summary"] = False
    s["options"]["commands"] = "off"  # (📝 with 🛡: the transcript only)
    state_file(app).write_text(json.dumps(s))
    before = len(wakes())
    record(c, "text1.ogg", "text", [("noise", 1), ("voice", 2), ("noise", 2)])
    time.sleep(1.5)
    assert len(wakes()) == before, wakes()[before:]  # (the transcript only)
    assert c.call("live_notes", op="status")["mode"] == "text"
    end(c, "text1.ogg")
    time.sleep(1.5)
    assert len(wakes()) == before, wakes()[before:]  # (no summary asked)
    s["options"]["summary"] = True
    state_file(app).write_text(json.dumps(s))
    record(c, "text2.ogg", "text", [("noise", 1), ("voice", 2), ("noise", 2)])
    time.sleep(1.5)
    assert len(wakes()) == before
    end(c, "text2.ogg")
    assert wait(lambda: len(wakes()) == before + 1), wakes()[before:]
    line = wakes()[-1]
    assert "text2.ogg, page" in line and ") ended: the whole text is in. Mode «Transcript»" in line and "since=0" in line and "summary page" in line, line


def test_5_drawing_tells_where_the_owner_drew_last(app):
    c = app.client()
    page = len(c.call("doc_info")["pages"])
    w = [x for x in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if x["type"] == "GtkXournal"][0]
    x, y, ww, hh = w["bbox"]
    for yy in range(y + 40, y + hh - 20, 60):  # (the owner's strokes, as test_e7 draws them)
        app.user_drag([(x + 10, yy), (x + ww // 2, yy), (x + ww - 10, yy)])
    time.sleep(0.5)
    s = json.loads(state_file(app).read_text())
    s["options"]["commands"] = "talk"  # (📝 with 💬: what ✍ was)
    state_file(app).write_text(json.dumps(s))
    before = len(wakes())
    record(c, "draw.ogg", "text", [("noise", 1), ("voice", 2), ("noise", 2)])
    assert wait(lambda: len(wakes()) > before), wakes()
    line = wakes()[-1]
    assert "Mode «Transcript» (text: annotate=none, commands=talk" in line, line
    assert "modes/drawing.md" in line and "The owner's latest ink: page " in line and "s ago." in line, line
    assert "What is said is context: draw nothing for it." in line and "his own strokes too" in line, line
    c.call("live_notes", op="read")
    end(c, "draw.ogg")
    assert len(c.call("doc_info")["pages"]) >= page
