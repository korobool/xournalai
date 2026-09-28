"""End-to-end harness for the xournalai MCP server (Python standard library only).

App  - launches the built xournalpp headless (xvfb-run) with an isolated config and a free port
Mcp  - minimal Streamable HTTP MCP client (initialize, tools/list, tools/call, prompts, resources)
"""

import base64
import gzip
import math
import random
import json
import os
import pathlib
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
BINARY = ROOT / "build" / "xournalpp"


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class App:
    """Runs xournalpp under Xvfb with its own XDG directories. Use as a context manager."""

    def __init__(self, *args, binary=BINARY):
        self.args = [str(a) for a in args]
        self.binary = str(binary)
        self.port = free_port()
        self.tmp = tempfile.TemporaryDirectory(prefix="xoai-")
        self.home = pathlib.Path(self.tmp.name)
        self.proc = None

    @property
    def env(self):
        env = dict(os.environ)
        for key in ("CONFIG", "DATA", "CACHE"):
            d = self.home / key.lower()
            d.mkdir(exist_ok=True)
            env[f"XDG_{key}_HOME"] = str(d)
        return env

    @property
    def config_file(self):
        return self.home / "config" / "xournalpp" / "mcp.json"

    def start(self, timeout=30):
        self.log = open(self.home / "app.log", "w")
        cmd = ["xvfb-run", "-a", "-s", "-screen 0 1600x1000x24", self.binary, "--mcp", f"--mcp-port={self.port}",
               *self.args]
        self.proc = subprocess.Popen(cmd, env=self.env, stdout=self.log, stderr=subprocess.STDOUT,
                                     stdin=subprocess.DEVNULL, start_new_session=True)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError("xournalpp exited early:\n" + self.read_log())
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{self.port}/", timeout=1).read()
                if self.config_file.exists():
                    self.token = json.loads(self.config_file.read_text())["token"]
                    return self
            except (urllib.error.URLError, ConnectionError, OSError):
                pass
            time.sleep(0.25)
        raise TimeoutError("MCP server did not come up:\n" + self.read_log())

    def read_log(self):
        try:
            return (self.home / "app.log").read_text()[-4000:]
        except OSError:
            return ""

    def stop(self):
        if self.proc and self.proc.poll() is None:
            os.killpg(self.proc.pid, signal.SIGTERM)
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(self.proc.pid, signal.SIGKILL)
        if getattr(self, "log", None):
            self.log.close()

    def client(self, **kw):
        return Mcp(f"http://127.0.0.1:{self.port}/mcp", self.token, **kw)

    def __enter__(self):
        return self.start()

    def __exit__(self, *exc):
        self.stop()
        self.tmp.cleanup()


class McpError(Exception):
    pass


class Mcp:
    def __init__(self, url, token, name="xoai-tests", initialize=True):
        self.url, self.token, self.session, self.next_id = url, token, None, 1
        if initialize:
            self.init = self.request("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                                    "clientInfo": {"name": name, "version": "1"}})
            self.notify("notifications/initialized")

    def _post(self, payload, timeout=120):
        req = urllib.request.Request(self.url, data=json.dumps(payload).encode(), method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("Accept", "application/json, text/event-stream")
        req.add_header("Authorization", f"Bearer {self.token}")
        if self.session:
            req.add_header("Mcp-Session-Id", self.session)
        try:
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                sid = resp.headers.get("Mcp-Session-Id")
                if sid:
                    self.session = sid
                body = resp.read()
                return resp.status, json.loads(body) if body else None
        except urllib.error.HTTPError as e:
            body = e.read()
            return e.code, json.loads(body) if body else None

    def request(self, method, params=None, timeout=120):
        rid = self.next_id
        self.next_id += 1
        status, body = self._post({"jsonrpc": "2.0", "id": rid, "method": method, "params": params or {}}, timeout)
        if status != 200 or body is None:
            raise McpError(f"{method}: HTTP {status} {body}")
        if "error" in body:
            raise McpError(f"{method}: {body['error']}")
        return body["result"]

    def notify(self, method, params=None):
        return self._post({"jsonrpc": "2.0", "method": method, "params": params or {}})[0]

    def tools(self):
        return {t["name"]: t for t in self.request("tools/list")["tools"]}

    def call_raw(self, name, timeout=120, **args):
        return self.request("tools/call", {"name": name, "arguments": args}, timeout)

    def call(self, name, timeout=120, **args):
        """Calls a tool; returns structuredContent (or text), raises McpError if the tool reported an error."""
        result = self.call_raw(name, timeout, **args)
        if result.get("isError"):
            raise McpError(f"{name}: " + " ".join(c.get("text", "") for c in result["content"]))
        if "structuredContent" in result:
            return result["structuredContent"]
        return "\n".join(c.get("text", "") for c in result["content"] if c["type"] == "text")

    def call_error(self, name, **args):
        """Calls a tool that is expected to fail; returns the error text."""
        result = self.call_raw(name, **args)
        assert result.get("isError"), f"{name} was expected to fail but returned {result}"
        return " ".join(c.get("text", "") for c in result["content"])

    @staticmethod
    def images(result):
        return [base64.b64decode(c["data"]) for c in result["content"] if c["type"] == "image"]


# ---- test documents ---------------------------------------------------------------------------------------------

def stroke(points, color="#000000ff", width=1.41, tool="pen"):
    """A stroke for make_xopp: points are (x, y) tuples."""
    return {"points": points, "color": color, "width": width, "tool": tool}


def wobbly(points, amount=0.6, seed=1, step=2.0):
    """Densifies a polyline and adds hand-like noise."""
    rnd = random.Random(seed)
    out = []
    for (x1, y1), (x2, y2) in zip(points, points[1:]):
        n = max(1, int(math.hypot(x2 - x1, y2 - y1) / step))
        for i in range(n):
            t = i / n
            out.append((x1 + (x2 - x1) * t + rnd.uniform(-amount, amount),
                        y1 + (y2 - y1) * t + rnd.uniform(-amount, amount)))
    out.append(points[-1])
    return out


def circle_points(cx, cy, r, n=48):
    return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n + 1)]


def glyphs(x, y, count, size=10, seed=3):
    """Letter-sized scribbles along a line (fake handwriting)."""
    rnd = random.Random(seed)
    out = []
    for i in range(count):
        gx = x + i * size * 0.75
        pts = [(gx + rnd.uniform(0, size * 0.6), y + rnd.uniform(0, size)) for _ in range(6)]
        out.append(stroke(wobbly(pts, 0.2, seed + i, 1.0)))
    return out


def make_xopp(path, pages):
    """Writes a gzipped .xopp. `pages` is a list of pages; a page is a list of strokes (see stroke())."""
    parts = ['<?xml version="1.0" standalone="no"?>', '<xournal creator="xoai-tests" fileversion="4">']
    for strokes in pages:
        parts.append('<page width="595.27" height="841.89"><background type="solid" color="#ffffffff" '
                     'style="plain"/><layer>')
        for s in strokes:
            coords = " ".join(f"{x:.2f} {y:.2f}" for x, y in s["points"])
            parts.append(f'<stroke tool="{s["tool"]}" color="{s["color"]}" width="{s["width"]}">{coords}</stroke>')
        parts.append("</layer></page>")
    parts.append("</xournal>")
    with gzip.open(path, "wt") as f:
        f.write("\n".join(parts))
    return path


def make_png(width, height, rgb=(255, 0, 0)):
    """A solid-color PNG (bytes)."""
    import struct
    import zlib
    raw = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))
