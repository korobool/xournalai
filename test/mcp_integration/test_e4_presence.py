"""E4: where the user is looking and pointing; messages on the canvas."""

import os
import subprocess
import time


def test_pointer_selection_and_message(app):
    c = app.client()
    cx, cy = app.canvas_center(c)
    env = dict(os.environ, DISPLAY=app.display, XAUTHORITY=app._xauthority)
    assert app.display != os.environ.get("DISPLAY")
    subprocess.run(["xdotool", "mousemove", str(cx), str(cy)], env=env, check=True)
    time.sleep(0.2)
    v = c.call("view")
    assert v["pointer"]["page"] == 1 and 0 < v["pointer"]["x"] < 600
    assert v["selection"] is None and v["user_tool"] == "pen"
    r = c.call("create_shapes", shapes=[{"type": "circle", "center": [300, 300], "r": 30}], animate=False,
               layer="current")
    c.call("elements_select", element_ids=[r["created"][0]["id"]])
    sel = c.call("view")["selection"]
    assert sel["count"] == 1 and sel["ids"] == [r["created"][0]["id"]]
    m = c.call("show_message", text="Nice sketch!", x=300, y=260, seconds=1)
    assert m["shown"]
    time.sleep(1.3)  # the callout closes itself
    assert c.call("app_status")["app"] == "xournalai"
