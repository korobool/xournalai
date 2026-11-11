"""E21: «📍 This page» (1.22.0, the owner, 11 Nov 2026: «аннотацию начинать с новой страницы или на текущей странице.
Это должна быть тоже галочка в меню под рекордингом»). A switch in the Record menu, remembered: 📄 a new page after
the current one (as before), or 📍 the page he is on. On his own page the title and the transcript column go only where
the page is empty; with his ink (or a slide) at the right edge there is no column, and the transcript stays off the
paper (live_notes still has it)."""

import json
import time

from test_e15_live_modes import long_press, state_file, widgets
from test_e15_live_notes import AGENT, DIR, RECOGNIZER, texts, wait, wav

LOG = DIR / "this-page-agent.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}}}}


def pages(c):
    return len(c.call("doc_info")["pages"])


def record(c, name):
    c.call("test_live_audio", op="start", name=name, mode="conspect")
    c.call("test_live_audio", op="feed", file=wav(DIR / f"{name}.wav", [("noise", 1), ("voice", 2), ("noise", 2)]))
    return wait(lambda: (lambda s: s if s["written"] >= 1 else None)(c.call("live_notes", op="status")))


def end(c, name):
    c.call("test_recording", file=str(DIR / name), name=name, duration_ms=5000)
    assert wait(lambda: c.call("live_notes", op="status")["done"])


def set_this_page(app, on):
    f = state_file(app)
    st = json.loads(f.read_text()) if f.exists() else {"last_mode": "conspect", "options": {}}
    st["options"]["this_page"] = on
    f.write_text(json.dumps(st))


def test_1_the_switch_is_in_the_record_menu_and_remembered(app):
    c = app.client()
    time.sleep(1)  # (the fake agent starts)
    long_press(app, widgets(c)["aiRecordButton"])
    ws = widgets(c)
    chip = ws["aiRecordOpt-this_page"]
    assert chip["label"].startswith("📄 New page"), chip  # (as before: a new page)
    c.call("ui_interact", op="click", target=chip["id"])
    ws = widgets(c)
    assert ws["aiRecordOpt-this_page"]["label"].startswith("📍 This page")
    assert json.loads(state_file(app).read_text())["options"]["this_page"] is True
    assert "📍" in ws["aiRecordMode-conspect-badges"]["label"], ws["aiRecordMode-conspect-badges"]
    assert "📍" in ws["aiRecordMode-text-badges"]["label"]
    assert "📍" not in ws["aiRecordMode-voice-badges"]["label"]  # (🎙 Voice only writes no page)
    app.user_key("Escape")


def test_2_on_his_empty_page_no_page_is_added(app):
    c = app.client()
    set_this_page(app, True)
    before = pages(c)
    st = record(c, "here.ogg")
    assert st, c.call("live_notes", op="status")
    assert pages(c) == before  # (no page inserted)
    live = st["pages"][0]
    assert live["page"] == 1 and live.get("own_page") is True, live
    assert live["transcript_column"], live  # (the right edge was empty: the column is there)
    t = texts(c, 1)
    assert any(s.startswith("Live notes") and "here.ogg" in s for s in t), t
    assert "TRANSCRIPT" in t and any(s.startswith("[0:00] piece 1") for s in t), t
    assert wait(lambda: "options: economy, this_page" in (LOG.read_text() if LOG.exists() else ""))  # (the agent knows)
    end(c, "here.ogg")


def test_3_with_his_ink_at_the_right_edge_no_column_and_the_transcript_off_the_paper(app):
    c = app.client()
    set_this_page(app, True)
    c.call("page_manage", op="insert")
    page = pages(c)
    c.call("page_manage", op="goto", page=page)
    w = c.call("doc_info")["pages"][page - 1]["width"]
    c.call("create_strokes", page=page, strokes=[{"points": [[w - 40, 200], [w - 20, 260]]}], animate=False)
    before = pages(c)
    st = record(c, "inked.ogg")
    assert st, c.call("live_notes", op="status")
    assert pages(c) == before
    live = st["pages"][0]
    assert live["page"] == page and live.get("own_page") is True, live
    assert live["transcript_column"] is None, live
    fx, fy, fw, fh = live["free_area"]
    assert fw > 0.8 * w, live  # (the whole page; only where it is empty, says the skill)
    t = texts(c, page)
    assert not [s for s in t if s.startswith("[")] and "TRANSCRIPT" not in t, t  # (nothing in the column)
    assert any(s.startswith("Live notes") for s in t), t  # (the top was empty: the title is there)
    got = c.call("live_notes", op="read", since=0)["pieces"]
    assert got and got[0]["text"].startswith("piece 1"), got  # (the transcript is still there for the agents)
    end(c, "inked.ogg")


def test_4_a_new_page_again_when_switched_back(app):
    c = app.client()
    set_this_page(app, False)
    c.call("page_manage", op="goto", page=1)
    before = pages(c)
    st = record(c, "new.ogg")
    assert st and pages(c) == before + 1, st
    assert st["pages"][0]["page"] == 2 and "own_page" not in st["pages"][0], st["pages"]
    end(c, "new.ogg")
