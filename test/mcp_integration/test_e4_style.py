"""E4: drawing additions in the user's own pen style."""

import math
import os
import tempfile

from xoai import make_xopp, stroke


def user_like(y):
    pts = [(80 + i * 2.0, y + 6 * math.sin(i / 9)) for i in range(120)]
    # heavy hand: strong middle, long taper at the end
    widths = [2.4 * (0.25 + 0.75 * min(1, i / 10) * min(1, (119 - i) / 30)) for i in range(120)]
    return stroke(pts, color="#aa2200ff", width=2.4, widths=widths)


PATH = make_xopp(os.path.join(tempfile.mkdtemp(), "mine.xopp"), [[user_like(100 + 30 * k) for k in range(6)]])
APP_ARGS = [PATH]


def test_user_style_is_learned(app):
    c = app.client()
    st = c.call("user_style")
    assert st["strokes_analyzed"] == 6 and abs(st["width"] - 2.4) < 0.01 and st["color"] == "#aa2200"
    o = st["profile_options"]
    assert 0.8 < o["base"] <= 1.0 and o["min"] < 0.4 and o["taper_out"] > o["taper_in"]


def test_match_user_drawing(app):
    c = app.client()
    r = c.call("create_strokes", strokes=[{"points": [[80, 400], [320, 400]]}], profile="match_user", animate=False)
    e = [x for x in c.call("page_elements", detail="full", limit=100)["elements"] if x["id"] == r["created"][0]["id"]][0]
    assert e["color"] == "#aa2200" and abs(e["width"] - 2.4) < 0.01 and e["pressure"]
    ws = [p[2] for p in e["points"]]
    # long end taper like the user's: the last 20 points are thinner than the middle
    assert sum(ws[-20:]) / 20 < 0.8 * ws[len(ws) // 2]
    p = c.call("pen_draw", strokes=[{"points": [[80, 450], [320, 450]]}], profile="match_user", speed=0)
    assert len(p["created"]) == 1
