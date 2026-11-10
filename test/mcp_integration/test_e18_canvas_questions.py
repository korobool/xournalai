"""E18: questions on the canvas (1.19.0; the owner: «если я нарисовал бабочку и ожидаю, что она улучшится, а слева внизу
появился тип, я могу это не заметить»). A question about something on the page shows there too, at once: an outline and
a bubble beside it with the short question and the asker's buttons, numbered and coloured as in the question panel,
where the full question stays. A tap in either place answers it, and both go. Never for an action outside the app. A
switch in the panel turns it off."""

import json
import pathlib
import time

from test_e15_live_notes import AGENT, DIR, wait

LOG = DIR / "canvas-questions-agent.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300}}
BUTTERFLY = {"page": 1, "region": [200, 220, 120, 90]}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def shown(ws, name):
    return name in ws and ws[name].get("visible", True)


def waiting(c, qid):
    return next((q for q in c.call("ask_owner", op="list")["waiting"] if q["id"] == qid), None)


def test_1_a_question_about_a_drawing_shows_on_it_at_once(app):
    c = app.client()
    assert wait(lambda: shown(widgets(c), "aiQuestionPanel"))
    qid = c.call("ask_owner", op="ask", question="Shall I redraw the butterfly with clean, symmetric lines?",
                 short="Improve the butterfly?", about=BUTTERFLY, options=["Yes, improve", "No"], wait=False)["id"]
    q = wait(lambda: (lambda q: q if q and q.get("on_canvas") else None)(waiting(c, qid)), 5)
    assert q and q["about"] == {"page": 1, "region": [200, 220, 120, 90]} and q["short"] == "Improve the butterfly?", q
    ws = widgets(c)
    bubble = f"aiQuestionBubble{qid}"
    assert shown(ws, bubble), sorted(n for n in ws if n.startswith("aiQuestion"))
    assert ws[bubble + "Text"]["label"] == q["mark"] + " Improve the butterfly?", ws[bubble + "Text"]
    assert ws[bubble + "Option1"]["label"] == "Yes, improve" and ws[bubble + "Option2"]["label"] == "No"
    # the panel: the full question, the same number, and where it is
    assert ws["aiQuestionText"]["label"] == "Shall I redraw the butterfly with clean, symmetric lines?"
    assert ws["aiQuestionWho"]["label"].startswith(q["mark"] + " Claude asks"), ws["aiQuestionWho"]
    assert ws["aiQuestionLocate"]["label"] == "↗ page 1"
    # a tap on the bubble answers it: both go
    c.call("ui_interact", op="click", target=ws[bubble + "Option1"]["id"])
    a = wait(lambda: (lambda a: a if a.get("state") == "answered" else None)(c.call("ask_owner", op="get", id=qid)), 5)
    assert a and a["answer"] == "Yes, improve" and a["by"] == "owner", a
    assert wait(lambda: bubble not in widgets(c), 5)
    assert widgets(c)["aiQuestionText"]["label"] == "No questions"


def test_2_answered_in_the_panel_the_bubble_goes_too(app):
    c = app.client()
    qid = c.call("ask_owner", op="ask", question="Colour the wings orange?", short="Orange wings?", about=BUTTERFLY,
                 wait=False)["id"]
    assert wait(lambda: shown(widgets(c), f"aiQuestionBubble{qid}"), 5)
    c.call("ui_interact", op="click", target=widgets(c)["aiQuestionOption2"]["id"])  # (No)
    assert wait(lambda: f"aiQuestionBubble{qid}" not in widgets(c), 5)
    assert c.call("ask_owner", op="get", id=qid)["answer"] == "no"


def test_3_never_for_an_action_outside_the_app(app):
    c = app.client()
    qid = c.call("ask_owner", op="ask", question="Upload the drawing to Google Drive?", about=BUTTERFLY, outside=True,
                 wait=False)["id"]
    time.sleep(0.5)
    q = waiting(c, qid)
    assert q and "about" not in q and f"aiQuestionBubble{qid}" not in widgets(c), q
    c.call("ask_owner", op="cancel", id=qid)


def test_4_about_his_latest_strokes(app):
    c = app.client()
    assert "drawn nothing" in c.call_error("ask_owner", op="ask", question="Smooth this?", about_ink=True, wait=False)
    x, y = app.canvas_center(c)
    app.user_drag([(x - 60 + i * 6, y + (i % 5)) for i in range(20)])  # (he draws)
    time.sleep(0.6)
    qid = c.call("ask_owner", op="ask", question="Smooth this line?", about_ink=True, wait=False)["id"]
    q = wait(lambda: (lambda q: q if q and q.get("on_canvas") else None)(waiting(c, qid)), 5)
    assert q and q["short"] == "Smooth this line?" and q["about"]["page"] == 1, q
    c.call("ask_owner", op="cancel", id=qid)
    assert wait(lambda: f"aiQuestionBubble{qid}" not in widgets(c), 5)


def test_5_the_panel_s_switch_turns_it_off(app):
    c = app.client()
    switch = widgets(c)["aiQuestionsOnCanvas"]
    assert switch["label"] == "Questions on the canvas too"
    c.call("ui_interact", op="click", target=switch["id"])
    cfg = json.loads(pathlib.Path(app.config_file).read_text())
    assert cfg["assistant"]["questions_on_canvas"] is False, cfg["assistant"]
    qid = c.call("ask_owner", op="ask", question="Add a caption?", about=BUTTERFLY, wait=False)["id"]
    time.sleep(0.5)
    assert waiting(c, qid)["on_canvas"] is False and f"aiQuestionBubble{qid}" not in widgets(c)
    assert widgets(c)["aiQuestionText"]["label"] == "Add a caption?"  # (still in the panel)
    c.call("ask_owner", op="cancel", id=qid)
    c.call("ui_interact", op="click", target=widgets(c)["aiQuestionsOnCanvas"]["id"])
    assert json.loads(pathlib.Path(app.config_file).read_text())["assistant"]["questions_on_canvas"] is True
