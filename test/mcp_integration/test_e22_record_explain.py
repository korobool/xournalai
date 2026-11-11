"""E22: the Record menu explains each choice (1.22.0, the owner, 11 Nov 2026: «при наведении курсора на режим
выскакивал бы тип, который объясняет, что оно означает и как работает … я не знаю, что такое proposals, не вполне
понимаю, чем экономные и интенсивные режимы отличаются»). Every tile, speech-commands level and option has a few lines:
the mouse's tooltip, and — hover tooltips are off for the pen and fingers (TooltipGuard) — a long press shows them in
a strip at the bottom of the menu, starting and switching nothing. «💡 Proposals» became «❓ May ask me»; the switches
say what each side means."""

import json
import time

from test_e15_live_modes import long_press, state_file, widgets
from test_e15_live_notes import AGENT, DIR, RECOGNIZER, wait

LOG = DIR / "explain-agent.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}

CHOICES = ["aiRecordPlain"] + [f"aiRecordMode-{m}" for m in ["text", "conspect", "konspekt", "voice"]] + \
          [f"aiRecordLevel-{l}" for l in ["off", "trigger", "talk"]] + \
          [f"aiRecordOpt-{t}" for t in ["this_page", "intense", "summary", "questions", "browser"]]


def open_menu(app, c):
    long_press(app, widgets(c)["aiRecordButton"])
    assert "aiRecordExplain" in widgets(c)


def test_1_every_choice_has_its_explanation_as_a_tooltip(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    open_menu(app, c)
    ws = widgets(c)
    for name in CHOICES:
        tip = ws[name].get("tooltip", "")
        assert len(tip) > 60 and tip.count(" ") > 10, (name, tip)  # (sentences, not a label again)
    assert "limits" in ws["aiRecordOpt-intense"]["tooltip"]  # (what Economy and Intense cost)
    assert "ask you" in ws["aiRecordOpt-questions"]["tooltip"]
    assert ws["aiRecordExplain"]["label"] == "Hold any choice to read what it does."


def test_2_the_renamed_option_and_the_switches_second_lines(app):
    c = app.client()
    ws = widgets(c)
    assert ws["aiRecordOpt-questions"]["label"] == "☐ ❓ May ask me", ws["aiRecordOpt-questions"]
    assert ws["aiRecordOpt-intense"]["label"] == "🌿 Economy\nfewer agents, saves limits"
    assert ws["aiRecordOpt-this_page"]["label"] == "📄 New page\nafter this one"


def test_3_a_long_press_explains_and_switches_nothing(app):
    c = app.client()
    before = json.loads(state_file(app).read_text()) if state_file(app).exists() else None
    long_press(app, widgets(c)["aiRecordOpt-intense"])
    ws = widgets(c)
    assert ws["aiRecordExplain"]["label"].startswith("🌿 Economy / ⚡ Intense\n"), ws["aiRecordExplain"]
    assert "limits" in ws["aiRecordExplain"]["label"]
    assert ws["aiRecordOpt-intense"]["label"].startswith("🌿 Economy")  # (not switched)
    after = json.loads(state_file(app).read_text()) if state_file(app).exists() else None
    assert after == before
    long_press(app, widgets(c)["aiRecordLevel-off"])
    ws = widgets(c)
    assert ws["aiRecordExplain"]["label"].startswith("🛡 Commands off\n"), ws["aiRecordExplain"]
    assert not ws["aiRecordLevel-off"].get("active", False)  # (the level stays as it was)


def test_4_a_long_press_on_a_tile_starts_no_recording(app):
    c = app.client()
    long_press(app, widgets(c)["aiRecordMode-conspect"])
    ws = widgets(c)
    assert ws["aiRecordExplain"]["label"].startswith("🧠 Notes\n"), ws["aiRecordExplain"]
    assert "aiRecordMode-conspect" in ws  # (the menu is still open)
    time.sleep(0.5)
    assert c.call("live_notes", op="status")["active"] is False
    assert not c.call("app_status").get("recording", False)
    # a plain tap is still a tap: the menu closes to start the recording (the test display has no microphone)
    c.call("ui_interact", op="click", target=ws["aiRecordMode-conspect"]["id"])
    assert wait(lambda: "aiRecordExplain" not in shown(c))


def shown(c):
    return {w["name"] for w in c.call("ui_inspect", max_depth=80)["widgets"] if w.get("name")}
