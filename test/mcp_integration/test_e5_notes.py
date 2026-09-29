"""E5: notes memory, kept next to the document and following its pages."""

import json
import os
import tempfile


def test_notes_follow_the_document(app):
    c = app.client()
    tmp = tempfile.mkdtemp(prefix="xoai-notes-")
    path = os.path.join(tmp, "lecture.xopp")
    c.call("page_manage", op="insert", page=1)
    c.call("page_manage", op="insert", page=2)

    # Untitled: kept in memory
    r = c.call("notes", op="set", kind="summary", text="Lecture on thermodynamics")
    assert r["changed"] and not r["saved"]
    c.call("notes", op="set", page=2, kind="transcript", text="dQ = dU + p dV")
    c.call("notes", op="set", page=2, region=[50, 60, 200, 80], kind="meaning", text="First law, closed system")
    assert c.call("notes", op="get")["count"] == 3
    assert c.call("notes", op="set", page=2, kind="transcript", text="dQ = dU + p dV")["changed"] is False

    # Saving the document writes them next to it
    c.call("file_save_as", path=path)
    assert c.call("notes", op="get", page=2)["count"] == 2
    side = json.load(open(path + ".ai-notes.json"))
    assert side["format"] == "xournalai-notes" and len(side["notes"]) == 3

    # Filters
    assert c.call("notes", op="get", kind="meaning")["notes"][0]["region"] == [50, 60, 200, 80]
    assert c.call("notes", op="get", page=2, region=[0, 0, 60, 70])["count"] == 2  # overlaps + page-wide
    assert c.call("notes", op="get", page=2, region=[400, 400, 10, 10])["count"] == 1

    # Pages move: notes follow
    c.call("page_manage", op="move", page=2, to=1)
    got = c.call("notes", op="get", page=1, kind="transcript")["notes"]
    assert got and got[0]["text"].startswith("dQ")
    c.call("notes", op="set", page=1, kind="note", text="moved")  # a change rewrites the sidecar
    pages = {n.get("page") for n in json.load(open(path + ".ai-notes.json"))["notes"]}
    assert pages == {None, 1}

    # Deleting deletes; empty text deletes a note
    assert c.call("notes", op="set", page=1, kind="note", text="")["deleted"]
    assert "A region needs a 'page'" in c.call_error("notes", op="set", region=[0, 0, 1, 1], text="x")

    # Reopening the file loads its notes
    c.call("file_save")
    c.call("file_new")
    assert c.call("notes", op="get")["count"] == 0
    c.call("file_open", path=path)
    notes = c.call("notes", op="get")["notes"]
    assert len(notes) == 3 and {n.get("page") for n in notes} == {None, 1}
