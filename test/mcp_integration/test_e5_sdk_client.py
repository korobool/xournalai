"""E5: the official MCP Python SDK (an independent client implementation) against both transports.
Skipped when the `mcp` package is not installed."""

import asyncio
import base64

import xoai

try:
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    from mcp.client.streamable_http import streamablehttp_client
except ImportError:  # pragma: no cover
    ClientSession = None


async def exercise(session):
    init = await session.initialize()
    assert init.serverInfo.name == "xournalai"
    tools = (await session.list_tools()).tools
    assert len(tools) >= 50 and all(t.inputSchema.get("type") == "object" for t in tools)
    status = await session.call_tool("app_status", {})
    assert not status.isError and status.structuredContent["app"] == "xournalai"
    drawn = await session.call_tool("create_shapes", {"shapes": [{"type": "circle", "center": [200, 200], "r": 30}],
                                                      "animate": False})
    assert not drawn.isError and drawn.structuredContent["created"]
    render = await session.call_tool("page_render", {"dpi": 30})
    image = [c for c in render.content if c.type == "image"][0]
    assert base64.b64decode(image.data)[:8] == b"\x89PNG\r\n\x1a\n"
    bad = await session.call_tool("page_elements", {"page": 999})
    assert bad.isError
    prompts = (await session.list_prompts()).prompts
    assert {"summarize", "extract_text"} <= {p.name for p in prompts}
    got = await session.get_prompt("summarize", {"page": "1"})
    assert got.messages
    resources = (await session.list_resources()).resources
    doc = await session.read_resource("xournal://document")
    assert resources and doc.contents[0].text.startswith("{")
    await session.subscribe_resource("xournal://changes")
    return len(tools)


def run(coro):
    return asyncio.new_event_loop().run_until_complete(coro)


def test_streamable_http(app):
    if ClientSession is None:
        print("    (skipped: pip install mcp)")
        return

    async def main():
        url = f"http://127.0.0.1:{app.port}/mcp"
        async with streamablehttp_client(url, headers={"Authorization": f"Bearer {app.token}"}) as (r, w, _):
            async with ClientSession(r, w) as session:
                return await exercise(session)

    assert run(main()) >= 50


def test_stdio_bridge(app):
    if ClientSession is None:
        return

    async def main():
        # The bridge must find the test app (its port) and must never start one on the real desktop
        params = StdioServerParameters(command=app.binary, args=["--mcp-stdio", f"--mcp-port={app.port}"],
                                       env=app.app_env())
        async with stdio_client(params) as (r, w):
            async with ClientSession(r, w) as session:
                return await exercise(session)

    assert run(main()) >= 50
