"""E3: permission tiers and safety backups (default permissions: no 'destructive')."""

import os
import time

import xoai


def test_destructive_operations_need_permission(app):
    c = app.client()
    assert not c.call("app_status")["mcp"]["permissions"]["destructive"]
    c.call("create_shapes", shapes=[{"type": "line", "from": [0, 0], "to": [50, 50]}], animate=False)
    assert "destructive" in c.call_error("file_new", on_unsaved="discard")
    assert "unsaved changes" in c.call_error("file_new")
    c.call("ui_menu_select", path="File/New", visible=False)
    prompt = c.call("ui_wait_for_window", kind="dialog")["window"]
    discard = [w for w in c.call("ui_inspect", target=prompt["id"])["widgets"]
               if w.get("label") == "Discard" and w["role"] == "button"][0]
    assert "destructive" in c.call_error("ui_interact", op="click", target=discard["id"])
    cancel = [w for w in c.call("ui_inspect", target=prompt["id"])["widgets"]
              if w.get("label") == "Cancel" and w["role"] == "button"][0]
    c.call("ui_interact", op="click", target=cancel["id"])


def test_backups_before_risky_operations(app):
    c = app.client()
    c.call("page_manage", op="insert", page=1)
    r = c.call("page_manage", op="delete", page=2)
    assert xoai.wait_for_file(r["backup"]) and r["backup"].endswith(".xopp")
    many = c.call("create_strokes", strokes=[{"points": [[10, 10 + i], [100, 10 + i]]} for i in range(25)],
                  animate=False)
    d = c.call("elements_delete", operation=many["operation"])
    assert d["deleted"] == 25 and xoai.wait_for_file(d["backup"])
    few = c.call("create_strokes", strokes=[{"points": [[10, 300], [100, 300]]}], animate=False)
    assert "backup" not in c.call("elements_delete", operation=few["operation"])
