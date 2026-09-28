# `src/core/api` — typed application services

These are plain C++ services over `Control` and the document model: reading, rendering, drawing, editing, files and
UI automation. They don't know about JSON or MCP. The MCP tools in `src/core/mcp/tools` use them, and the Lua plugin
API can be moved onto them later.

Every function must be called on the GTK main thread.
