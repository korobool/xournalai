# <img src="ui/pixmaps/io.github.korobool.xournalai.svg" align="left" width="100" height="100">  <br> xournalai

> [!WARNING]
> **This is an experimental fork of [Xournal++](https://github.com/xournalpp/xournalpp) for personal use.**
> It is not affiliated with or supported by the Xournal++ team. Things may be broken, unfinished, or change without notice.
> If you just want a stable note-taking app, use the [original project](https://github.com/xournalpp/xournalpp) and its
> [official releases](https://github.com/xournalpp/xournalpp/releases).

<img src="readme/main.png" width=550px title="xournalai on GNU/Linux"/>

## About

xournalai is a handwriting and PDF annotation app with an **AI agent built in as a tool**. It is based on
Xournal++. The running app hosts an [MCP](https://modelcontextprotocol.io) server. Any MCP-capable agent can read
your notes, draw in them with stylus-like pressure, operate the whole application, and work on the page together
with you. That includes Claude Code, Gemini CLI, OpenCode, Codex, Cursor or your own scripts. Everything runs locally: the server only
listens on `127.0.0.1` and needs a token.

## What an agent can do

| Area | Examples |
|---|---|
| **Understand** | Render pages to look at them, list strokes/text/images with their positions, find layout blocks and crop them for reading handwriting, read the text of an annotated PDF, recognise sketched shapes. Built-in recipes: *summarize*, *extract text*, *explain figure*. |
| **Draw like a pen** | `pen_draw` drives the app's real stylus pipeline with pressure profiles (ink, brush, pencil, calligraphy, marker), at hand speed so you can watch. It can erase and lasso-select too. `user_style` learns the pressure of *your* strokes and `match_user` draws in your hand. |
| **Create directly** | Strokes with per-point pressure, shapes (arrows, polygons, Béziers, arcs, coordinate systems, optionally hand-drawn), text, LaTeX, images, links, whole SVG illustrations. It can also find free space on the page. Drafts let it iterate privately before showing the result. |
| **Edit** | Move, scale, rotate, restyle, delete or select elements by id. Undo/redo and history. |
| **Control the app** | Run any of the ~130 menu/toolbar actions, choose menu entries visibly, operate dialogs and file choosers, press shortcuts. Manage pages, layers, tools, zoom and view; use the clipboard. |
| **Files** | Open, save, save as, new, close; export to PDF/PNG/SVG; import images, PDFs and SVGs. |
| **Work together** | `wait_for_user` returns when you've drawn something and paused, with a picture of it. A change log tells your strokes from the agent's. The agent sees your page, pointer, selection and tool, and can leave short callouts on the canvas. |
| **Remember** | `notes` keeps transcripts, summaries and meanings of drawings next to the file (`<file>.ai-notes.json`). |

The full reference is generated from the running server: [docs/mcp/TOOLS.md](docs/mcp/TOOLS.md) (56 tools) and
[docs/mcp/COVERAGE.md](docs/mcp/COVERAGE.md) (every action, menu, dialog and Lua API function).

## You stay in charge

- **AI Agent menu**, and a **status strip** at the bottom of the window showing whether the server is listening,
  the agents connected and what they are doing.
- **Pause AI agent** (button in the strip, menu, or **Ctrl+Alt+Esc**): every agent call is refused until you resume.
  A stroke in progress lifts the pen and your tool is restored.
- **Permissions** by tier: *read*, *draw*, *ui*, *files*, and *destructive* (discard unsaved work, overwrite, quit),
  which is off by default. Safety backups are made before risky operations.
- **AI Agent Settings…** sets the server on/off, port, token, permissions, where drawings go and animation. Agents
  cannot open these settings, and they cannot pause or resume themselves.
- **Where drawings go**: by default into your **current layer**, so you can erase and edit them right away. Set the
  layer to `AI` to keep them on a separate layer that you accept (merge), hide or clear from the AI Agent menu.
- Everything the agent does is an ordinary undo step.

## Getting started

Build and install (see [LinuxBuild.md](readme/LinuxBuild.md) for dependencies; CMake, Ninja and a C++20 compiler):

```sh
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=$PWD/build/install -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target install
tools/register-desktop.sh        # optional: a "xournalai" launcher with its own icon
build/install/bin/xournalpp
```

Connect an agent **once**. The stdio bridge reads the token itself, so the agent works whenever xournalai is running:

```sh
claude mcp add -s user xournalai -- $PWD/build/install/bin/xournalpp --mcp-stdio
```

- The agent always has xournalai's tools. While the app is closed, a call answers "xournalai is not open, ask the
  user to open it"; as soon as you open it, the same session just works, with no reconnect. The agent never
  reopens a window you closed. Only its very first call, if you haven't started xournalai at all yet, opens it.
- Over HTTP instead: `http://127.0.0.1:7474/mcp` with `Authorization: Bearer <token>`. **AI Agent → Copy agent
  connect command** copies a ready-made command, and the token is in `~/.config/xournalpp/mcp.json`.
- Setups for Gemini CLI, OpenCode, Codex and Cursor are in [docs/mcp/CLIENTS.md](docs/mcp/CLIENTS.md).

Then just ask, e.g. *"summarize page 2"*, *"draw a labelled diagram of a heat engine next to my notes"*, or
*"wait until I finish this sketch and then clean it up"*.

## Development

- Plan, roadmap and changes: [docs/mcp/PLAN.md](docs/mcp/PLAN.md), [ROADMAP.md](docs/mcp/ROADMAP.md),
  [CHANGELOG.md](docs/mcp/CHANGELOG.md).
- The MCP code is in `src/core/mcp` (protocol, transport, tools) and `src/core/api` (typed services). Build with
  `-DENABLE_MCP=OFF` to build without the agent server.
- Tests: `ctest` (unit tests, configure with `-DENABLE_GTEST=on`) and
  `python3 test/mcp_integration/run.py` (end-to-end scenarios against the installed build under Xvfb).
- `tools/mcpdoc/gen_docs.py` regenerates the reference docs; `tools/mcpdoc/client_matrix.py` checks installed agents.

## Original project

All credit for Xournal++ goes to its authors and contributors (see [AUTHORS](AUTHORS)).

- Repository: https://github.com/xournalpp/xournalpp
- Website & user guide: https://xournalpp.github.io
- Report upstream bugs and contribute there, not here.

## License

GNU GPL v2 or later, same as upstream. See [LICENSE](LICENSE).
