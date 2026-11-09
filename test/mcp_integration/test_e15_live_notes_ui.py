"""E15: live notes from the big buttons: a long press on Record offers Record or Record + live notes (when the
experimental switch is on; a long press is not a tap: it starts nothing), ✂ shows next to Record while live notes
go on and sends the piece being spoken."""

import json
import time

from test_e15_live_notes import DIR, RECOGNIZER, wait, wav

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": False,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def shown(ws, name):
    return name in ws and ws[name].get("visible", True)


def long_press(app, w, hold=1.2):
    x, y, width, height = w["bbox"]
    app._xdo("mousemove", x + width // 2, y + height // 2)
    app._xdo("mousedown", 1)
    time.sleep(hold)
    app._xdo("mouseup", 1)
    time.sleep(0.4)


def recording(c):
    return c.call("app_status")["ui"].get("recording") == "listening"


def test_1_a_long_press_on_record_offers_live_notes(app):
    c = app.client()
    ws = widgets(c)
    assert shown(ws, "aiRecordButton") and not shown(ws, "aiLiveCutButton"), ws.get("aiLiveCutButton")
    long_press(app, ws["aiRecordButton"])
    ws = widgets(c)
    assert shown(ws, "aiRecordPlain") and shown(ws, "aiRecordLive"), [n for n in ws if n.startswith("aiRecord")]
    bx, by, bw, bh = ws["aiRecordButton"]["bbox"]
    assert ws["aiRecordLive"]["bbox"][1] >= by + bh  # (under the button)
    app.user_key("Escape")
    time.sleep(0.4)
    ws = widgets(c)
    assert not shown(ws, "aiRecordLive")
    assert [w["kind"] for w in c.call("ui_windows")["windows"]] == ["main"]  # (the press started no recording)


def test_2_the_scissors_show_during_live_notes_and_cut(app):
    c = app.client()
    c.call("test_live_audio", op="start", name="ui.ogg")
    ws = widgets(c)
    assert shown(ws, "aiLiveCutButton")
    c.call("test_live_audio", op="feed", file=wav(DIR / "ui1.wav", [("noise", 1), ("voice", 3)]))
    c.call("ui_interact", op="click", target=ws["aiLiveCutButton"]["id"])
    c.call("test_live_audio", op="feed", file=wav(DIR / "ui2.wav", [("voice", 0.2)]))
    st = wait(lambda: (lambda s: s if s["written"] >= 1 else None)(c.call("live_notes", op="status")))
    assert st and st["pieces"] == 1, st
    c.call("test_recording", file=str(DIR / "ui.ogg"), name="ui.ogg", duration_ms=5000)
    assert wait(lambda: not shown(widgets(c), "aiLiveCutButton"))


def test_3_the_switch_is_remembered(app):
    c = app.client()
    c.call("action_run", action="win.ai-live-notes", state=False)
    assert json.loads(app.config_file.read_text())["assistant"]["live_notes"]["enabled"] is False
    c.call("action_run", action="win.ai-live-notes", state=True)
    assert json.loads(app.config_file.read_text())["assistant"]["live_notes"]["enabled"] is True
