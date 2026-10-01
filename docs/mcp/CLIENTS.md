# Connecting an AI agent to xournalai

xournalai runs an MCP server while the application is open. Any MCP-capable agent can use it: Claude Code, Gemini CLI,
OpenCode, OpenAI Codex CLI, Cursor, or your own scripts.

- **Endpoint**: `http://127.0.0.1:7474/mcp` (MCP Streamable HTTP; local connections only)
- **Auth**: header `Authorization: Bearer <token>`
- **Token and settings**: `~/.config/xournalpp/mcp.json`. The file is created on first start and is readable only by
  you. Its `_help` section contains ready-made commands with your token filled in.
- **stdio alternative**: `xournalpp --mcp-stdio`. It forwards to the running app and starts it if needed, and it reads
  the token itself.

**The easy way:** *AI Agent → Connect an Agent…* in the app shows every option below with your token and the
binary's path filled in, ready to copy (also in `mcp.json` under `_help.connect`). The universal `mcpServers` JSON
block (stdio) works with any MCP client.

Replace `<token>` below with the value from `mcp.json`. Client configuration formats change from time to time, so if a
snippet is rejected, check that client's MCP documentation. The endpoint, header and stdio command stay the same.

## Claude Code
Recommended: the stdio bridge, once, for all projects. It reads the token itself, answers for the app while it isn't
running (no tools; nothing is launched) and announces the tools as soon as you start xournalai:
```sh
claude mcp add -s user xournalai -- /path/to/build/install/bin/xournalpp --mcp-stdio
```
HTTP instead (the agent stays connected across app restarts; if the app isn't running when the agent starts,
reconnect with `/mcp`):
```sh
claude mcp add -s user --transport http xournalai http://127.0.0.1:7474/mcp --header "Authorization: Bearer <token>"
```

## Gemini CLI — `~/.gemini/settings.json`
Once, from a terminal (stdio; note: no `--` before the command):
```sh
gemini mcp add -s user xournalai /path/to/build/install/bin/xournalpp --mcp-stdio
```
Or by hand:
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
Once, from a terminal:
```sh
codex mcp add xournalai -- /path/to/build/install/bin/xournalpp --mcp-stdio
```
Or by hand:
```toml
[mcp_servers.xournalai]
command = "xournalpp"
args = ["--mcp-stdio"]
```
HTTP variant (token from an environment variable):
```toml
[mcp_servers.xournalai]
url = "http://127.0.0.1:7474/mcp"
bearer_token_env_var = "XOURNALAI_TOKEN"
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
| `mcp.json` | `default_layer` | Where agent drawings go: `"current"` (default: your selected layer, so you can erase and edit them right away), `"AI"` (a separate layer you accept, hide or clear from the AI Agent menu) or a layer name |
| `mcp.json` | `animate` | Animate agent drawing by default |
| `mcp.json` | `export_dir` | Folder for renders and exports when no path is given |
| `mcp.json` | `backups` / `backup_dir` | Save a copy of the document before risky agent operations (discarding changes, deleting pages or layers, deleting 20+ elements); the newest 30 are kept |
| command line | `--mcp` / `--no-mcp` | Force the server on or off for this session |
| command line | `--mcp-port=N` | Port for this session. The stdio bridge uses the same flag to find the app |

The **AI Agent → AI Agent Settings…** dialog edits the same file and applies the changes immediately. It is for the
user only: agents cannot open or operate it. After editing `mcp.json` by hand, restart xournalai.

## Client matrix (xournalai 1.0.0, 2026-09-29)
Produced by `python3 tools/mcpdoc/client_matrix.py`. The script starts a headless xournalai and asks each installed
client for its MCP server status, using a temporary config and never the user's own. No model calls are made.

| Client | Version | HTTP | Details |
|---|---|---|---|
| Gemini CLI | 0.52.0 | ✅ | `gemini mcp list`: xournalai (http) - Connected |
| OpenCode | 1.17.8 | ✅ | `opencode mcp list`: xournalai connected |
| Claude Code | 2.1.284 (Claude Code) | ⚠️ | config accepted; project servers need a one-time approval in `claude` (not given by this check) |
| OpenAI Codex CLI | codex-cli 0.116.0 | ⚠️ | config accepted (Codex connects when a session starts; `codex mcp get` shows it as enabled) |
| MCP Python SDK | 1.26.0 | ✅ | full protocol run over HTTP and stdio: initialize, tools, prompts, resources, subscribe (`test_e5_sdk_client.py`) |

✅ connected and listed the tools · ⚠️ configuration accepted, connection not attempted (see details)

## Testing the connection
```sh
python3 test/mcp_integration/run.py      # end-to-end scenarios against the built app (needs xvfb-run)
```
