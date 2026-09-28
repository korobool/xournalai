# Connecting an AI agent to xournalai

xournalai runs an MCP server while the application is open. Any MCP-capable agent can use it: Claude Code, Gemini CLI,
OpenCode, OpenAI Codex CLI, Cursor, or your own scripts.

- **Endpoint**: `http://127.0.0.1:7474/mcp` (MCP Streamable HTTP; local connections only)
- **Auth**: header `Authorization: Bearer <token>`
- **Token and settings**: `~/.config/xournalpp/mcp.json`. The file is created on first start and is readable only by
  you. Its `_help` section contains ready-made commands with your token filled in.
- **stdio alternative**: `xournalpp --mcp-stdio`. It forwards to the running app and starts it if needed, and it reads
  the token itself.

Replace `<token>` below with the value from `mcp.json`. Client configuration formats change from time to time, so if a
snippet is rejected, check that client's MCP documentation. The endpoint, header and stdio command stay the same.

## Claude Code
```sh
claude mcp add --transport http xournalai http://127.0.0.1:7474/mcp --header "Authorization: Bearer <token>"
# or, without handling the token yourself:
claude mcp add xournalai -- xournalpp --mcp-stdio
```

## Gemini CLI — `~/.gemini/settings.json`
```json
{
  "mcpServers": {
    "xournalai": {
      "httpUrl": "http://127.0.0.1:7474/mcp",
      "headers": { "Authorization": "Bearer <token>" }
    }
  }
}
```
stdio variant: `"xournalai": { "command": "xournalpp", "args": ["--mcp-stdio"] }`

## OpenCode — `opencode.json`
```json
{
  "mcp": {
    "xournalai": {
      "type": "remote",
      "url": "http://127.0.0.1:7474/mcp",
      "headers": { "Authorization": "Bearer <token>" },
      "enabled": true
    }
  }
}
```
stdio variant: `"xournalai": { "type": "local", "command": ["xournalpp", "--mcp-stdio"], "enabled": true }`

## OpenAI Codex CLI — `~/.codex/config.toml`
```toml
[mcp_servers.xournalai]
command = "xournalpp"
args = ["--mcp-stdio"]
```

## Cursor — `~/.cursor/mcp.json`
```json
{
  "mcpServers": {
    "xournalai": {
      "url": "http://127.0.0.1:7474/mcp",
      "headers": { "Authorization": "Bearer <token>" }
    }
  }
}
```

## Anything else
- **HTTP**: POST JSON-RPC to the endpoint with `Content-Type: application/json`, the bearer header, and after
  `initialize` the `Mcp-Session-Id` header from the initialize response. `GET` with `Accept: text/event-stream`
  opens the notification stream.
- **stdio**: launch `xournalpp --mcp-stdio` and exchange newline-delimited JSON-RPC over stdin/stdout.

## Options
| Where | Setting | Meaning |
|---|---|---|
| `mcp.json` | `enabled` | Start the server with the app (default `true`) |
| `mcp.json` | `port` | TCP port (default `7474`) |
| `mcp.json` | `permissions` | `read`, `draw`, `ui`, `files` (default on), `destructive` (default off: discard unsaved changes, overwrite files, close without saving) |
| `mcp.json` | `default_layer` | Where agent drawings go: `"AI"` (default), `"current"` or a layer name |
| `mcp.json` | `animate` | Animate agent drawing by default |
| `mcp.json` | `export_dir` | Folder for renders and exports when no path is given |
| command line | `--mcp` / `--no-mcp` | Force the server on or off for this session |
| command line | `--mcp-port=N` | Port for this session. The stdio bridge uses the same flag to find the app |

Restart xournalai after editing `mcp.json`.

## Testing the connection
```sh
python3 test/mcp_integration/run.py      # end-to-end scenarios against the built app (needs xvfb-run)
```
