"""E3: with the 'destructive' permission granted, discarding works and leaves a backup."""

import glob
import os

APP_PERMISSIONS = {"read": True, "draw": True, "ui": True, "files": True, "destructive": True}


def test_discard_with_permission_makes_backup(app):
    c = app.client()
    assert c.call("app_status")["mcp"]["permissions"]["destructive"]
    c.call("create_shapes", shapes=[{"type": "line", "from": [0, 0], "to": [50, 50]}], animate=False)
    r = c.call("file_new", on_unsaved="discard")
    assert r["untitled"] and not r["modified"]
    backups = glob.glob(str(app.home / "data" / "xournalpp" / "mcp-backups" / "*before-discard*.xopp"))
    assert backups, "a backup must be written before discarding"
