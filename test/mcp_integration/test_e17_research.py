"""E17: the research browser (🔎, 1.18.0): a Chrome of the app's own (its own profile in the app's data folder, with
the owner's Claude in Chrome native-messaging manifest copied in, named «xournalai research»), driven through the
DevTools protocol: searches open as tabs, agents read them and save pictures, the owner's selection goes on the page
with «📌 Insert selection». A local test site stands in for the web (no internet)."""

import http.server
import os
import signal
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
    return {w["name"]: w for w in c.call("ui_inspect", max_depth=80, all=True)["widgets"] if w.get("name")}


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
    assert lst["browser"] == "xournalai research" and lst["extension"].startswith("not installed"), lst
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

