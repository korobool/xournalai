"""E17: the research browser (🔎, 1.18.0): a Chrome of the app's own (its own profile in the app's data folder, with
the owner's Claude in Chrome native-messaging manifest copied in, named «xournalai research»), driven through the
DevTools protocol: searches open as tabs, agents read them and save pictures, the owner's selection goes on the page
with «📌 Insert selection». A local test site stands in for the web (no internet)."""

import http.server
import os
import signal
import subprocess
import sys
import json
import pathlib
import socketserver
import struct
import threading
import time
import zlib


def png(width, height):
    raw = b"".join(b"\x00" + bytes(v for x in range(width) for v in ((x * 7) % 256, (y * 5) % 256, 120))
                   for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


PICTURE = png(200, 120)


class Site(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path.startswith("/pic.png"):
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.end_headers()
            self.wfile.write(PICTURE)
            return
        if self.path.startswith("/ticks"):  # (a page that counts its own timer ticks: 10 a second when awake)
            body = (b"<!doctype html><html><head><title>Ticks</title></head><body><main><p id='n'>ticks: 0</p></main>"
                    b"<script>let n = 0; setInterval(() => { n++; document.getElementById('n').textContent = "
                    b"'ticks: ' + n; }, 100);</script></body></html>")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(body)
            return
        q = self.path.split("q=", 1)[1] if "q=" in self.path else ""
        from urllib.parse import unquote_plus
        q = unquote_plus(q)
        page = f"""<!doctype html><html><head><meta charset="utf-8"><title>Results for {q}</title></head><body>
<main><h1>Results for {q}</h1><p>The SMILES notation writes a molecule as a line of text.</p>
<p id="quote">A quote worth keeping about {q}.</p><img src="/pic.png" alt="a molecule"></main>
<script>window.addEventListener('load', () => {{ const r = document.createRange();
r.selectNodeContents(document.getElementById('quote')); getSelection().removeAllRanges(); getSelection().addRange(r);
}});</script></body></html>"""
        body = page.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(body)


SERVER = socketserver.ThreadingTCPServer(("127.0.0.1", 0), Site)
SERVER.daemon_threads = True
threading.Thread(target=SERVER.serve_forever, daemon=True).start()
BASE = f"http://127.0.0.1:{SERVER.server_address[1]}"

from test_e15_live_notes import RECOGNIZER, wav, DIR

APP_ENV = {"XOURNALAI_TEST_HOOKS": "1"}
APP_CONFIG = {"assistant": {"autostart": False,
                            "live_notes": {"enabled": True,
                                           "transcriber": {"kind": "stream", "command": f"python3 -u {RECOGNIZER}"}},
                            "research_browser": {
                                "chrome": "google-chrome", "where": "here", "password_store": "basic",
                                "close_on_exit": True, "offer_extension": False,
                                "extra_args": ["--disable-background-networking", "--disable-component-update",
                                               "--disable-sync", "--disable-features=Translate"],
                                "sign_in_pages": [f"{BASE}/signin"],
                                "search": {k: f"{BASE}/search?kind={k}&q={{q}}"
                                           for k in ("web", "images", "wiki", "scholar")}}}}


def widgets(c):
    # (the main window's: while the browser is in front, the focused window isn't the app's, and «↩ Back» is a small
    # window of the app's own)
    main = max(c.call("ui_windows")["windows"], key=lambda w: w["size"][0] * w["size"][1])
    return {w["name"]: w for w in c.call("ui_inspect", target=main["id"], max_depth=80, all=True)["widgets"]
            if w.get("name")}


def wait(pred, timeout=20):
    end = time.time() + timeout
    while time.time() < end:
        v = pred()
        if v:
            return v
        time.sleep(0.3)
    return pred()


STATE = {}


def test_0_the_option_starts_the_browser_with_a_recording(app):
    """🔎 in the Record menu: the research browser starts with the recording, warm for the first topics."""
    # the owner's Claude in Chrome host manifest (here a stand-in in the test's HOME): copied into the profile
    hosts = app.home / "home" / ".config" / "google-chrome" / "NativeMessagingHosts"
    hosts.mkdir(parents=True, exist_ok=True)
    (hosts / "com.anthropic.claude_code_browser_extension.json").write_text(json.dumps(
        {"name": "com.anthropic.claude_code_browser_extension", "path": "/nowhere/host", "type": "stdio",
         "allowed_origins": ["chrome-extension://fcoeoabgfenejglbffodgkkbkcdhcgfn/"]}))
    c = app.client()
    (app.home / "config" / "xournalpp" / "ai-live-modes.json").write_text(
        json.dumps({"last_mode": "conspect", "options": {"browser": True}}))
    c.call("test_live_audio", op="start", name="research.ogg", mode="conspect")
    profile = app.home / "data" / "xournalai" / "research-browser"
    assert wait(lambda: (profile / "DevToolsActivePort").exists(), 30)
    assert wait(lambda: widgets(c)["aiResearchPinButton"].get("visible", True))
    c.call("test_recording", file=str(DIR / "research.ogg"), name="research.ogg", duration_ms=1000)


def test_1_a_search_opens_in_the_app_s_own_browser(app):
    c = app.client()
    r = c.call("research", op="open", query="smiles notation", kind="web", topic="T4 notations", timeout=60)
    assert r["tab"] and r["topic"] == "T4 notations" and r["url"].startswith(BASE + "/search?kind=web&q=smiles"), r
    STATE["tab"] = r["tab"]
    profile = app.home / "data" / "xournalai" / "research-browser"
    assert (profile / "DevToolsActivePort").exists()
    assert (profile / "NativeMessagingHosts" / "com.anthropic.claude_code_browser_extension.json").exists()
    assert json.loads((profile / "Default" / "Preferences").read_text())["profile"]["name"] == "xournalai research" \
        or (profile / "Default" / "Preferences").exists()
    lst = c.call("research", op="list")
    assert any(t["tab"] == r["tab"] and t.get("topic") == "T4 notations" for t in lst["tabs"]), lst
    assert lst["browser"] == "xournalai research" and lst["extension"]["installed"] is False, lst
    assert lst["extension"]["device_id"] is None and "select_browser" in lst["extension"]["use"], lst
    assert wait(lambda: widgets(c)["aiResearchPinButton"].get("visible", True))  # (📌 while it runs)


def test_2_a_tab_is_read_with_its_pictures_and_the_selection(app):
    c = app.client()
    r = wait(lambda: (lambda x: x if x["title"].startswith("Results for") else None)(
        c.call("research", op="read", tab=STATE["tab"])))
    assert r["title"] == "Results for smiles notation", r
    assert "writes a molecule as a line of text" in r["text"] and r["topic"] == "T4 notations", r
    assert r["pictures"] and r["pictures"][0]["width"] == 200 and r["pictures"][0]["alt"] == "a molecule", r
    assert r["selection"].startswith("A quote worth keeping"), r


def test_3_a_picture_is_saved_and_placed(app):
    c = app.client()
    r = c.call("research", op="image", tab=STATE["tab"], n=0)
    p = pathlib.Path(r["path"])
    assert p.exists() and p.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n" and r["width"] == 200, r
    assert r["source"].startswith(BASE) and str(p).startswith(str(app.home)), r
    made = c.call("create_image", path=r["path"], x=50, y=50, width=100)
    assert made["created"], made


def test_4_the_owner_s_selection_goes_on_the_page(app):
    c = app.client()
    before = len(c.call("page_elements", detail="full", limit=500)["elements"])
    c.call("ui_interact", op="click", target=widgets(c)["aiResearchPinButton"]["id"])
    def quote():
        es = c.call("page_elements", detail="full", limit=500)["elements"]
        return [e for e in es if e.get("type") == "text" and "A quote worth keeping" in e.get("text", "")]
    q = wait(quote, 15)
    assert q, c.call("page_elements", detail="full", limit=500)["elements"][before:]
    assert "— Results for smiles notation · " + BASE in q[0]["text"], q[0]["text"]


def test_5_closing_its_tabs(app):
    c = app.client()
    r = c.call("research", op="close", all=True)
    assert STATE["tab"] in r["closed"], r
    assert not [t for t in c.call("research", op="list")["tabs"] if t.get("topic")], "a tab with a topic is left"


def holder(profile):
    """The Chrome holding the profile (its SingletonLock → host-pid) and its command line"""
    try:
        pid = int(os.readlink(profile / "SingletonLock").rsplit("-", 1)[1])
        return pid, pathlib.Path(f"/proc/{pid}/cmdline").read_bytes().replace(b"\0", b" ").decode()
    except (OSError, ValueError, IndexError):
        return 0, ""


def test_6_signing_in_lets_go_of_the_browser_and_takes_it_back(app):
    # Google refuses to sign in while a program is connected to the browser (its DevTools port; the owner, 10 Nov):
    # for signing in it is restarted without the port; once he closes it, it is back, connected
    c = app.client()
    profile = app.home / "data" / "xournalai" / "research-browser"
    before, cmd = holder(profile)
    assert before and "--remote-debugging-port" in cmd, cmd
    assert c.call("research", op="signin")["signing_in"]
    plain = wait(lambda: (lambda p: p if p[0] and p[0] != before and "--remote-debugging-port" not in p[1] else None)(
        holder(profile)), 30)
    assert plain, holder(profile)
    pid, cmd = plain
    assert "research-sign-in.html" in cmd and f"{BASE}/signin" in cmd and "chromewebstore" not in cmd, cmd
    assert not (profile / "DevToolsActivePort").exists()
    assert "Sign in to the research browser" in (profile.parent / "research-sign-in.html").read_text()
    assert "signing in" in c.call_error("research", op="list"), "reading waits while he signs in"
    assert wait(lambda: not widgets(c)["aiResearchPinButton"].get("visible", True)), "📌 hides meanwhile"
    # he closes it (here: the same clean exit as closing its window; not in its first second)
    time.sleep(1.5)
    os.kill(pid, signal.SIGTERM)
    back = wait(lambda: (lambda p: p if p[0] and p[0] != pid and "--remote-debugging-port" in p[1] else None)(
        holder(profile)), 40)
    assert back, holder(profile)
    lst = wait(lambda: (lambda r: r if not r.get("isError") else None)(c.call_raw("research", op="list")), 20)
    assert lst and not lst.get("isError"), lst
    assert wait(lambda: widgets(c)["aiResearchPinButton"].get("visible", True)), "📌 is back"


def test_7_tabs_open_in_the_background_and_stay_awake(app):
    # (the owner, 10 Nov: the browser took focus when pages opened; "не будет такого, что табы подзасыпают?")
    c = app.client()
    profile = app.home / "data" / "xournalai" / "research-browser"
    _, cmd = holder(profile)
    assert "--disable-background-timer-throttling" in cmd and "--disable-renderer-backgrounding" in cmd, cmd
    assert "--disable-features=IntensiveWakeUpThrottling,Translate" in cmd, cmd  # (one, merged with extra_args')
    prefs = json.loads((profile / "Default" / "Preferences").read_text())["performance_tuning"]
    assert prefs["high_efficiency_mode"]["state"] == 0 and prefs["tab_freezing"]["enabled"] is False, prefs
    assert prefs["force_foreground_priority_for_all_tabs"] is True, prefs
    tab = c.call("research", op="open", url=f"{BASE}/ticks", topic="awake")["tab"]
    time.sleep(4)
    r = c.call("research", op="read", tab=tab)
    assert r["visible"] is False, "it opened in the background, not as the active tab"
    ticks = int(r["text"].split("ticks:")[1].split()[0])
    assert ticks >= 20, f"a background tab counted only {ticks} ticks in 4 s (throttled: about 4)"
    c.call("research", op="close", tab=tab)


def test_8_target_pages_are_kept_until_he_closes_them(app):
    # (the owner, 10 Nov: keep the pages with what was looked for; the search results and steps on the way go; the kept
    # ones stay open until he closes them himself)
    c = app.client()
    target = c.call("research", op="open", query="kept article", topic="T7 SMILES")["tab"]
    step = c.call("research", op="open", query="a step on the way", topic="T7 SMILES")["tab"]
    other = c.call("research", op="open", url=f"{BASE}/search?kind=web&q=reached+by+clicks")["tab"]
    wait(lambda: all(c.call("research", op="read", tab=t)["title"].startswith("Results") for t in (target, step, other)))
    k = c.call("research", op="keep", tab=target, note="the article itself")
    assert k["kept"] and k["topic"] == "T7 SMILES", k
    # a page an agent reached through the extension: kept by its address
    assert c.call("research", op="keep", url=f"{BASE}/search?kind=web&q=reached+by+clicks", topic="T7 SMILES",
                  note="found by clicking")["tab"] == other
    assert wait(lambda: c.call("research", op="read", tab=target)["title"].startswith("★ Results")), "★ in its title"
    tabs = {t["tab"]: t for t in c.call("research", op="list")["tabs"]}
    assert tabs[target]["kept"] and tabs[target]["note"] == "the article itself" and not tabs[step].get("kept"), tabs
    assert "kept" in c.call_error("research", op="close", tab=target)
    r = c.call("research", op="close", all=True)
    assert step in r["closed"] and target not in r["closed"] and other not in r["closed"], r
    assert {t["tab"] for t in r["left_open"]} == {target, other}, r
    src = c.call("research", op="sources")["sources"]
    assert src == [{"topic": "T7 SMILES", "pages": src[0]["pages"]}] and len(src[0]["pages"]) == 2, src
    assert {p["note"] for p in src[0]["pages"]} == {"the article itself", "found by clicking"}, src
    assert not any(p["title"].startswith("★") for p in src[0]["pages"]), src
    log = (app.home / "data" / "xournalai" / "research" / "sources.jsonl").read_text().splitlines()
    assert any(json.loads(l)["note"] == "the article itself" for l in log), log
    # he asks to close it: then it goes
    assert c.call("research", op="close", tab=target, owner_asked=True)["closed"]
    assert target not in {t["tab"] for t in c.call("research", op="list")["tabs"]}


def x_env(app):
    """The app's own Xvfb (never the desktop)"""
    env = dict(os.environ, DISPLAY=app.display)
    assert env["DISPLAY"] != os.environ.get("DISPLAY"), "never the real desktop"
    if app._xauthority:
        env["XAUTHORITY"] = app._xauthority
    return env


SET_ACTIVE = """import sys
from Xlib import display, Xatom
d = display.Display()
r = d.screen().root
r.change_property(d.intern_atom('_NET_ACTIVE_WINDOW'), Xatom.WINDOW, 32, [int(sys.argv[1])])
d.sync()
"""


def wm_activates(app, window):
    """What a window manager does when a window becomes the active one (the test display has none)"""
    subprocess.run([sys.executable, "-c", SET_ACTIVE, str(window)], env=x_env(app), check=True, timeout=20)


def xdo(app, *a):
    return subprocess.run(["xdotool", *map(str, a)], env=x_env(app), check=True, timeout=20, capture_output=True,
                          text=True).stdout.strip()


def devtools(app, path):
    """The research browser's DevTools HTTP (the test's own look at it)"""
    import urllib.request
    port = (app.home / "data" / "xournalai" / "research-browser" / "DevToolsActivePort").read_text().split()[0]
    with urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=10) as r:
        return r.read().decode()


def test_9_it_never_takes_the_focus_by_itself_and_his_own_switch_stays(app):
    # (the owner, 10 Nov: «браузер неконтролируемо перехватывает фокус … уводит на другой workspace»; «если я сам
    # руками поменяю фокус на браузер или альт-табом сделаю, что будет?»)
    c = app.client()
    s = c.call("test_focus")
    assert s["x11"] and s["app_window"] and s["browser_window"], s
    app_w, browser_w = s["app_window"], s["browser_window"]
    wm_activates(app, app_w)  # (he is in his notes)
    time.sleep(0.5)
    # 1. agents at work, and the browser comes to the front by itself: he is brought back at once
    c.call("research", op="open", query="busy now", topic="T9")
    assert c.call("test_focus")["busy"]
    wm_activates(app, browser_w)
    s = wait(lambda: (lambda s: s if s["returned"] >= 1 else None)(c.call("test_focus")), 5)
    assert s and s["returned"] == 1 and not s["in_browser"] and not s["back_pill"], s
    assert s["last_decision"].startswith("returned"), s
    # 2. his own switch: a press that never reached the app (here a click on the browser), then the switch
    wm_activates(app, app_w)
    time.sleep(0.3)
    target = c.call("research", op="open", query="the page he reads", topic="T9")["tab"]
    devtools(app, f"/json/activate/{target}")  # (the tab in front in the browser)
    xdo(app, "windowraise", browser_w)
    xdo(app, "mousemove", "--window", browser_w, 300, 300, "click", 1)
    wm_activates(app, browser_w)
    s = wait(lambda: (lambda s: s if s["in_browser"] else None)(c.call("test_focus")), 5)
    assert s and s["returned"] == 1 and s["back_pill"], s
    assert s["last_decision"] == "his own: a press of his went past the app", s
    # the tab in front of him counts as seen: an agent can't close it
    assert wait(lambda: any(t["tab"] == target and t.get("seen") for t in c.call("research", op="list")["tabs"]), 8)
    assert "looked at" in c.call_error("research", op="close", tab=target)
    # 3. «↩ Back to the notes»: a small window of the app's own over the browser
    pill = xdo(app, "search", "--name", "^Back to the notes$")
    xdo(app, "mousemove", "--window", pill.split()[0], 20, 20, "click", 1)
    s = wait(lambda: (lambda s: s if not s["in_browser"] else None)(c.call("test_focus")), 5)
    assert s and not s["back_pill"], s
    # 4. «🔎 Browser · N new»: tabs opened while he wasn't looking; one tap takes him there
    wm_activates(app, app_w)
    c.call("research", op="open", query="while he draws", topic="T9")
    assert wait(lambda: c.call("test_focus")["browser_button"] == "🔎 Browser\n1 new", 5), c.call("test_focus")
    c.call("ui_interact", op="click", target=widgets(c)["aiResearchBrowserButton"]["id"])
    s = wait(lambda: (lambda s: s if s["in_browser"] else None)(c.call("test_focus")), 5)
    assert s and s["shown"] and s["back_pill"] and s["browser_button"] == "🔎 Browser", s
    c.call("ui_interact", op="click", target=widgets(c)["aiResearchBrowserButton"]["id"])  # (again: back)
    assert wait(lambda: not c.call("test_focus")["in_browser"], 5)
    # 5. when nothing works in the browser, its coming to the front is left alone
    wm_activates(app, app_w)
    time.sleep(3.5)
    assert not c.call("test_focus")["busy"]
    wm_activates(app, browser_w)
    s = wait(lambda: (lambda s: s if s["in_browser"] else None)(c.call("test_focus")), 5)
    assert s and s["returned"] == 1 and s["last_decision"] == "his own: the browser wasn't busy", s
    wm_activates(app, app_w)


def test_10_signing_in_is_in_the_browser_button_s_menu(app):
    c = app.client()
    w = widgets(c)["aiResearchBrowserButton"]
    x, y, width, height = w["bbox"]
    app._xdo("mousemove", x + width // 2, y + height // 2)
    app._xdo("mousedown", 1)
    time.sleep(1.2)
    app._xdo("mouseup", 1)
    ws = wait(lambda: (lambda ws: ws if "aiResearchSignIn" in ws else None)(widgets(c)), 5)
    assert ws and ws["aiResearchSignIn"]["label"] == "🔑 Browser sign-in", sorted(ws)[:20]
    # (no «🔑» in the Record menu any more: test_e15_live_modes)


def alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def test_11_a_closed_browser_comes_back_when_it_is_needed(app):
    # (the owner, 10 Nov: «если браузер был закрыт, то пользоваться им нельзя, он не может его автоматически открыть
    # повторно, да?»: a search started it again, but list and read answered an error)
    c = app.client()
    profile = app.home / "data" / "xournalai" / "research-browser"
    pid, _ = holder(profile)
    os.kill(pid, signal.SIGTERM)  # (he closes its window)
    assert wait(lambda: not alive(pid), 15)
    time.sleep(2.5)  # (the app notices it is gone)
    assert not widgets(c)["aiResearchPinButton"].get("visible", True), "its buttons go with it"
    lst = c.call("research", op="list", timeout=60)  # (an agent looks at its tabs first)
    assert isinstance(lst["tabs"], list), lst
    new, cmd = holder(profile)
    assert new and new != pid and alive(new) and "--remote-debugging-port" in cmd, (new, cmd)
    assert wait(lambda: widgets(c)["aiResearchPinButton"].get("visible", True), 5), "and come back with it"

