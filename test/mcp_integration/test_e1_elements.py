"""E1: reading content of a real handwritten document."""

import xoai

APP_ARGS = [xoai.ROOT / "test/files/benchmark/handwritten-text.xopp"]


def test_elements_bbox_overview(app):
    c = app.client()
    info = c.call("doc_info")
    assert info["path"].endswith("handwritten-text.xopp")
    r = c.call("page_elements", page=1, detail="bbox", limit=5000)
    assert r["total"] > 20, r["total"]
    e = r["elements"][0]
    assert e["id"].startswith("e") and e["type"] == "stroke"
    assert len(e["bbox"]) == 4 and "points" not in e


def test_ids_are_stable_and_simplified_points_are_fewer(app):
    c = app.client()
    a = c.call("page_elements", page=1, detail="simplified", tolerance=1.0, limit=10)
    b = c.call("page_elements", page=1, detail="full", limit=10)
    assert [e["id"] for e in a["elements"]] == [e["id"] for e in b["elements"]]
    simplified = sum(len(e["points"]) for e in a["elements"])
    full = sum(len(e["points"]) for e in b["elements"])
    assert simplified < full
    assert sum(e["point_count"] for e in b["elements"]) == full


def test_pagination_region_and_filters(app):
    c = app.client()
    first = c.call("page_elements", page=1, detail="bbox", limit=3)
    assert first["returned"] == 3 and first["next_offset"] == 3
    second = c.call("page_elements", page=1, detail="bbox", offset=3, limit=3)
    assert {e["id"] for e in first["elements"]}.isdisjoint({e["id"] for e in second["elements"]})
    x, y, w, h = first["elements"][0]["bbox"]
    region = c.call("page_elements", page=1, detail="bbox", region=[x, y, w, h], limit=5000)
    assert first["elements"][0]["id"] in [e["id"] for e in region["elements"]]
    assert region["total"] < first["total"]
    texts = c.call("page_elements", page=1, types=["text"], detail="bbox")
    assert all(e["type"] == "text" for e in texts["elements"])


def test_errors_are_helpful(app):
    c = app.client()
    assert "does not exist" in c.call_error("page_elements", page=999)
    assert "must be one of" in c.call_error("page_elements", detail="everything")
    assert "no PDF background" in c.call_error("pdf_text", page=1)
