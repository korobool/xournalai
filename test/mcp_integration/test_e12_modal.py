"""E12: while a modal dialog runs (Print holds the document lock all the while), tools are refused at once with a clear
reason instead of deadlocking the app (2026-10-05: Ctrl+P during agent work froze it for good)."""

import time

import xoai

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}


def test_1_tools_wait_for_a_dialog_instead_of_deadlocking(app):
    c = app.client()
    c.call("page_manage", op="insert")
    c.call("test_modal", ms=3000)  # opens right after answering, like Ctrl+P
    time.sleep(0.5)  # the "dialog" is open
    other = app.client()
    begin = time.time()
    err = other.call_error("transaction_begin", label="x", page=1)  # would take the write lock
    assert time.time() - begin < 2, "should answer at once"
    assert "showing a dialog" in err, err
    time.sleep(3)
    # afterwards everything works again
    tr = other.call("transaction_begin", label="x", page=1)
    other.call("transaction_abort", transaction=tr["transaction"])
