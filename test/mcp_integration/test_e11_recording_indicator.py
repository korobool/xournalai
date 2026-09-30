"""E11: while the Xournal++ recorder records, the status line shows it (dot, level, time, Stop), so it is not
forgotten. The test hook switches the indicator as the recorder would, without opening the microphone."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def widgets(c):
    return c.call("ui_inspect", max_depth=80, all=True)["widgets"]


def find(c, name):
    for w in widgets(c):
        if w.get("name") == name:
            return w
    return None


def shown(w):
    return w is not None and w.get("visible", True)


def test_1_hidden_while_nothing_records(app):
    c = app.client()
    assert not shown(find(c, "recordingIndicator"))


def test_2_recording_shows_in_the_ai_status_line(app):
    c = app.client()
    assert c.call("test_recorder", on=True)["recording"]
    time.sleep(0.3)
    ind, status = find(c, "recordingIndicator"), find(c, "mcpStatus")
    assert shown(ind), ind
    # one status line: the indicator sits in the AI strip, next to its text
    iy, ih = ind["bbox"][1], ind["bbox"][3]
    sy, sh = status["bbox"][1], status["bbox"][3]
    assert abs((iy + ih / 2) - (sy + sh / 2)) < max(ih, sh) / 2, (ind["bbox"], status["bbox"])
    assert ind["bbox"][0] < status["bbox"][0], (ind["bbox"], status["bbox"])
    assert find(c, "recordingIndicatorLabel")["label"].startswith("Recording 0:0")
    assert shown(find(c, "recordingIndicatorStop"))


def test_3_the_time_runs(app):
    c = app.client()
    time.sleep(2.2)
    label = find(c, "recordingIndicatorLabel")["label"]
    assert label in ("Recording 0:02", "Recording 0:03"), label


def test_4_stop_ends_it(app):
    c = app.client()
    c.call("ui_interact", op="click", target=find(c, "recordingIndicatorStop")["id"])
    time.sleep(0.3)
    assert not shown(find(c, "recordingIndicator"))


def test_5_starts_again_from_zero(app):
    c = app.client()
    c.call("test_recorder", on=True)
    time.sleep(0.3)
    assert find(c, "recordingIndicatorLabel")["label"] == "Recording 0:00"
    c.call("test_recorder", on=False)
    time.sleep(0.2)
    assert not shown(find(c, "recordingIndicator"))
