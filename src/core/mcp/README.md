# `src/core/mcp` — embedded MCP server

This directory holds the transport (HTTP and stdio bridge), the JSON-RPC/MCP protocol, the tool, prompt and resource
registries, and the tool definitions (`tools/`). Tools translate JSON to and from the typed services in `src/core/api`,
and they contain no document logic themselves.

It is only compiled with `-DENABLE_MCP=ON`. The design is described in `docs/mcp/PLAN.md` and the roadmap in `docs/mcp/ROADMAP.md`.
