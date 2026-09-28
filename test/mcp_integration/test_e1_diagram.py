"""E1: understanding a sketched diagram (shapes, labels, arrow)."""

import tempfile

import xoai
from xoai import circle_points, glyphs, make_xopp, stroke, wobbly

PATH = tempfile.NamedTemporaryFile(prefix="xoai-diagram-", suffix=".xopp", delete=False).name
PAGE = (
    [stroke(wobbly([(100, 100), (260, 100), (260, 180), (100, 180), (100, 101)], seed=1))]  # box
    + glyphs(130, 130, 5, seed=10)                                                          # label in the box
    + [stroke(wobbly(circle_points(450, 140, 45), 0.8, seed=2))]                           # circle
    + [stroke(wobbly([(262, 140), (402, 140)], 0.3, seed=4))]                              # arrow box -> circle
    + glyphs(100, 400, 12, seed=20)                                                         # a line of "text"
)
make_xopp(PATH, [PAGE])
APP_ARGS = [PATH]


def test_layout_of_diagram(app):
    c = app.client()
    r = c.call("layout_analyze", page=1, include_element_ids=True)
    kinds = [b["kind"] for b in r["blocks"]]
    assert kinds.count("figure") >= 2, r["blocks"]
    assert "handwriting" in kinds
    assert len(r["connectors"]) == 1, r["connectors"]
    con = r["connectors"][0]
    assert con["from_block"] and con["to_block"] and con["from_block"] != con["to_block"]
    label = [b for b in r["blocks"] if b.get("parent")]
    assert label, "label inside the box should name the box as parent"


def test_blocks_render(app):
    c = app.client()
    r = c.call("layout_analyze", page=1)
    ids = [b["id"] for b in r["blocks"]]
    res = c.call_raw("blocks_render", page=1, block_ids=ids[:3], save=False)
    assert len(xoai.Mcp.images(res)) == 3
    assert len(res["structuredContent"]["images"]) == 3
    one = c.call_raw("blocks_render", page=1, element_ids=[r["connectors"][0]["element_id"]], save=False)
    assert len(xoai.Mcp.images(one)) == 1
    assert "block id" in c.call_error("blocks_render", block_ids=["x1"])
    assert "does not exist" in c.call_error("blocks_render", block_ids=["b99"])


def test_shapes_recognize(app):
    c = app.client()
    els = c.call("page_elements", page=1, detail="bbox", limit=500)["elements"]
    box, circle = els[0]["id"], els[6]["id"]
    res = {x["id"]: x for x in c.call("shapes_recognize", element_ids=[box, circle])["results"]}
    assert res[box]["shape"] in ("rectangle", "quadrilateral"), res[box]
    assert res[circle]["shape"] in ("circle", "ellipse"), res[circle]
    assert abs(res[circle]["center"][0] - 450) < 8
