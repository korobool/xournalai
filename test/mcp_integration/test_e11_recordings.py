"""E11: when a Xournal++ recording ends, the serving session hears about it; strokes written during a recording show
their moment in it (to align a transcript with the ink)."""

import tempfile

import xoai

DIR = tempfile.mkdtemp(prefix="xoai-rec-")
DOC = xoai.make_xopp(f"{DIR}/lecture.xopp", [[
    {**xoai.stroke([(50, 50), (120, 60)]), "audio": ("rec1.ogg", 1.5)},
    {**xoai.stroke([(50, 90), (150, 95)]), "audio": ("rec1.ogg", 4.2)},
    xoai.stroke([(50, 140), (150, 150)]),
]])
APP_ARGS = [DOC]
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def test_1_strokes_know_their_moment_in_the_recording(app):
    c = app.client()
    els = c.call("page_elements", page=1, detail="bbox")["elements"]
    with_audio = [e for e in els if "audio" in e]
    assert len(with_audio) == 2 and len(els) == 3, els
    assert sorted(e["audio"]["t"] for e in with_audio) == [1.5, 4.2]
    assert all(e["audio"]["file"].endswith("rec1.ogg") for e in with_audio)


def test_2_a_finished_recording_is_announced(app):
    c = app.client()
    cursor = c.call("changes_get")["cursor"]
    assert c.call("test_recording", file=f"{DIR}/rec1.ogg", name="rec1.ogg", duration_ms=65000)["observed"]
    events = [e for e in c.call("changes_get", since=cursor)["events"] if e["type"] == "audio_recorded"]
    assert len(events) == 1, events
    e = events[0]
    ids = [x["id"] for x in c.call("page_elements", page=1, detail="bbox")["elements"] if "audio" in x]
    assert sorted(e["ids"]) == sorted(ids) and e["page"] == 1
    for part in (f"{DIR}/rec1.ogg", "(1:05, page 1; 2 stroke(s) written meanwhile", "audio_transcribe",
                 "For your information"):
        assert part in e["step"], (part, e["step"])
