"""E4: resources and push notifications for subscribed clients."""

import base64


def test_resources_list_and_read(app):
    c = app.client()
    listed = {r["uri"] for r in c.request("resources/list")["resources"]}
    assert {"xournal://document", "xournal://changes"} <= listed
    templates = {t["uriTemplate"] for t in c.request("resources/templates/list")["resourceTemplates"]}
    assert {"xournal://page/{page}", "xournal://page/{page}/image"} <= templates
    doc = c.request("resources/read", {"uri": "xournal://document"})["contents"][0]
    assert '"pages"' in doc["text"]
    img = c.request("resources/read", {"uri": "xournal://page/1/image"})["contents"][0]
    assert base64.b64decode(img["blob"])[:4] == b"\x89PNG"
    c.call("create_shapes", shapes=[{"type": "circle", "center": [100, 100], "r": 10}], animate=False)
    page = c.request("resources/read", {"uri": "xournal://page/1"})["contents"][0]
    assert '"elements"' in page["text"] and "stroke" in page["text"]


def test_subscription_pushes_updates(app):
    c = app.client()
    c.request("resources/subscribe", {"uri": "xournal://page/1"})
    next_message = c.open_stream(timeout=10)
    c.call("create_shapes", shapes=[{"type": "line", "from": [10, 10], "to": [90, 90]}], animate=False)
    seen = []
    for _ in range(10):
        m = next_message()
        if m is None:
            break
        seen.append(m)
        if m.get("method") == "notifications/resources/updated" and m["params"]["uri"] == "xournal://page/1":
            break
    assert any(m.get("params", {}).get("uri") == "xournal://page/1" for m in seen), seen


def test_stdio_bridge_forwards_notifications(app):
    import json
    import subprocess
    import threading

    p = subprocess.Popen([app.binary, "--mcp-stdio", f"--mcp-port={app.port}"], env=app.env, text=True,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    def send(m):
        p.stdin.write(json.dumps(m) + "\n")
        p.stdin.flush()

    send({"jsonrpc": "2.0", "id": 1, "method": "initialize",
          "params": {"protocolVersion": "2025-06-18", "capabilities": {}, "clientInfo": {"name": "s", "version": "1"}}})
    json.loads(p.stdout.readline())
    send({"jsonrpc": "2.0", "id": 2, "method": "resources/subscribe", "params": {"uri": "xournal://changes"}})
    json.loads(p.stdout.readline())
    import time
    time.sleep(0.5)  # the bridge opens its event stream after the first response
    app.client().call("create_shapes", shapes=[{"type": "line", "from": [5, 5], "to": [50, 50]}], animate=False)
    got = {}

    def reader():
        for _ in range(10):
            line = p.stdout.readline()
            if not line:
                return
            m = json.loads(line)
            if m.get("method") == "notifications/resources/updated":
                got["m"] = m
                return

    th = threading.Thread(target=reader, daemon=True)
    th.start()
    th.join(timeout=10)
    p.stdin.close()
    p.wait(timeout=10)
    assert got.get("m", {}).get("params", {}).get("uri") == "xournal://changes"
