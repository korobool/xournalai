"""E9: rendering from page snapshots keeps each page's background (a snapshot once turned every page into a PDF page:
"PDF background missing" everywhere)."""

import io
import time

from PIL import Image

import xoai
from test_e8_deep_zoom import canvas_shot


def ruling(img):
    """Pixels of the light blue lines of a lined page."""
    return sum(1 for r, g, b in img.convert("RGB").getdata() if b > 200 and r < 200 and abs(r - g) < 60 and b - r > 40)


def test_a_lined_page_looks_lined(app):
    c = app.client()
    assert c.call("doc_info")["pages"][0]["background"]["type"] == "lined"
    time.sleep(1.5)
    assert ruling(canvas_shot(c)) > 500  # on screen (page buffer and background rendering)
    img = Image.open(io.BytesIO(xoai.Mcp.images(c.call_raw("page_render", page=1))[0]))
    assert ruling(img) > 500  # what agents see
