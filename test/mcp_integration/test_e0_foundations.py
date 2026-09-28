"""E0: connection, protocol basics, status tools, security."""

import json
import subprocess
import urllib.error
import urllib.request

import xoai


def test_initialize_and_server_info(app):
    c = app.client()
    info = c.init["serverInfo"]
    assert info["name"] == "xournalai"
    assert c.init["protocolVersion"] == "2025-06-18"
    assert "instructions" in c.init
    assert c.session


def test_tools_have_portable_schemas(app):
    tools = app.client().tools()
    assert {"app_status", "doc_info"} <= set(tools)
    for name, t in tools.items():
        assert t["inputSchema"]["type"] == "object", name
        text = json.dumps(t["inputSchema"])
        for bad in ("$ref", "oneOf", "anyOf", "additionalProperties"):
            assert bad not in text, (name, bad)


def test_app_status(app):
    s = app.client().call("app_status")
    assert s["app"] == "xournalai"
    assert s["mcp"]["permissions"]["read"] is True
    assert s["mcp"]["permissions"]["destructive"] is False
    assert s["document"]["page_count"] >= 1
    assert s["user_tool"]["tool"]


def test_doc_info(app):
    c = app.client()
    d = c.call("doc_info")
    assert d["page_count"] == len(d["pages"]) >= 1
    p = d["pages"][0]
    assert p["page"] == 1 and p["width"] > 0 and p["height"] > 0
    assert p["layers"][0]["index"] == 1
    err = c.call_error("doc_info", colour="red")
    assert "Unknown argument" in err


def test_requires_token(app):
    req = urllib.request.Request(f"http://127.0.0.1:{app.port}/mcp", method="POST",
                                 data=b'{"jsonrpc":"2.0","id":1,"method":"ping"}')
    try:
        urllib.request.urlopen(req, timeout=5)
        raise AssertionError("request without token was accepted")
    except urllib.error.HTTPError as e:
        assert e.code == 401


def test_stdio_bridge(app):
    p = subprocess.Popen([app.binary, "--mcp-stdio", f"--mcp-port={app.port}"], env=app.env, text=True,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    p.stdin.write(json.dumps({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                              "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                                         "clientInfo": {"name": "stdio", "version": "1"}}}) + "\n")
    p.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
    p.stdin.write(json.dumps({"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                              "params": {"name": "doc_info", "arguments": {}}}) + "\n")
    p.stdin.flush()
    first = json.loads(p.stdout.readline())
    second = json.loads(p.stdout.readline())
    p.stdin.close()
    p.wait(timeout=10)
    assert first["result"]["serverInfo"]["name"] == "xournalai"
    assert second["result"]["structuredContent"]["page_count"] >= 1


def test_guide_and_prompts(app):
    c = app.client()
    assert "page_render" in c.call("guide")
    assert "page points" in c.call("guide", topic="coordinates")
    assert "Unknown" in c.call_error("guide", topic="nope") or "must be" in c.call_error("guide", topic="nope")
    prompts = {p["name"] for p in c.request("prompts/list")["prompts"]}
    assert {"summarize", "extract_text", "explain_figure"} <= prompts
    msg = c.request("prompts/get", {"name": "summarize", "arguments": {"page": "2"}})["messages"][0]
    assert "page 2" in msg["content"]["text"]
