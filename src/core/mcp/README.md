# `src/core/mcp` — embedded MCP server

This directory holds the transport (HTTP and stdio bridge), the JSON-RPC/MCP protocol, the tool, prompt and resource
registries, and the tool definitions (`tools/`). Tools translate JSON to and from the typed services in `src/core/api`,
and they contain no document logic themselves.

It is only compiled with `-DENABLE_MCP=ON`. The design is described in `docs/mcp/PLAN.md` and the roadmap in `docs/mcp/ROADMAP.md`.

## Threads: agents never freeze the user's pen

The server runs on the GTK main loop, the same thread that draws the user's strokes. So a tool that takes long must not
run there:

- **Heavy reading tools** (`page_render`, `page_elements`, `layout_analyze`, `blocks_render`) use an `asyncHandler`
  that parses the arguments on the UI thread and hands the work to `runOffUi` (`OffUi.h`). The work runs on the render
  thread, which also renders pages, so PDF backgrounds are never rendered concurrently. There it takes a snapshot
  under a short shared document lock (`model/PageSnapshot.h`), then does the long part without the lock. It also
  serializes its result (`ToolResult::prepare`), so sending a big result costs the UI thread nothing.
- **Background rendering** (page buffers, sidebar previews) likewise holds the lock only to copy what it draws. The UI
  thread needs the exclusive lock to add a stroke, and it must never wait for a whole render.
- **The watchdog** (`util/StallWatch.h`) records every time the UI thread does not respond for more than 50 ms, with
  what it was doing (`stall::Activity` labels: the tool, "waiting for the document lock", "redraw", …) and what ran
  meanwhile on other threads. The counts and the latest stalls are in `app_status.ui`; the detailed log goes to
  `~/.cache/xournalai/stall-trace.log` while `~/.cache/xournalai/trace-stalls` exists.
  `test/mcp_integration/test_e9_responsiveness.py` keeps it that way.
