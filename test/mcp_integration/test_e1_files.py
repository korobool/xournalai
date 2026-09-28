"""E1: file management tools."""

import gzip
import os
import tempfile

import xoai
from xoai import glyphs, make_xopp

DIR = tempfile.mkdtemp(prefix="xoai-files-")
SOURCE = make_xopp(os.path.join(DIR, "notes.xopp"), [glyphs(100, 100, 8), glyphs(100, 200, 4)])
APP_ARGS = [SOURCE]


def test_file_info(app):
    info = app.client().call("file_info")
    assert info["path"] == SOURCE and info["page_count"] == 2 and not info["modified"]


def test_save_as_then_reopen(app):
    c = app.client()
    target = os.path.join(DIR, "copy")
    r = c.call("file_save_as", path=target)
    assert r["path"] == target + ".xopp" and os.path.exists(target + ".xopp")
    with gzip.open(target + ".xopp", "rt") as f:
        assert "<xournal" in f.read(500)
    c.call("file_save")  # saves to the new path
    back = c.call("file_open", path=SOURCE)
    assert back["path"] == SOURCE and back["page_count"] == 2
    again = c.call("file_open", path=target + ".xopp", page=2)
    assert again["page_count"] == 2 and again["current_page"] == 2


def test_new_and_close(app):
    c = app.client()
    new = c.call("file_new")
    assert new["untitled"] and new["page_count"] == 1
    assert "untitled" in c.call_error("file_save")
    closed = c.call("file_close")
    assert closed["untitled"]


def test_errors_and_guards(app):
    c = app.client()
    assert "not found" in c.call_error("file_open", path=os.path.join(DIR, "missing.xopp"))
    assert "Folder does not exist" in c.call_error("file_save_as", path="/nonexistent-dir/x.xopp")
    c.call("file_open", path=SOURCE)
    other = make_xopp(os.path.join(DIR, "other.xopp"), [glyphs(10, 10, 2)])
    assert "already exists" in c.call_error("file_save_as", path=other)
    assert "destructive" in c.call_error("file_save_as", path=other, overwrite=True)


def test_recent(app):
    r = app.client().call("file_recent")
    assert isinstance(r["notes"], list) and isinstance(r["pdfs"], list)
