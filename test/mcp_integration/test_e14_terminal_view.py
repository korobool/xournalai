"""E14: the AI terminal floats over the page with a see-through background; the owner sets how much shows through
(a slider in its header) or docks it under the page (the pin)."""

import os
import subprocess
import time

APP_TERMINAL_FLOATING = True  # (the other tests start with it docked)



def widgets(c, name):
    return [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == name]


def wait(pred, timeout=8):
    end = time.time() + timeout
    while time.time() < end:
        if pred():
            return True
        time.sleep(0.1)
    return False


def view(app):
    path = app.home / "config" / "xournalpp" / "ai-terminal-view.ini"
    return open(path).read() if path.exists() else ""


def shot(app, name):
    """Saves a screenshot of the app's own Xvfb screen when XOAI_SHOTS names a folder (for looking at it)."""
    folder = os.environ.get("XOAI_SHOTS")
    if folder:
        display = app.display
        assert display != os.environ.get("DISPLAY")
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "x11grab", "-i", display, "-frames:v", "1",
                        os.path.join(folder, name + ".png")], env=app.app_env(), check=False, stderr=open(os.path.join(folder, name + ".log"), "w"))


def test_1_floats_over_the_page_70_percent(app):
    c = app.client()
    cx, cy = app.canvas_center(c)
    # ink across the bottom of the page, to see through the terminal
    for k in range(6):
        app.user_drag([(cx - 300 + i * 30, cy + 60 + k * 40 + (i % 2) * 15) for i in range(21)])
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    assert wait(lambda: widgets(c, "aiTerminalDock") and widgets(c, "aiTerminalDock")[0].get("visible", True))
    pin = widgets(c, "aiTerminalPin")[0]
    assert "floating=true" in view(app)  # floating by default
    assert "70%" in [w.get("text") or w.get("label") for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    time.sleep(1)
    shot(app, "floating-70")


def test_2_the_slider_sets_and_remembers_the_opacity(app):
    c = app.client()
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalOpacity")[0]["id"], value=30)
    assert wait(lambda: "opacity=30" in view(app))
    assert "30%" in [w.get("text") or w.get("label") for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"]]
    time.sleep(0.5)
    shot(app, "floating-30")


def test_3_the_pin_docks_it_and_floats_it_again(app):
    c = app.client()
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=True)
    assert wait(lambda: "floating=false" in view(app))
    assert widgets(c, "aiTerminalDock")[0].get("visible", True)
    time.sleep(0.5)
    shot(app, "docked")
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=False)
    assert wait(lambda: "floating=true" in view(app))
    assert widgets(c, "aiTerminalDock")[0].get("visible", True)


def test_4_dragging_the_header_resizes_it_floating_and_docked(app):
    c = app.client()
    if not widgets(c, "aiTerminalCloseTab"):
        c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    assert wait(lambda: widgets(c, "aiTerminalDock")[0]["bbox"][3] > 100)
    for docked in (False, True):
        c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=docked)
        time.sleep(0.5)
        before = widgets(c, "aiTerminalDock")[0]["bbox"][3]
        if docked:  # the row's empty space, left of the controls
            x, y, w, h = widgets(c, "aiTerminalControls")[0]["bbox"]
            gx, gy = x - 40, y + h // 2
        else:  # the title
            x, y, w, h = widgets(c, "aiTerminalHandle")[0]["bbox"]
            gx, gy = x + w // 2, y + h // 2
        step = 10 if docked else -10  # docked in the small test window: no room to grow, so 80 px down (shorter)
        app.user_drag([(gx, gy + k * step) for k in range(9)], step_delay=0.03)
        time.sleep(0.5)
        shot(app, "after-drag-docked" if docked else "after-drag-floating")
        grown = lambda: widgets(c, "aiTerminalDock")[0]["bbox"][3] - before
        assert wait(lambda: (grown() <= -60) if docked else (grown() >= 60)), (
            docked, before, widgets(c, "aiTerminalDock")[0]["bbox"], (x, y, w, h), view(app))
        shot(app, "resized-docked" if docked else "resized-floating")
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=False)


def test_5_moving_without_the_button_does_not_resize(app):
    c = app.client()
    if not widgets(c, "aiTerminalCloseTab"):
        c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    assert wait(lambda: widgets(c, "aiTerminalDock")[0]["bbox"][3] > 100)
    time.sleep(0.5)
    before = widgets(c, "aiTerminalDock")[0]["bbox"][3]
    x, y, w, h = widgets(c, "aiTerminalHandle")[0]["bbox"]
    app.user_drag([(x + w // 2, y + h // 2), (x + w // 2, y + h // 2 - 30)], step_delay=0.05)  # a drag ends…
    time.sleep(0.5)
    after_drag = widgets(c, "aiTerminalDock")[0]["bbox"][3]
    for k in range(8):  # …then the pointer wanders with the button up
        app._xdo("mousemove", x + w // 2, y + h // 2 - 60 - k * 10)
        time.sleep(0.03)
    time.sleep(0.5)
    assert widgets(c, "aiTerminalDock")[0]["bbox"][3] == after_drag, (before, after_drag)
    # and a click on a tab still switches tabs (the row is not all handle)
    c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    time.sleep(0.5)
    labels = [w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name") == "aiTerminalCloseTab"]
    assert len(labels) >= 2


def terminal_shade(app, c):
    """The average brightness (0 … 255) of the floating terminal's empty lower part, grabbed from the Xvfb screen."""
    import tempfile
    from PIL import Image
    assert app.display != os.environ.get("DISPLAY")
    x, y, w, h = widgets(c, "aiTerminalDock")[0]["bbox"]
    png = os.path.join(tempfile.mkdtemp(prefix="xoai-shade-"), "s.png")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "x11grab", "-i", app.display, "-frames:v", "1", png],
                   env=app.app_env(), check=True)
    img = Image.open(png).convert("L").crop((x + w // 2, y + h - 40, x + w // 2 + 200, y + h - 10))
    return sum(img.getdata()) / (img.width * img.height)


def test_6_automatic_colours_go_light_over_a_white_page_and_can_be_switched_off(app):
    c = app.client()
    if not widgets(c, "aiTerminalCloseTab"):
        c.call("action_run", action="win.ai-terminal-open", parameter="shell")
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalPin")[0]["id"], value=False)
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalOpacity")[0]["id"], value=70)
    assert "auto_colors=true" in view(app)  # on by default
    time.sleep(1.5)
    shot(app, "auto-light")
    assert terminal_shade(app, c) > 200  # white at 70% over white paper
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalAutoColors")[0]["id"], value=False)
    assert wait(lambda: "auto_colors=false" in view(app))
    time.sleep(1)
    shot(app, "auto-off-dark")
    assert terminal_shade(app, c) < 120  # black at 70%
    c.call("ui_interact", op="set_value", target=widgets(c, "aiTerminalAutoColors")[0]["id"], value=True)
    assert wait(lambda: "auto_colors=true" in view(app))
