"""E3: every application action is reachable."""


def test_list_has_menu_labels_and_states(app):
    c = app.client()
    r = c.call("actions_list")
    names = {a["name"]: a for a in r["actions"]}
    assert r["count"] > 100
    assert names["win.open"]["menu"].startswith("File/")
    assert "state" in names["win.fullscreen"]
    assert isinstance(names["win.zoom"]["state"], float)  # zoom is set through its state
    zoom = c.call("actions_list", filter="zoom")
    assert all("zoom" in (a["name"] + a.get("menu", "")).lower() for a in zoom["actions"])


def test_run_actions(app):
    c = app.client()
    pages = c.call("doc_info")["page_count"]
    c.call("action_run", action="win.new-page-after")
    assert c.call("doc_info")["page_count"] == pages + 1
    c.call("action_run", action="undo")
    assert c.call("doc_info")["page_count"] == pages
    r = c.call("action_run", action="win.zoom", state=1.5)
    assert abs(r["state"] - 1.5) < 0.01
    layers = len(c.call("doc_info")["pages"][0]["layers"])
    c.call("action_run", action="layer-new-above-current")
    assert len(c.call("doc_info")["pages"][0]["layers"]) == layers + 1


def test_state_and_errors(app):
    c = app.client()
    r = c.call("action_run", action="win.grid-snapping", state=True)
    assert r["state"] is True
    r = c.call("action_run", action="win.grid-snapping", state=False)
    assert r["state"] is False
    assert "Unknown action" in c.call_error("action_run", action="win.fly-to-moon")
    assert "needs a 'parameter'" in c.call_error("action_run", action="win.select-tool")
    assert "destructive" in c.call_error("action_run", action="app.quit")
    assert "destructive" in c.call_error("action_run", action="quit")
