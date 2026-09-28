# xournalai changelog

The fork has its own version (`XOURNALAI_VERSION` in `CMakeLists.txt`). The application reports it as
`<upstream version>+ai.<fork version>`, e.g. `1.3.7+ai.0.1.0`. The embedded MCP server reports the fork version.
See [ROADMAP.md](ROADMAP.md) for the versioning rules.

## Unreleased

## 0.1.0 — Foundations (epoch E0)
Any MCP agent can now connect to a running xournalai and call its first tools.
- Tools: `app_status` (version, connection, permissions, document and active tool) and `doc_info` (pages, sizes,
  backgrounds, layers and element counts, paginated).
- Tool runtime: forgiving typed arguments (numbers and booleans are also accepted as strings; typos in argument
  names are reported), color parsing (hex, rgb(), CSS names), PNG and base64 helpers, and an export folder.
- End-to-end test harness `test/mcp_integration` (stdlib Python client, headless app, isolated config) and
  per-client setup guide `docs/mcp/CLIENTS.md` (Claude Code, Gemini CLI, OpenCode, Codex, Cursor, generic).
- Spikes (`docs/mcp/SPIKES.md`): synthetic pen events go through the real stylus pipeline with hardware-identical
  pressure; nanosvg is suitable for SVG → strokes; the highlighter never uses pressure.
  `InputContext::handleSynthetic()` hook added.
- Verified: 136/136 unit and GTK tests, 6/6 integration scenarios; also builds with `-DENABLE_MCP=OFF`.

### 0.0.3 — Protocol & transport
- JSON-RPC 2.0 / MCP protocol core (protocol versions 2025-06-18, 2025-03-26 and 2024-11-05): initialize, ping,
  tools, prompts, resources (with subscriptions) and logging. A tool registry that enforces portable schemas; an
  error barrier around every tool call.
- Streamable HTTP transport on `127.0.0.1:7474/mcp` (libsoup 3): sessions, bearer token, Origin/Host checks,
  paused replies for slow tools, SSE stream for notifications.
- `~/.config/xournalpp/mcp.json`: enabled flag, port, generated token, permission tiers (read, draw, ui, files and
  destructive; destructive is off by default), default layer, animation, export folder. Readable by the owner only.
- Command line: `--mcp`, `--no-mcp`, `--mcp-port=N`, and `--mcp-stdio` (a stdio bridge that starts the app if it
  isn't running).

### 0.0.2 — Build integration
- Recorded the upstream baseline: 119/119 tests passing (`docs/mcp/BASELINE.md`).
- `ENABLE_MCP` CMake option (default ON), with libsoup 3, nlohmann/json and a vendored copy of nanosvg.
- `src/core/mcp` module skeleton; `McpServer` is owned by `Control` and started with the main window.

### 0.0.1 — Planning & tooling
- Implementation plan, roadmap and task registry (`docs/mcp/`).
- Browser-based Kanban progress monitor (`tools/monitor/`).
- Fork versioning.
