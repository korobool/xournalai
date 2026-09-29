"""E2: private drafts, committed as one undo step."""

import xoai

APP_CONFIG = {"default_layer": "AI"}  # these scenarios exercise the separate AI layer


def layer_names(c):
    return [l["name"] for l in c.call("doc_info")["pages"][0]["layers"]]


def test_draft_lifecycle(app):
    c = app.client()
    d = c.call("draft", op="begin")
    assert d["layer"] in layer_names(c)
    info = c.call("doc_info")["pages"][0]
    draft_layer = [l for l in info["layers"] if l["name"] == d["layer"]][0]
    assert not draft_layer["visible"]  # hidden from the user
    c.call("create_shapes", shapes=[{"type": "rectangle", "x": 100, "y": 100, "w": 120, "h": 80}],
           layer=d["layer"], animate=False)
    c.call("create_text", texts=[{"text": "Draft label", "x": 110, "y": 120}], layer=d["layer"], animate=False)
    r = c.call_raw("draft", op="render", draft=d["draft"], save=False)
    assert len(xoai.Mcp.images(r)) == 1
    assert c.call("draft", op="list")["drafts"][0]["elements"] == 2
    committed = c.call("draft", op="commit", draft=d["draft"], animate=False)
    assert committed["layer_name"] == "AI" and len(committed["created"]) == 2
    assert d["layer"] not in layer_names(c)
    ai = [l for l in c.call("doc_info")["pages"][0]["layers"] if l["name"] == "AI"][0]
    assert ai["elements"] == 2 and ai["visible"]


def test_discard_and_errors(app):
    c = app.client()
    d = c.call("draft", op="begin")
    c.call("create_strokes", strokes=[{"points": [[10, 10], [100, 100]]}], layer=d["layer"], animate=False)
    r = c.call("draft", op="discard", draft=d["draft"])
    assert r["discarded_elements"] == 1 and d["layer"] not in layer_names(c)
    assert "No open draft" in c.call_error("draft", op="render", draft=d["draft"])
    d2 = c.call("draft", op="begin")
    assert "cannot draw into drafts" in c.call_error("pen_draw", layer=d2["layer"],
                                                     strokes=[{"points": [[0, 0], [5, 5]]}])
    assert "empty" in c.call_error("draft", op="commit", draft=d2["draft"])
