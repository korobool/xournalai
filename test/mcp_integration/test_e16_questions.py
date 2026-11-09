"""E16: the question panel (1.17.2, the owner's wish): agents ask with ask_owner and the owner answers with a tap. It
is the strip left of the AI terminal (the terminal in the middle, the big keys on the right), or a card at the bottom
left of the canvas when the terminal is closed: who asks, the question and only the asker's options as big buttons (a
yes/no question: Да, Нет; the owner dropped fixed ones, 1.17.3). Questions queue; silence gives the safe default; a
worker's question says so."""

import threading
import time

from test_e15_live_notes import AGENT, DIR, wait

LOG = DIR / "questions-agent.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": True, "command": f"env FAKE_AGENT_LOG={LOG} python3 -u {AGENT}",
                            "wake_idle_ms": 300}}


def widgets(c):
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


def on_screen(c):
    """The widgets really shown (their window and all their parents too)"""
    return {w["name"] for w in c.call("ui_inspect", max_depth=80)["widgets"] if w.get("name")}


def log():
    return LOG.read_text() if LOG.exists() else ""


def shown(ws, name):
    return name in ws and ws[name].get("visible", True)


def test_1_the_panel_is_left_of_the_terminal_and_waits_for_questions(app):
    c = app.client()
    assert wait(lambda: shown(widgets(c), "aiQuestionPanel"))
    ws = widgets(c)
    panel, keys = ws["aiQuestionPanel"]["bbox"], ws["aiTerminalKeys"]["bbox"]
    terms = [w["bbox"] for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]
             if w["type"] == "VteTerminal" and w.get("visible", True) and w["bbox"][2] > 0]
    assert terms, "no terminal shown"
    t = terms[0]
    assert panel[0] + panel[2] <= t[0] + 2 and t[0] + t[2] <= keys[0] + 2, (panel, t, keys)  # (left, middle, right)
    assert ws["aiQuestionText"]["label"] == "Вопросов нет"
    assert not [n for n in ws if n.startswith("aiQuestionOption")] and "aiQuestionYes" not in ws  # (no buttons)
    assert "aiQuestionCard" not in on_screen(c)  # (the terminal is shown: no card on the canvas)


def test_2_options_are_big_buttons_and_a_tap_answers(app):
    c = app.client()
    r = c.call("ask_owner", op="ask", question="Нарисовать схему?", options=["Да, рисовать", "Нет"], wait=False)
    ws = widgets(c)
    assert ws["aiQuestionText"]["label"] == "Нарисовать схему?", ws["aiQuestionText"]
    assert ws["aiQuestionWho"]["label"] == "Claude спрашивает"
    assert ws["aiQuestionOption1"]["label"] == "Да, рисовать" and ws["aiQuestionOption2"]["label"] == "Нет"
    assert c.call("ask_owner", op="get", id=r["id"])["state"] == "waiting"
    c.call("ui_interact", op="click", target=ws["aiQuestionOption1"]["id"])
    a = c.call("ask_owner", op="get", id=r["id"])
    assert a["state"] == "answered" and a["answer"] == "Да, рисовать" and a["by"] == "owner", a
    assert widgets(c)["aiQuestionText"]["label"] == "Вопросов нет"
    # the serving session asked without waiting: it is woken with the answer
    assert wait(lambda: "The owner's answer to your question" in log() and "«Да, рисовать» (owner)" in log()), log()


def test_3_a_yes_no_question_waits_for_the_tap(app):
    c = app.client()
    result = {}

    def ask():
        result["r"] = app.client().call("ask_owner", op="ask", question="Удалить страницу 3?", timeout_s=60)

    th = threading.Thread(target=ask)
    th.start()
    assert wait(lambda: widgets(c)["aiQuestionText"]["label"] == "Удалить страницу 3?")
    ws = widgets(c)
    assert ws["aiQuestionOption1"]["label"] == "Да" and ws["aiQuestionOption2"]["label"] == "Нет", ws
    assert ws["aiQuestionLeft"]["label"].startswith("через "), ws["aiQuestionLeft"]
    c.call("ui_interact", op="click", target=ws["aiQuestionOption1"]["id"])
    th.join(15)
    assert result["r"]["answer"] == "yes" and result["r"]["by"] == "owner", result


def test_4_silence_gives_the_safe_default(app):
    t = time.time()
    r = app.client().call("ask_owner", op="ask", question="Вставить картинку?", options=["Да, вставить"],
                          default="no", timeout_s=10)
    assert r["answer"] == "no" and r["by"] == "timeout" and 9 <= time.time() - t < 20, r


def test_5_a_queue_and_a_worker_s_question(app):
    c = app.client()
    w = app.client(headers={"X-Xournalai-Worker": "T4"})
    a = c.call("ask_owner", op="ask", question="Первый?", wait=False)
    b = w.call("ask_owner", op="ask", question="Какой вариант?", options=["Этот", "Тот"], wait=False)
    ws = widgets(c)
    assert ws["aiQuestionWho"]["label"] == "Claude спрашивает · 1/2", ws["aiQuestionWho"]
    c.call("ui_interact", op="click", target=ws["aiQuestionOption2"]["id"])  # (Нет)
    ws = widgets(c)
    assert ws["aiQuestionWho"]["label"] == "⚙ T4 спрашивает" and ws["aiQuestionText"]["label"] == "Какой вариант?"
    assert not shown(ws, "aiQuestionOption3")  # (only its own two)
    c.call("ui_interact", op="click", target=ws["aiQuestionOption2"]["id"])
    assert c.call("ask_owner", op="get", id=a["id"])["answer"] == "no"
    assert w.call("ask_owner", op="get", id=b["id"])["answer"] == "Тот"
    assert c.call("ask_owner", op="list")["waiting"] == []


def test_6_with_the_terminal_closed_the_card_shows_on_the_canvas(app):
    c = app.client()
    c.call("action_run", action="win.ai-terminal")  # (closes the terminal)
    assert wait(lambda: "aiQuestionPanel" not in on_screen(c))
    assert "aiQuestionCard" not in on_screen(c)  # (no question yet)
    q = c.call("ask_owner", op="ask", question="Карточка видна?", options=["Да, видна"], wait=False)
    assert wait(lambda: "aiQuestionCard" in on_screen(c))
    ws = widgets(c)
    card = ws["aiQuestionCard"]["bbox"]
    canvas = [w for w in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if w["type"] == "GtkXournal"][0]
    cx, cy, cw, ch = canvas["bbox"]
    assert card[0] < cx + cw / 3 and card[1] + card[3] > cy + ch * 2 / 3, (card, canvas["bbox"])  # (bottom left)
    c.call("ui_interact", op="click", target=ws["aiQuestionCardOption1"]["id"])
    assert c.call("ask_owner", op="get", id=q["id"])["answer"] == "Да, видна"
    assert wait(lambda: "aiQuestionCard" not in on_screen(c))
