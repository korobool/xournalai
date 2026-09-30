"""E12: after the pen (or a touch), moving the mouse, touchpad or TrackPoint brings a visible cursor back: the app
applies it again even if its cache says nothing changed (something else may have hidden it). Ctrl does it too. The
touch trace records the device switches and each cursor applied, to diagnose it on real hardware."""

import pathlib
import tempfile
import time

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-cursor-")) / "trace.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TRACE_TOUCH": str(TRACE)}


def cursor_lines():
    return [line.strip().split(" ", 1)[1] for line in TRACE.read_text().splitlines() if " cursor: " in line]


def test_1_the_mouse_after_the_pen_gets_its_cursor_back(app):
    c = app.client()
    c.call("test_pen", op="hover", x=400, y=300)
    c.call("test_pen", op="hover", x=410, y=300)
    time.sleep(0.3)
    assert "cursor: device mouse -> pen" in cursor_lines(), cursor_lines()
    x, y = app.canvas_center(c)
    app._xdo("mousemove", x, y)
    app._xdo("mousemove", x + 20, y + 10)
    time.sleep(0.5)
    lines = cursor_lines()
    back = lines.index("cursor: device pen -> mouse")
    assert any(line.startswith("cursor: set ") for line in lines[back + 1:]), lines


def test_2_ctrl_applies_the_cursor_again(app):
    c = app.client()
    before = len(cursor_lines())
    app.user_key("ctrl")
    time.sleep(0.5)
    assert any(line.startswith("cursor: set ") for line in cursor_lines()[before:]), cursor_lines()[before:]
