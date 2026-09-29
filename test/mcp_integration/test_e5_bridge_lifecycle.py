"""E5: an agent configured with the stdio bridge works whenever the user runs xournalai: the bridge answers for the
absent app (no tools, no window popping up) and announces the tools when the user starts it, and again after a
restart."""

import json
import os
import queue
import subprocess
import threading
import time

import xoai


class Bridge:
    def __init__(self, app):
        env = dict(app.env)
        env.pop("DISPLAY", None)  # the bridge must not start the app on its own here (and never on the desktop)
        env.pop("WAYLAND_DISPLAY", None)
        self.p = subprocess.Popen([app.binary, "--mcp-stdio", f"--mcp-port={app.port}"], env=env, text=True,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        self.lines = queue.Queue()
        threading.Thread(target=self._read, daemon=True).start()
        self.next_id = 1

    def _read(self):
        for line in self.p.stdout:
            self.lines.put(json.loads(line))

    def send(self, method, params=None, notify=False):
        msg = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        if not notify:
            msg["id"] = self.next_id
            self.next_id += 1
        self.p.stdin.write(json.dumps(msg) + "\n")
        self.p.stdin.flush()
        return msg.get("id")

    def wait(self, pred, timeout=20):
        end = time.time() + timeout
        seen = []
        while time.time() < end:
            try:
                m = self.lines.get(timeout=0.2)
            except queue.Empty:
                continue
            seen.append(m)
            if pred(m):
                return m, seen
        raise AssertionError(f"no matching message; got {seen}")

    def request(self, method, params=None, timeout=20):
        i = self.send(method, params)
        m, _ = self.wait(lambda m: m.get("id") == i, timeout)
        return m

    def close(self):
        self.p.stdin.close()
        self.p.wait(timeout=10)


def test_bridge_waits_for_the_app(app):
    app.stop()  # this scenario manages its own app: first there is none
    later = xoai.App()
    later.home, later.tmp = app.home, app.tmp  # same config (token) as the bridge
    later.port = app.port
    b = Bridge(later)
    try:
        init = b.request("initialize", {"protocolVersion": "2025-06-18", "capabilities": {},
                                        "clientInfo": {"name": "t", "version": "1"}}, timeout=10)
        r = init["result"]
        assert r["serverInfo"]["version"] == "offline" and r["capabilities"]["tools"]["listChanged"] is True
        b.send("notifications/initialized", notify=True)
        assert b.request("tools/list")["result"]["tools"] == []
        launched = subprocess.run(["pgrep", "-f", f"xournalpp --mcp --mcp-port={later.port}"], capture_output=True,
                                  text=True).stdout.strip()
        assert launched == "", "nothing may be started by the handshake"

        later.start()  # the user starts xournalai
        b.wait(lambda m: m.get("method") == "notifications/tools/list_changed", timeout=20)
        tools = b.request("tools/list")["result"]["tools"]
        assert len(tools) > 50
        st = b.request("tools/call", {"name": "app_status", "arguments": {}})
        assert st["result"]["structuredContent"]["app"] == "xournalai"

        later.stop()  # the user closes it: the tools stay listed, calls ask for the app instead of relaunching it
        assert len(b.request("tools/list")["result"]["tools"]) > 50
        r = b.request("tools/call", {"name": "doc_info", "arguments": {}})["result"]
        assert r["isError"] and "not open" in r["content"][0]["text"]
        time.sleep(1)
        assert subprocess.run(["pgrep", "-f", f"xournalpp --mcp --mcp-port={later.port}"], capture_output=True,
                              text=True).stdout.strip() == "", "a closed app must not be reopened by the agent"
        later.start()  # the user opens it again
        b.wait(lambda m: m.get("method") == "notifications/tools/list_changed", timeout=20)
        assert b.request("tools/call", {"name": "doc_info", "arguments": {}})["result"]["structuredContent"][
            "page_count"] >= 1
    finally:
        b.close()
        later.stop()
