"""E8: pinch zoom on a touchscreen follows the fingers, even when a gesture daemon (e.g. Touchégg, as on Pop!_OS)
turns the same pinch into Ctrl+KP_Add / Ctrl+KP_Subtract keystrokes."""

import time

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}

X = 500  # fingers move vertically around (X, CY) in canvas widget coordinates
CY = 400


def zoom(c):
    return c.call("view", op="get")["zoom"]


def pinch(app, c, d0, d1, steps=10, keys_at=None, keys=()):
    """Two fingers at distance d0 spread (or close) to d1; `keys` are pressed after step `keys_at`."""
    c.call("test_touch", op="begin", finger=0, x=X, y=CY - d0 / 2)
    c.call("test_touch", op="begin", finger=1, x=X, y=CY + d0 / 2)
    c.call("test_touch", op="update", finger=0, x=X, y=CY - d0 / 2)  # the first motion starts the zoom
    for i in range(1, steps + 1):
        d = d0 + (d1 - d0) * i / steps
        c.call("test_touch", op="update", finger=0, x=X, y=CY - d / 2)
        c.call("test_touch", op="update", finger=1, x=X, y=CY + d / 2)
        if i == keys_at:
            for k in keys:
                app.user_key(k)
            time.sleep(0.5)  # zoom actions run from the main loop
    c.call("test_touch", op="end", finger=0, x=X, y=CY - d1 / 2)
    c.call("test_touch", op="end", finger=1, x=X, y=CY + d1 / 2)


def close(a, b):
    return abs(a - b) / b < 0.02


def test_1_pinch_follows_the_fingers(app):
    c = app.client()
    z0 = zoom(c)
    pinch(app, c, 200, 300)
    assert close(zoom(c), z0 * 1.5), (z0, zoom(c))
    pinch(app, c, 300, 200)
    assert close(zoom(c), z0), (z0, zoom(c))


def test_2_zoom_keys_during_a_pinch_are_ignored(app):
    c = app.client()
    z0 = zoom(c)
    pinch(app, c, 200, 300, keys_at=5, keys=["ctrl+KP_Add", "ctrl+KP_Add"])
    assert close(zoom(c), z0 * 1.5), (z0, zoom(c))
    pinch(app, c, 300, 200, keys_at=3, keys=["ctrl+KP_Subtract"])
    assert close(zoom(c), z0), (z0, zoom(c))


def test_3_zoom_keys_work_again_after_the_pinch(app):
    c = app.client()
    pinch(app, c, 200, 240)
    time.sleep(0.8)
    z0 = zoom(c)
    app.user_key("ctrl+KP_Add")
    time.sleep(0.5)
    assert close(zoom(c), z0 * 1.1), (z0, zoom(c))
