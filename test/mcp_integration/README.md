# MCP integration scenarios

These are end-to-end tests of the embedded MCP server. Each `test_*.py` file gets a fresh, headless xournalpp
(`xvfb-run`) with isolated XDG directories and a free port. Only the Python standard library is needed.

```sh
cmake --build build                      # the app must be built with -DENABLE_MCP=ON
python3 test/mcp_integration/run.py       # all scenarios
python3 test/mcp_integration/run.py e1 -k render
```

- `xoai.py` contains `App` (launcher) and `Mcp` (a minimal Streamable HTTP client).
- A module can set `APP_ARGS = ["file.xopp"]` to start the app with arguments.
- Results are written to `build/integration.log`.
