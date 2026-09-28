"""E1 acceptance: an agent-style walk through 'understand my page' on real handwriting."""

import xoai

APP_ARGS = [xoai.ROOT / "test/files/benchmark/handwritten-text.xopp"]


def test_understanding_recipe(app):
    c = app.client()
    status = c.call("app_status")
    assert status["document"]["page_count"] >= 1
    guide = c.call("guide", topic="summarize")
    assert "layout_analyze" in guide
    layout = c.call("layout_analyze", page=1)
    handwriting = [b for b in layout["blocks"] if b["kind"] == "handwriting"]
    assert handwriting
    lines = c.call_raw("blocks_render", page=1, block_ids=[handwriting[0]["id"]], split_lines=True, max_images=5,
                       save=False)
    images = xoai.Mcp.images(lines)
    assert len(images) == 5
    # Each line crop is wide and flat, i.e. one line of text at readable resolution
    for meta in lines["structuredContent"]["images"]:
        x, y, w, h = meta["region"]
        assert w > 3 * h and meta["px_per_pt"] >= 2
    overview = c.call_raw("page_render", page=1, max_px=1000, save=False)
    assert len(xoai.Mcp.images(overview)) == 1
    exported = c.call("export", format="xjson", pages="1", region=[40, 40, 200, 40], delivery="inline")
    assert exported["elements"]
