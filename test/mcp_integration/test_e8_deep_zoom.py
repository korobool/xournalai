"""E8: deep zoom. A toolbar toggle raises the maximum zoom from 700% to 3000%; beyond one buffer's budget pages (and
PDF backgrounds) are rendered only where they are visible, so memory stays flat and the page still looks right."""

import io
import os
import subprocess
import tempfile
import time

from PIL import Image

import xoai

from test_e1_pdf import make_pdf

PDF = tempfile.NamedTemporaryFile(prefix="xoai-", suffix=".pdf", delete=False).name
make_pdf(PDF, ["Deep zoom"])  # Helvetica 18pt at (72, 100) from the top
APP_ARGS = [PDF]


def zoom_to(c, factor, expect=None):
    """Asks for `factor`; returns the zoom once it is `expect` (default: factor), or the zoom after 3 s."""
    expect = expect or factor
    c.call("view", op="zoom", factor=factor)
    deadline = time.time() + 3
    while time.time() < deadline:
        z = c.call("view")["zoom"]
        if about(z, expect):
            return z
        time.sleep(0.1)
    return c.call("view")["zoom"]


def about(a, b):
    return abs(a - b) / b < 0.01


def rss_mb(app, key="VmRSS"):
    """Resident memory of the app in MB (the largest process in its group: xvfb-run starts it as a child); key=VmHWM
    for the peak."""
    pids = subprocess.run(["pgrep", "-g", str(os.getpgid(app.proc.pid))], capture_output=True, text=True).stdout
    rss = 0
    for pid in pids.split():
        try:
            for line in open(f"/proc/{pid}/status"):
                if line.startswith(key + ":"):
                    rss = max(rss, int(line.split()[1]) // 1024)
        except OSError:
            pass
    return rss


def canvas_shot(c):
    w = [x for x in c.call("ui_inspect", max_depth=40, all=True)["widgets"] if x["type"] == "GtkXournal"][0]
    return Image.open(io.BytesIO(xoai.Mcp.images(c.call_raw("ui_screenshot", target=w["id"]))[0])).convert("RGB")


def dark_fraction(img):
    px = img.getdata()
    return sum(1 for r, g, b in px if r + g + b < 200) / len(px)


def test_1_the_maximum_is_700_percent_until_deep_zoom_is_on(app):
    c = app.client()
    assert about(zoom_to(c, 20, expect=7), 7)
    c.call("action_run", action="win.zoom-deep")
    assert about(zoom_to(c, 20), 20)
    settings = (app.home / "config" / "xournalpp" / "settings.xml").read_text()
    assert '<property name="deepZoom" value="true"/>' in settings


def test_2_deep_zoom_renders_the_visible_part(app):
    c = app.client()
    c.call("create_strokes", strokes=[{"points": [[70, 60], [110, 60]], "color": "#d01010ff"}], width=2,
           animate=False)
    zoom_to(c, 30)
    c.call("view", op="scroll", page=1, region=[72, 88, 12, 12])  # the "D" of the PDF text
    time.sleep(1.5)  # rendering runs in the background
    img = canvas_shot(c)
    assert dark_fraction(img) > 0.02, "the PDF text is rendered at deep zoom"
    assert rss_mb(app) < 1500, rss_mb(app)  # a whole A4 page at 3000% would need gigabytes


def test_3_scrolling_renders_the_new_part(app):
    c = app.client()
    zoom_to(c, 30)
    c.call("view", op="scroll", page=1, region=[60, 50, 60, 20])  # the red stroke
    time.sleep(1.5)
    img = canvas_shot(c)
    red = sum(1 for r, g, b in img.getdata() if r > 150 and g < 90 and b < 90)
    assert red > 100, red


def test_4_turning_it_off_brings_the_zoom_back(app):
    c = app.client()
    zoom_to(c, 25)
    c.call("action_run", action="win.zoom-deep")
    time.sleep(0.5)
    assert about(c.call("view")["zoom"], 7)
    assert about(zoom_to(c, 20, expect=7), 7)


def test_5_a_page_sized_filled_highlighter_renders_at_deep_zoom(app):
    # filled highlighter strokes are drawn through a mask of their size, clipped to what is being rendered (else a
    # page-sized stroke at 3000% would get a mask of ~700 megapixels on every render)
    c = app.client()
    c.call("action_run", action="win.zoom-deep")  # test 4 turned it off
    c.call("create_strokes", strokes=[{"points": [[20, 20], [300, 400], [575, 820]]}], tool="highlighter",
           color="#ffee00ff", width=12, fill_opacity=0.5, animate=False)  # filled: drawn through a mask
    zoom_to(c, 30)
    c.call("view", op="scroll", page=1, region=[290, 390, 20, 20])
    time.sleep(1.5)
    img = canvas_shot(c)
    yellow = sum(1 for r, g, b in img.getdata() if r > 200 and g > 200 and b < 150)
    assert yellow > 1000, yellow
    assert rss_mb(app, "VmHWM") < 1500, rss_mb(app, "VmHWM")
