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


def png_size(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


def test_page_render_whole_page_and_file(app):
    c = app.client()
    r = c.call_raw("page_render", page=1, max_px=800)
    meta = r["structuredContent"]
    w, h = png_size(xoai.Mcp.images(r)[0])
    assert (w, h) == (meta["width_px"], meta["height_px"]) and max(w, h) <= 800
    assert meta["region"][0] == 0 and meta["px_per_pt"] > 0
    with open(meta["file"], "rb") as f:
        assert png_size(f.read()) == (w, h)


def test_page_render_region_grid_highlight(app):
    c = app.client()
    e = c.call("page_elements", page=1, detail="bbox", limit=1)["elements"][0]
    r = c.call_raw("page_render", page=1, region=[40, 40, 300, 150], dpi=144, grid=True, highlight=[e["id"]],
                   save=False)
    meta = r["structuredContent"]
    assert meta["region"] == [40, 40, 300, 150] and abs(meta["px_per_pt"] - 2.0) < 0.01
    assert png_size(xoai.Mcp.images(r)[0]) == (600, 300)
    assert "file" not in meta


def test_page_render_errors(app):
    c = app.client()
    assert "outside the page" in c.call_error("page_render", region=[5000, 5000, 10, 10])
    assert "does not exist" in c.call_error("page_render", layers=[9])
    assert "Unknown element id" in c.call_error("page_render", highlight=["e999999"])


def test_layout_analyze_handwriting(app):
    c = app.client()
    r = c.call("layout_analyze", page=1)
    kinds = [b["kind"] for b in r["blocks"]]
    assert "handwriting" in kinds and "figure" not in kinds, kinds
    main = max(r["blocks"], key=lambda b: b["element_count"])
    assert len(main["lines"]) > 10
    assert 4 < r["typical_stroke_height"] < 20
    ids = c.call("layout_analyze", page=1, region=[40, 40, 250, 60], include_element_ids=True)
    assert all(len(b["element_ids"]) == b["element_count"] for b in ids["blocks"])
