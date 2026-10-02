"""E12: a hang is written down while it lasts (a frozen app may be killed before it ends): the stall trace gets a
HANG line with what the UI thread was doing and the stack of every thread. Zone statuses are cut by whole characters,
so a long non-English status never breaks the window's painting (invalid UTF-8 puts cairo in an error state)."""

import pathlib
import tempfile
import time

TRACE = pathlib.Path(tempfile.mkdtemp(prefix="xoai-hang-")) / "stalls.log"
APP_ENV = {"XOURNALAI_TEST_HOOKS": "1", "XOURNALAI_TRACE_STALLS": str(TRACE)}


def test_1_a_hang_is_reported_with_the_stacks(app):
    c = app.client()
    c.call("test_block_ui", ms=3500)
    deadline = time.time() + 10
    while time.time() < deadline and " stall 3" not in TRACE.read_text():  # (the block may start after the reply)
        time.sleep(0.2)
    log = TRACE.read_text()
    assert " start pid " in log.splitlines()[0], log[:200]
    assert " HANG 2s so far: " in log, log
    stacks = log.split(" HANG ", 1)[1]
    assert "-- thread" in stacks and "(UI)" in stacks, stacks[:500]
    ui = stacks.split("(UI)", 1)[1].split("-- thread", 1)[0]
    assert len([line for line in ui.splitlines() if "xournalpp" in line or "lib" in line]) >= 5, ui
    assert " stall 3" in log  # (and the stall itself once it ended)


def test_2_a_long_cyrillic_status_does_not_break_painting(app):
    c = app.client()
    z = c.call("thinking", op="start", text="переписываю страницу 2 крупным шрифтом, чтобы всё нормально читалось "
                                            "на экране → и подписи в квадратиках тоже")["id"]
    time.sleep(1.5)
    c.call("thinking", op="done", id=z)
    assert "not valid UTF-8" not in app.read_log()
