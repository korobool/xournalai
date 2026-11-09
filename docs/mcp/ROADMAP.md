# xournalai MCP — implementation roadmap

> Generated from `docs/mcp/tasks.json` by `tools/monitor/track.py roadmap`. Do not edit by hand.
> Design: [PLAN.md](PLAN.md). Live board: `tools/monitor/run.sh` → http://127.0.0.1:8765

## Versioning & commits

- The fork has its own semantic version, XOURNALAI_VERSION, kept in CMakeLists.txt. The application version string becomes <upstream>+ai.<XOURNALAI_VERSION>, e.g. 1.3.7+ai.0.2.0.
- Each task is exactly one commit. The commit message is '<planned message> (<task id>)'.
- The last task of a stage bumps the PATCH version (0.E.s), in the same commit.
- The last task of an epoch is a release task: MINOR bump (0.E+1.0, or 1.0.0 for the final epoch), CHANGELOG entry and a local git tag ai-v<version>.

Current version: **1.17.2**

## E0 — Foundations (release 0.1.0)

Any MCP agent can connect to a running xournalai (over HTTP or the stdio bridge) and call its first tools.

### S0.1 — Planning & tooling → v0.0.1

**US-0.1** — As the owner, I want a task roadmap and a live Kanban board in my browser, so that I can follow the implementation as it happens.

- [ ] tasks.json lists epochs, stages, stories and tasks, each task with its planned commit message
- [ ] the board at http://127.0.0.1:8765 shows task status, commits, build/test status and activity, and refreshes automatically
- [ ] the application reports a fork version

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T0.1.1 | US-0.1 | **Roadmap & task registry** — docs/mcp/tasks.json is the source of truth; docs/mcp/ROADMAP.md is generated from it. | `mcp: add implementation roadmap and task registry (T0.1.1)` | ✅ done `954f62d94` |
| T0.1.2 | US-0.1 | **Kanban monitor** — tools/monitor: stdlib Python server + standalone HTML board; track.py CLI for status, activity, build and test updates. | `mcp: add browser-based Kanban progress monitor (T0.1.2)` | ✅ done `63f9fdd21` |
| T0.1.3 | US-0.1 | **Fork versioning** — XOURNALAI_VERSION in CMake, '+ai.<v>' version suffix, docs/mcp/CHANGELOG.md; bump to 0.0.1. | `mcp: introduce xournalai fork versioning (T0.1.3)` | ✅ done `6aa166534` |

### S0.2 — Build integration → v0.0.2

**US-0.2** — As the owner, I want the MCP server to be an optional part of the normal build, so that upstream features keep working and the feature can be turned off.

- [ ] upstream builds and its unit tests pass before any change (baseline recorded)
- [ ] -DENABLE_MCP=ON/OFF both build
- [ ] libsoup-3, nlohmann_json and nanosvg are resolved, with fallbacks where sensible

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T0.2.1 | US-0.2 | **Baseline build & tests** — Build the unmodified fork and run test-units; record the results in docs/mcp/BASELINE.md. | `mcp: record upstream baseline build and test results (T0.2.1)` | ✅ done `49a6458f6` |
| T0.2.2 | US-0.2 | **ENABLE_MCP & dependencies** — CMake option, pkg-config libsoup-3.0, nlohmann_json (system or FetchContent), nanosvg via FetchContent, ENABLE_MCP in config-features.h. | `mcp: add ENABLE_MCP build option and dependencies (T0.2.2)` | ✅ done `6d70348ee` |
| T0.2.3 | US-0.2 | **Module skeleton & lifecycle** — src/core/mcp and src/core/api directories; McpServer is owned by Control and started and stopped with the application. Bump 0.0.2. | `mcp: add MCP module skeleton wired into Control lifecycle (T0.2.3)` | ✅ done `a30cdcb80` |

### S0.3 — Protocol & transport → v0.0.3

**US-0.3** — As an agent (any MCP client), I want to connect to the running app, initialize a session and list and call tools, so that I can use xournalai as a tool.

- [ ] initialize, ping, tools/list and tools/call follow the MCP spec (protocol 2025-06-18, with 2025-03-26 accepted)
- [ ] Streamable HTTP on 127.0.0.1 with a bearer token and an Origin check
- [ ] the stdio bridge works for clients that only launch stdio servers
- [ ] protocol unit tests pass

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T0.3.1 | US-0.3 | **JSON-RPC/MCP protocol core** — McpProtocol (initialize, ping, tools/list, tools/call, prompts, resources, error codes), ToolRegistry with flat schemas and a schema-portability lint; unit tests. | `mcp: implement JSON-RPC 2.0 / MCP protocol core and tool registry (T0.3.1)` | ✅ done `f0ae458d5` |
| T0.3.2 | US-0.3 | **HTTP transport** — SoupServer on the GLib main loop: POST /mcp, GET /mcp (SSE), DELETE session, Mcp-Session-Id, Origin check, bearer auth, paused responses. | `mcp: add Streamable HTTP transport on libsoup (T0.3.2)` | ✅ done `e6808d60c` |
| T0.3.3 | US-0.3 | **Config, token, CLI flags** — ~/.config/xournalpp/mcp.json (enabled, port, token, permission tiers), token generation, --mcp, --mcp-port, --no-mcp flags. | `mcp: add configuration file, token and command line flags (T0.3.3)` | ✅ done `e8b688ce4` |
| T0.3.4 | US-0.3 | **stdio bridge** — xournalpp --mcp-stdio proxies stdio JSON-RPC to the running instance (and starts the app if it isn't running). Bump 0.0.3. | `mcp: add stdio bridge mode for stdio-only clients (T0.3.4)` | ✅ done `87f76ba23` |

### S0.4 — First tools, spikes & release → v0.1.0

**US-0.4** — As an agent, I want to query application status and document info; as the owner, I want the risky technical questions answered early.

- [ ] app_status and doc_info work from a real MCP client
- [ ] an integration test drives the app under xvfb
- [ ] a spike report covers synthetic pen input, nanosvg flattening and highlighter pressure

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T0.4.1 | US-0.4 | **Tool runtime helpers** — Exception barrier, argument validation helpers, result builders (text, json, image + file path), an export folder. | `mcp: add tool runtime helpers and error barrier (T0.4.1)` | ✅ done `50a098dd5` |
| T0.4.2 | US-0.4 | **app_status & doc_info** — Version, capabilities, permissions, document summary, current page, layer and tool. | `mcp: add app_status and doc_info tools (T0.4.2)` | ✅ done `7b9db5065` |
| T0.4.3 | US-0.4 | **Integration harness & client docs** — test/mcp_integration (Python, stdlib HTTP client, xvfb-run) plus docs/mcp/CLIENTS.md (Claude Code, Gemini CLI, OpenCode, Codex, Cursor, generic). | `mcp: add integration test harness and client setup docs (T0.4.3)` | ✅ done `6f956dfdb` |
| T0.4.4 | US-0.4 | **Technical spikes** — Synthetic pen events through InputContext, nanosvg to strokes, highlighter and pressure behaviour; findings in docs/mcp/SPIKES.md. | `mcp: document technical spikes (pen input, SVG, pressure) (T0.4.4)` | ✅ done `e7696c890` |
| T0.4.5 | US-0.4 | **Release 0.1.0** — CHANGELOG, version 0.1.0, tag ai-v0.1.0. | `mcp: release 0.1.0 (T0.4.5)` | ✅ done `053117d13` |

## E1 — Files & Understanding (release 0.2.0)

The agent can open files, read and see content in detail, and export it.

### S1.1 — Model access → v0.1.1

**US-1.1** — As an agent, I want stable ids and structured access to pages, layers and elements, so that I can reason about and refer to content precisely.

- [ ] element ids stay stable across calls and across undo/redo
- [ ] page_elements supports bbox, simplified and full detail levels, with pagination
- [ ] pdf_text returns the text of a background PDF page, optionally with positions

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T1.1.1 | US-1.1 | **ElementIdRegistry** — Stable session ids for elements and pages, invalidated on deletion; unit tests. | `mcp: add stable element and page id registry (T1.1.1)` | ✅ done `808dfc70b` |
| T1.1.2 | US-1.1 | **DocumentApi snapshots** — api/DocumentApi: document, page, layer and element snapshots; RDP simplification; JSON mapping. | `mcp: add DocumentApi with element snapshots and simplification (T1.1.2)` | ✅ done `f23417b1e` |
| T1.1.3 | US-1.1 | **page_elements & pdf_text** — Tools with filters (page, layer, region, detail, tolerance) and pagination. Bump 0.1.1. | `mcp: add page_elements and pdf_text tools (T1.1.3)` | ✅ done `a17e1597c` |

### S1.2 — Rendering & layout understanding → v0.1.2

**US-1.2** — As an agent, I want renders of pages, regions and content blocks plus a layout analysis, so that I can read handwriting and interpret diagrams.

- [ ] page_render returns PNG image content and a file path, with dpi or max size, grid, highlight and layer options
- [ ] layout_analyze returns blocks with kinds, reading order and connectors
- [ ] blocks_render returns tight high-dpi crops
- [ ] shapes_recognize uses the built-in ShapeRecognizer
- [ ] guide and the prompts give summarize, extract_text and explain_figure recipes

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T1.2.1 | US-1.2 | **RenderApi & page_render** — Cairo image surface via DocumentView, PNG encoding, grid overlay, element highlighting, pixel/point scale metadata. | `mcp: add RenderApi and page_render tool (T1.2.1)` | ✅ done `90e588c23` |
| T1.2.2 | US-1.2 | **layout_analyze** — Stroke clustering, block classification, reading order and connector detection; unit tests. | `mcp: add layout analysis of pages into content blocks (T1.2.2)` | ✅ done `822238dfe` |
| T1.2.3 | US-1.2 | **blocks_render & shapes_recognize** — Crops per block; ShapeRecognizer applied to given strokes. | `mcp: add blocks_render and shapes_recognize tools (T1.2.3)` | ✅ done `0db818c6d` |
| T1.2.4 | US-1.2 | **guide tool & prompts** — Recipes and conventions (coordinates, pressure, anchors) as the guide tool and as MCP prompts. Bump 0.1.2. | `mcp: add guide tool and understanding prompts (T1.2.4)` | ✅ done `c259389d8` |

### S1.3 — Files & export → v0.2.0

**US-1.3** — As an agent, I want to open, create, save and close documents and export content in standard and lossless formats, so that I can work with my notes end to end.

- [ ] file_open supports open, annotate_pdf, image_as_page and template modes, with an on_unsaved policy
- [ ] export supports pdf, png, svg, xopp and xjson, for a document, page range, region, layers or selection, delivered as a path or inline
- [ ] xjson is lossless (widths, styles, text, images, LaTeX source)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T1.3.1 | US-1.3 | **file_* tools** — file_open, file_new, file_save, file_save_as, file_close, file_recent, file_info. | `mcp: add file management tools (T1.3.1)` | ✅ done `bf976aa1b` |
| T1.3.2 | US-1.3 | **xjson format** — Serializer and parser for the lossless element interchange format; unit tests. | `mcp: add xjson lossless element interchange format (T1.3.2)` | ✅ done `ee49fc428` |
| T1.3.3 | US-1.3 | **export tool** — pdf, png, svg, xopp and xjson; scopes; path or inline delivery. | `mcp: add export tool (T1.3.3)` | ✅ done `ed8c0466a` |
| T1.3.4 | US-1.3 | **E1 integration & release 0.2.0** — Integration scenario for epoch 1, CHANGELOG, tag. | `mcp: release 0.2.0 (T1.3.4)` | ✅ done `c7e0161e9` |

## E2 — Drawing (release 0.3.0)

The agent draws with stylus-like pressure, either by simulating a pen or by creating objects directly, and can import content.

### S2.1 — Pressure model & direct engine → v0.2.1

**US-2.1** — As the owner, I want agent strokes to vary in width like real stylus strokes, so that AI drawing looks hand-made and expressive.

- [ ] normalised pressure maps exactly like hardware input (minimum pressure, multiplier, tool width)
- [ ] profiles ink, brush, pencil, calligraphy, marker and constant exist, with speed-awareness and resampling
- [ ] unit tests pass

**US-2.2** — As an agent, I want to create strokes, shapes, SVG drawings, text, LaTeX, images and links directly, so that I can build precise content fast.

- [ ] each call is one undo step
- [ ] content goes to the AI layer by default
- [ ] elements are attributed to the agent
- [ ] placement anchors and find_free_space work

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T2.1.1 | US-2.1 | **PressureModel** — Hardware-equivalent mapping, profiles, speed-awareness, resampling, jitter; unit tests. | `mcp: add stylus-like pressure model (T2.1.1)` | ✅ done `197c98bbf` |
| T2.1.2 | US-2.2 | **DrawApi core** — Target layer resolution (AI layer), attribution, grouped undo, animated insertion. | `mcp: add DrawApi core with AI layer, attribution and undo (T2.1.2)` | ✅ done `72be4d62f` |
| T2.1.3 | US-2.2 | **create_strokes & create_shapes** — Geometry generators for line, arrow, rectangle, ellipse, polygon, bezier, arc and coordinate system. | `mcp: add create_strokes and create_shapes tools (T2.1.3)` | ✅ done `c38b7738d` |
| T2.1.4 | US-2.2 | **create_text/latex/image/link** — Text elements, LaTeX via the existing pipeline, image elements, links. | `mcp: add text, LaTeX, image and link creation tools (T2.1.4)` | ✅ done `f92eda7e7` |
| T2.1.5 | US-2.2 | **create_from_svg** — nanosvg flattening into strokes, <text> pre-pass, fit modes, warnings. | `mcp: add SVG to editable strokes conversion (T2.1.5)` | ✅ done `a51ac3265` |
| T2.1.6 | US-2.2 | **Placement helpers** — find_free_space, relative anchors, page new or current. Bump 0.2.1. | `mcp: add placement helpers and anchors (T2.1.6)` | ✅ done `2cedbe78e` |

### S2.2 — Pen engine → v0.2.2

**US-2.3** — As the owner, I want the agent to draw through the same pen pipeline as my stylus, so that its strokes, erasing and selecting behave exactly like mine and I can watch it draw.

- [ ] pen_draw replays trajectories with pressure and timing through the input handlers
- [ ] pen_draw works with pen, highlighter, eraser, lasso/rect selection and the shape tools
- [ ] my tool state is restored afterwards
- [ ] replay waits while I'm mid-stroke

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T2.2.1 | US-2.3 | **SyntheticPen** — Synthetic pen-class InputEvents fed to the input handlers; page/view coordinate mapping; tool save and restore; collision queue. | `mcp: add synthetic pen input engine (T2.2.1)` | ✅ done `8ed8cd90a` |
| T2.2.2 | US-2.3 | **pen_draw tool** — Tool selection, speed modes, results with created, erased and selected ids. Bump 0.2.2. | `mcp: add pen_draw tool (T2.2.2)` | ✅ done `e6317c4e0` |

### S2.3 — Drafts, editing & import → v0.3.0

**US-2.4** — As an agent, I want drafts, editing tools and import, so that I can iterate on a drawing and bring external content in.

- [ ] draft begin, render, commit and discard work
- [ ] elements can be selected, moved, scaled, rotated, restyled, reordered and deleted, with undo, redo and history
- [ ] import supports svg, image, xjson, xopp pages and pdf pages
- [ ] the xjson round-trip is lossless

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T2.3.1 | US-2.4 | **draft tool** — Draft layer lifecycle, render for self-critique, commit as one undo step. | `mcp: add draft workflow tool (T2.3.1)` | ✅ done `dff7574eb` |
| T2.3.2 | US-2.4 | **Editing & history tools** — elements_select, elements_edit, elements_delete, undo, redo, history. | `mcp: add element editing and history tools (T2.3.2)` | ✅ done `b2d1f6e77` |
| T2.3.3 | US-2.4 | **import tool** — svg, image, xjson, xopp pages, pdf pages; path or inline sources; placement. | `mcp: add import tool (T2.3.3)` | ✅ done `24b80b2d0` |
| T2.3.4 | US-2.4 | **E2 integration & release 0.3.0** — Text-to-drawing scenario, xjson round-trip check, CHANGELOG, tag. | `mcp: release 0.3.0 (T2.3.4)` | ✅ done `f9209dd2d` |

## E3 — Application control (release 0.4.0)

The agent can control everything in the app, semantically and through the real UI.

### S3.1 — Semantic control → v0.3.1

**US-3.1** — As an agent, I want to run any app action and manage pages, layers, tools, the view and the clipboard, so that I can operate the app reliably.

- [ ] actions_list and action_run cover all actions
- [ ] page_manage, layer_manage, tool_get, tool_set, view and clipboard work

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.1.1 | US-3.1 | **actions_list & action_run** — Via ActionDatabase, with enabled flag, state and parameters. | `mcp: add actions_list and action_run tools (T3.1.1)` | ✅ done `2680f8801` |
| T3.1.2 | US-3.1 | **page_manage & layer_manage** — Insert, delete, move, background, size and goto; layer add, rename, visibility, select, merge, delete and copy. | `mcp: add page and layer management tools (T3.1.2)` | ✅ done `e6c242f3e` |
| T3.1.3 | US-3.1 | **tool_*, view, clipboard** — Tool state get and set, zoom, scroll, layout, fullscreen, presentation, clipboard. Bump 0.3.1. | `mcp: add tool, view and clipboard control tools (T3.1.3)` | ✅ done `dc3e14c67` |

### S3.2 — UI automation → v0.3.2

**US-3.2** — As the owner, I want the agent to operate the real interface — open menus, navigate them, operate dialogs — so that everything I can do, the agent can do.

- [ ] ui_menu_tree and a visible ui_menu_select work
- [ ] ui_windows, ui_inspect and ui_screenshot work
- [ ] ui_interact, ui_keys, ui_file_chooser and ui_wait_for_window work

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.2.1 | US-3.2 | **Widget registry & inspection** — Stable widget ids, ui_windows, ui_inspect, ui_screenshot. | `mcp: add UI inspection tools (T3.2.1)` | ✅ done `6c1d781b8` |
| T3.2.2 | US-3.2 | **Menu automation** — ui_menu_tree from GMenuModel, ui_menu_select with visible navigation. | `mcp: add menu tree and visible menu navigation (T3.2.2)` | ✅ done `8063ea91f` |
| T3.2.3 | US-3.2 | **Widget interaction** — ui_interact, ui_keys, ui_file_chooser, ui_wait_for_window. Bump 0.3.2. | `mcp: add widget interaction, keys and file chooser automation (T3.2.3)` | ✅ done `1b055a989` |

### S3.3 — Safety & release → v0.4.0

**US-3.3** — As the owner, I want permission tiers and automatic backups, so that an agent can't destroy my work.

- [ ] calls outside the granted tiers return an explanatory error
- [ ] a backup is taken before destructive or bulk operations

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.3.1 | US-3.3 | **Permission tiers & backups** — read, draw, ui and files tiers; backup snapshots before risky operations. | `mcp: add permission tiers and safety backups (T3.3.1)` | ✅ done `eb0db8608` |
| T3.3.2 | US-3.3 | **E3 integration & release 0.4.0** — Menu and dialog scenario, CHANGELOG, tag. | `mcp: release 0.4.0 (T3.3.2)` | ✅ done `da97aaa17` |

## E4 — Co-creation (release 0.5.0)

I draw, the agent watches, reacts and adds to my work in real time.

### S4.1 — Events → v0.4.1

**US-4.1** — As an agent, I want to know what the user changes and when they pause, so that I can react at the right moment.

- [ ] changes_get returns an ordered event log
- [ ] wait_for_user returns after the user draws and pauses, with a render of the region
- [ ] resource subscriptions push updates to clients that support them

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T4.1.1 | US-4.1 | **EventHub & changes_get** — Hooks in undo/redo and the document listener, in-progress stroke state, cursor-based log. | `mcp: add EventHub and changes_get tool (T4.1.1)` | ✅ done `0778f90a9` |
| T4.1.2 | US-4.1 | **wait_for_user** — Paused HTTP responses resumed on idle or timeout; region filter. | `mcp: add wait_for_user tool (T4.1.2)` | ✅ done `4d0569c56` |
| T4.1.3 | US-4.1 | **Resources & push** — xournal:// resources, subscribe, SSE notifications. Bump 0.4.1. | `mcp: add resources with subscriptions and SSE notifications (T4.1.3)` | ✅ done `64a5269e9` |

### S4.2 — Presence & in-app UI → v0.5.0

**US-4.2** — As the owner, I want to see when and where the agent acts, stop it at once and configure it, so that co-creation feels safe and natural.

- [ ] view get returns the visible area, pen position and selection
- [ ] show_message shows a callout on the canvas
- [ ] the match_user profile works
- [ ] a status indicator, a Stop agent action and highlights exist
- [ ] MCP settings are editable in the app

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T4.2.1 | US-4.2 | **Presence tools** — view get (visible area, pen position, selection), show_message callouts. | `mcp: add presence tools (T4.2.1)` | ✅ done `3141e780b` |
| T4.2.2 | US-4.2 | **match_user profile** — Derive the pressure and width profile from the user's recent strokes. | `mcp: add match_user pressure profile (T4.2.2)` | ✅ done `c5ca883cb` |
| T4.2.3 | US-4.2 | **Status, Stop & highlight** — Status indicator, Stop agent action and shortcut, highlight of agent additions, AI layer accept and clear. | `mcp: add agent status indicator, stop action and highlights (T4.2.3)` | ✅ done `3e6efcd37` |
| T4.2.4 | US-4.2 | **Settings page** — MCP page in Settings: enable, port, token copy, tiers, defaults. | `mcp: add MCP settings page (T4.2.4)` | ✅ done `e2e946212` |
| T4.2.5 | US-4.2 | **E4 integration & release 0.5.0** — Co-creation scenario, CHANGELOG, tag. | `mcp: release 0.5.0 (T4.2.5)` | ✅ done `6ae2063b3` |

## E5 — Polish (release 1.0.0)

Complete, documented, verified coverage across several agents.

### S5.1 — Memory, performance, docs & 1.0 → v1.0.0

**US-5.1** — As the owner, I want persistent AI notes, good performance and full documentation, so that the connector is dependable day to day.

- [ ] notes are stored in .xopp
- [ ] renders are cached
- [ ] TOOLS.md, CLIENTS.md and COVERAGE.md are complete
- [ ] a client matrix has been run

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T5.1.1 | US-5.1 | **Notes memory** — notes tool storing transcripts, summaries and meanings in the document. | `mcp: add persistent notes memory (T5.1.1)` | ✅ done `9e272225b` |
| T5.1.2 | US-5.1 | **Performance** — Render cache, payload limits, pagination review. | `mcp: improve performance of rendering and large documents (T5.1.2)` | ✅ done `12ac74607` |
| T5.1.3 | US-5.1 | **Reference docs & coverage** — TOOLS.md generated from the registry; COVERAGE.md mapping actions, menus and dialogs. | `mcp: add tool reference and coverage documentation (T5.1.3)` | ✅ done `eb903bcd9` |
| T5.1.4 | US-5.1 | **Release 1.0.0** — Client matrix results, CHANGELOG, tag. | `mcp: release 1.0.0 (T5.1.4)` | ✅ done `4b15053c9` |

## E6 — Serving session in the app (Assistant Milestone 1) (release 1.2.0)

Your canvas workflow inside xournalai: an embedded terminal running the serving Claude Code (bypass permissions by default), an AI toolbar built on your markers, an Auto-improve toggle, and visible thinking, with no extra windows.

### S6.1 — Embedded AI terminal → v1.1.1

**US-6.1** — As the owner, I want a collapsible terminal inside xournalai that runs Claude Code or Codex, so that the serving session lives in the app, not in a separate window.

- [ ] a dock with tabs toggles with a key and keeps its processes while hidden
- [ ] tab 1 starts Claude Code in the companion folder with --dangerously-skip-permissions (setting: bypass | normal)
- [ ] + opens Codex (--dangerously-bypass-approvals-and-sandbox), OpenCode or a shell
- [ ] builds without VTE (ENABLE_AI_TERMINAL off)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T6.1.1 | US-6.1 | **Terminal dock** — CMake option ENABLE_AI_TERMINAL (vte-2.91); a collapsible dock (bottom, resizable) with a notebook of VTE tabs; View/AI Agent menu toggle and shortcut; processes keep running while hidden. | `ai: add embedded terminal dock (T6.1.1)` | ✅ done `8377db56a` |
| T6.1.2 | US-6.1 | **Companion folder** — ~/.local/share/xournalai/companion: CLAUDE.md (role, markers *! **! *w! *c! *r!, assist-don't-redo, delegation, wake-up protocol), .mcp.json (stdio bridge to this app), settings (hooks); created and updated by the app. | `ai: provision the companion folder (T6.1.2)` | ✅ done `cd2bf6bc9` |
| T6.1.3 | US-6.1 | **Serving session autostart** — Tab 1 auto-starts `claude --dangerously-skip-permissions --continue` in the companion folder when the app starts (settings: autostart, permission mode bypass|normal, agent claude|codex); '+' menu for Codex / OpenCode / shell; restart action. | `ai: start the serving session in the dock (T6.1.3)` | ✅ done `1c7f934aa` |

### S6.2 — The app watches, the session works → v1.1.2

**US-6.2** — As the owner, I want the serving session to react whenever something relevant happens on the canvas, without it falling asleep, so that watching works all day.

- [ ] the app knows whether the session is busy or idle (hooks)
- [ ] relevant events wake an idle session by typing a short line into its terminal
- [ ] events are coalesced; a watchdog resends once and offers Restart
- [ ] nothing is sent while AI is paused or the session is busy with your typing

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T6.2.1 | US-6.2 | **Session state via hooks** — `xournalpp --ai-hook <event>` (SessionStart, UserPromptSubmit, Stop, Notification) reports to the running app; state busy/idle/waiting shown in the status strip. | `ai: track the serving session state with hooks (T6.2.1)` | ✅ done `feef02967` |
| T6.2.2 | US-6.2 | **Event pump and wake-ups** — Coalesced canvas events (user strokes after idle, markers, toolbar actions) become one wake-up line typed into the idle session's terminal (only when its prompt is empty); intents_next / changes_get for details; watchdog and Restart. | `ai: wake the serving session on canvas events (T6.2.2)` | ✅ done `49854ef3e` |

### S6.3 — AI toolbar and Auto-improve → v1.1.3

**US-6.3** — As the owner, I want AI buttons for my marker commands and a toggle for assistant mode, so that I can trigger AI help without writing markers every time.

- [ ] an AI toolbar row with: Improve strokes (*!), Illustrate (**!), Web summary (*w!), Real image (*r!), Command (*c!), Revise page, Pause, Terminal
- [ ] buttons act on the selection (or the last drawn piece)
- [ ] an Auto-improve toggle: on = improve everything written; off = markers and commands only
- [ ] handwritten markers are detected cheaply and trigger the same actions

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T6.3.1 | US-6.3 | **AI toolbar** — A toolbar row with the AI actions based on your markers, plus Revise page, Pause and Terminal; each creates an intent with the selection (or the last piece) and wakes the session. | `ai: add the AI toolbar (T6.3.1)` | ✅ done `33f9c1fdf` |
| T6.3.2 | US-6.3 | **Auto-improve toggle** — Toggle button, menu entry and shortcut; per-rule dropdown (formulas, text, diagrams, colours); the mode is in the status strip and in every wake-up. | `ai: add the Auto-improve toggle (T6.3.2)` | ✅ done `b3edb143a` |
| T6.3.3 | US-6.3 | **Handwritten marker detection** — Detect markers among fresh strokes, geometrically and without a model: an asterisk is 2-4 short crossing strokes, '!' is a bar plus a dot, and a letter between them is read by the session from a crop. Each marker becomes an intent with its stroke ids; the session deletes them with elements_delete after acting (no separate marker_done tool needed). | `ai: detect handwritten markers (T6.3.3)` | ✅ done `b8f98f520` |

### S6.4 — Visible thinking → v1.1.4

**US-6.4** — As the owner, I want to see where and what the AI is thinking about, so that it is never silent.

- [ ] the zone being worked on is covered with a translucent grey veil and an animated outline, with a thinking icon and one-line status
- [ ] states queued, thinking, drawing, done or failed are visible
- [ ] the status strip shows the session state and the number of tasks in progress
- [ ] clicking the icon cancels (interrupts the session)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T6.4.1 | US-6.4 | **Thinking overlay** — Canvas overlay: a translucent grey veil plus animated outline over the zone, an icon, and a one-line status; follows zoom and scroll; the session sets it via intent_update / a thinking tool, the app sets it for its own intents. | `ai: show thinking zones on the canvas (T6.4.1)` | ✅ done `3b9b6f46e` |
| T6.4.2 | US-6.4 | **Status and cancel** — The status strip shows the session state, active tasks and the mode; clicking a thinking icon or Stop interrupts the session (Esc into its terminal) and clears the zone. | `ai: show assistant status and allow cancel (T6.4.2)` | ✅ done `c64b7f107` |

### S6.5 — Milestone 1 release → v1.2.0

**US-6.5** — As the owner, I want Milestone 1 tested and documented, so that it works for a full day of real use.

- [ ] integration tests with a fake companion (the terminal and the wake-up protocol)
- [ ] docs/assistant updated
- [ ] CHANGELOG and tag ai-v1.2.0

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T6.5.1 | US-6.5 | **Milestone 1 release** — Integration tests (fake companion script in the dock; wake-ups, hooks, toolbar intents, overlay), docs, CHANGELOG, tag ai-v1.2.0. | `ai: release 1.2.0 (Assistant Milestone 1) (T6.5.1)` | ✅ done `e93410ccb` |

## E7 — Parallel serving session (transactions) (release 1.3.0)

The serving session coordinates up to 5 background subagents: they think in parallel, and each one's edits land as one ordered, optionally stylus-like transaction (one undo step), with conflict checks, claims and leases.

### S7.1 — Transactions → v1.2.1

**US-7.1** — As the owner, I want every AI change to land as one ordered transaction, played instantly or like a stylus, so that parallel AI work never produces half-done or interleaved edits.

- [ ] a transaction is a draft plus an ordered list of operations (draw instant/stylus, delete, restyle, move)
- [ ] commit plays it in order as ONE undo step
- [ ] commits are serialized; the check refuses changes to strokes that changed since begin
- [ ] claims: no two open transactions on overlapping areas; at most 5 open; 10 min lease
- [ ] Stop finishes a playback instantly and aborts open transactions

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T7.1.1 | US-7.1 | **Undo sinks and hurry** — DrawApi and EditApi can hand their undo actions to a collector instead of registering them; AgentGate::hurry() finishes playbacks at once. | `ai: collect undo actions for transactions (T7.1.1)` | ✅ done `670d00741` |
| T7.1.2 | US-7.1 | **Transaction engine and tools** — api/Transactions (begin with page/region/base ids/zone, claims, leases, max 5, conflict check against the change log, ordered playback queue, one undo step, abort); MCP tools transaction_begin / transaction_commit / transaction_abort / transaction_list. | `ai: add edit transactions (T7.1.2)` | ✅ done `9b0e66e74` |

### S7.2 — Coordinator and subagents → v1.2.2

**US-7.2** — As the owner, I want the serving session to stay responsive while up to five subagents work in parallel, and to see and stop them.

- [ ] companion subagents: canvas-quick (fast model) and canvas-artist (strong model)
- [ ] coordinator instructions: delegate in the background, end the turn, small fixes itself, max_parallel from settings
- [ ] zones follow transactions (begin → thinking, commit → done, refused → failed)
- [ ] the status strip counts working subagents (hooks)
- [ ] Stop aborts all open transactions

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T7.2.1 | US-7.2 | **Subagents and coordinator** — Companion .claude/agents/canvas-quick.md and canvas-artist.md; coordinator rules in CLAUDE.md; setting max_parallel (1-5, default 5) in wake-ups. | `ai: coordinate background subagents (T7.2.1)` | ✅ done `2a43e646e` |
| T7.2.2 | US-7.2 | **Zones and subagent status** — Zones bound to transactions; the coordinator going idle no longer closes them; subagent count from PreToolUse(Agent) / SubagentStop hooks; Stop aborts transactions. | `ai: show parallel work (T7.2.2)` | ✅ done `c291c1f2d` |

### S7.3 — Release → v1.3.0

**US-7.3** — As the owner, I want parallel serving tested and documented.

- [ ] tests: parallel transactions, conflicts, claims, leases, undo, stylus playback, Stop
- [ ] CHANGELOG, tag ai-v1.3.0

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T7.3.1 | US-7.3 | **Release 1.3.0** — Tests, docs, CHANGELOG, tag. | `ai: release 1.3.0 (parallel serving session) (T7.3.1)` | ✅ done `7848a9783` |

## E8 — Touch zoom and deep zoom (release 1.4.0)

Pinch zoom on a touchscreen follows the fingers 1:1, even when a gesture daemon (Touchégg on Pop!_OS) turns the same pinch into zoom keystrokes. A toolbar toggle raises the maximum zoom to 3000%, with pages rendered only where visible.

### S8.1 — Pinch owns the zoom → v1.3.1

**US-8.1** — As the owner, I want pinch zoom on my laptop's touchscreen to follow my fingers smoothly.

- [ ] zoom keys, Ctrl+wheel and the zoom action are ignored while two fingers are down (and 0.4 s after)
- [ ] a pinch whose zoom sequence was ended elsewhere re-anchors instead of compounding
- [ ] opt-in touch/zoom trace for diagnosis on real hardware
- [ ] test: synthetic pinch with Touchégg-style keystrokes in the middle

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T8.1.1 | US-8.1 | **Pinch owns the zoom** — Root cause from a trace on the owner's laptop: Touchégg's default config maps a 2-finger pinch to repeated Ctrl+KP_Add/KP_Subtract; each 10% step ended the touch zoom sequence, after which the pinch factor multiplied the current zoom (compounding to 700%/30%). Fix in ZoomControl/TouchInputHandler; test hook test_touch (XOURNALAI_TEST_HOOKS=1). | `ai: pinch owns the zoom (T8.1.1)` | ✅ done `a16b038d0` |

### S8.2 — Deep zoom → v1.4.0

**US-8.2** — As the owner, I want to zoom much deeper than 700% when I need to, with a toggle button on the panel.

- [ ] ZOOM_DEEP toolbar toggle (next to Zoom in), remembered in settings: maximum 700% ↔ 3000%
- [ ] turning it off brings a deeper zoom back to 700%; slider, zoom action and MCP view follow the range
- [ ] beyond one buffer's budget pages and PDF backgrounds render only the visible part (plus a margin); memory stays flat
- [ ] tests: toggle and range, rendering at 3000%, scrolling renders the new part, memory; unit tests for the budget

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T8.2.1 | US-8.2 | **Deep zoom toggle and partial rendering** — Action ZOOM_DEEP + settings deepZoom; ZoomControl::setDeepZoom; view/RenderBudget.h; XojPageView keeps the visible range and a partial buffer extent; RenderJob renders partial buffers; PdfCache renders directly when a full page would not fit; icons xopp-zoom-deep; toolbar.ini.in. | `ai: deep zoom (T8.2.1)` | ✅ done `98f16e306` |

## E9 — No freezes (release 1.5.2)

Agents thinking never freeze the user's pen; agents drawing freeze it no longer than the drawing itself.

### S9.1 — Responsive while agents work → v1.5.0

**US-9.1** — As the owner, I want to keep drawing smoothly while the serving session and its subagents think and draw.

- [ ] a watchdog reports every UI stall over 50 ms with what caused it (app_status.ui, opt-in log)
- [ ] page_render, page_elements, layout_analyze, blocks_render run off the UI thread, results serialized there
- [ ] background rendering and previews hold the document lock only to copy what they draw
- [ ] tests: agents reading and the user drawing meanwhile on a 1M-point page: no stall of 100 ms or more

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T9.1.1 | US-9.1 | **Watchdog, agent reads off the UI thread, snapshot rendering** — Measured on a 1M-point page: page_render froze the UI 1.4 s, page_elements 0.25 s, the pen waited 0.38 s for preview rendering, an edit after a zoom ~3 s for page rendering. util/StallWatch; mcp/OffUi (render thread) + ToolResult::prepare + Response text; model/PageSnapshot for RenderJob, PreviewJob, api::renderPage; test_e9_responsiveness.py. | `ai: no freezes while agents work (T9.1.1)` | ✅ done `ce8ad5921` |
| T9.1.2 | US-9.1 | **Release 1.5.0** — Tests, docs, CHANGELOG, tag. | `ai: release 1.5.0 (no freezes) (T9.1.2)` | ✅ done `daf9739c7` |

### S9.2 — Fixes → v1.5.2

**US-9.2** — As the owner, I want 1.5.0's rendering to look right and page changes never to wait for other pages' rendering.

- [ ] page snapshots keep the page background (not "PDF background missing")
- [ ] removing a page view waits only for its own render job
- [ ] touch trace logs the touch handler's decisions (invalid touches, resets, blocking)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T9.2.1 | US-9.2 | **Snapshot backgrounds, scheduler waits, touch decisions** — snapshotPage used setBackgroundPdfPageNr, which makes any page a PDF page: every normal page showed 'PDF background missing' (1.5.0). XournalScheduler::removeSource waited for any running job: deleting a page froze the UI while another page rendered (3-4 s on a 1M-point page); now it waits on runningDoneCond only for a job of that source. Tests: PageSnapshotTest, test_e9_backgrounds.py. | `ai: fix page snapshot backgrounds and scheduler waits (T9.2.1)` | ✅ done `88d6fe516` |
| T9.2.2 | US-9.2 | **Review fixes** — Deep review of 1.3.1-1.5.1. Pinch guard could stay on after a third touch (palm) or a lost touch end, silently ignoring zoom keys, Ctrl+wheel, the slider and agents' zoom (test_e8_pinch test_4, failed before); now cleared on invalidation and expiring after 3 s without pinch movement. Watchdog polled every 5 ms (200 wake-ups/s on a laptop); now sleeps until a stall could have started. wait_for_user and draft(op=render) rendered on the UI thread; now off it. ZOOM_DEEP disabled in presentation mode like the other zoom actions. Checked and accepted: snapshot cost with 60 LaTeX formulas (no stalls), bbox-based partial copies (stroke bbox includes width), text-in-editing flag copied, prepared results not read afterwards. | `ai: review fixes (pinch guard, watchdog wake-ups, off-UI renders) (T9.2.2)` | ✅ done `0cb3399a9` |

## E10 — Ask: lasso + voice (experimental) (release 1.6.3)

Point and say: hold the pen's barrel button, speak, circle an area; a popover next to it shows the transcript (editable) and command icons; the request goes to the serving session as a zone. Speech-to-text runs locally (whisper.cpp).

### S10.1 — Local speech to text → v1.5.3

**US-10.1** — As the owner, I want my spoken requests transcribed locally, quickly and privately.

- [ ] xournalai-stt helper: whisper.cpp (pinned), microphone capture, JSON lines over stdin/stdout
- [ ] the app keeps it warm and restarts it; the model is downloaded once
- [ ] English (base.en/small.en); ~1-2 s for a short phrase on the owner's laptop

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T10.1.1 | US-10.1 | **STT helper (whisper.cpp)** — tools/stt or src/stt: FetchContent whisper.cpp v1.9.4, PortAudio capture, protocol start/stop/cancel/transcribe(wav); jfk.wav test; model benchmark. | `ai: local speech-to-text helper (T10.1.1)` | ✅ done `0f33feed0` |
| T10.1.2 | US-10.1 | **STT client in the app** — Spawn lazily, keep warm, restart on crash; model download to ~/.local/share/xournalai/models; settings assistant.stt_model. | `ai: speech-to-text client (T10.1.2)` | ✅ done `399699f97` |

### S10.2 — Ask → v1.5.4

**US-10.2** — As the owner, I want to circle an area with the pen while holding its button and say what I want.

- [ ] barrel button 1 held: listen; lasso points recorded; silent → the normal lasso selection only
- [ ] speech → popover at the lasso: transcript (editable), command icons (Improve, Illustrate, Write, Revise, Style, Explain, Summarize), Send, Esc
- [ ] AI toolbar Ask button: the same popover for the selection (or the visible page), with a hold-to-talk mic button
- [ ] the request becomes a zone and wakes the serving session; ask_get returns text, command, region, elements, crop

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T10.2.1 | US-10.2 | **Pen button listening and lasso capture** — A pen-button observer in the input system (no change to the normal tools); test_stylus hook. | `ai: pen button listening and lasso capture (T10.2.1)` | ✅ done `800e676d3` |
| T10.2.2 | US-10.2 | **Ask popover and toolbar button** — GtkPopover at the lasso; command icons; mic hold button; toolbar Ask. | `ai: ask popover (T10.2.2)` | ✅ done `8184f1a9f` |
| T10.2.3 | US-10.2 | **Ask requests to the serving session** — AskStore, ask_get tool, EventPump line, zone, Companion instructions. | `ai: ask requests (T10.2.3)` | ✅ done `77c134e84` |

### S10.3 — Release → v1.6.0

**US-10.3** — As the owner, I want Ask tested and documented.

- [ ] tests: STT on a sample, pen-button flow with synthetic events, silence keeps selection, popover, ask_get
- [ ] CHANGELOG, tag ai-v1.6.0

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T10.3.1 | US-10.3 | **Release 1.6.0** — Tests, docs, CHANGELOG, tag. | `ai: release 1.6.0 (ask: lasso + voice) (T10.3.1)` | ✅ done `c535a73a3` |

### S10.4 — Ask: the button and the recording indicator → v1.6.3

**US-10.4** — As the owner, I want a visible Ask tool, and to see clearly when it is recording.

- [ ] AI toolbar Ask (Ctrl+Alt+A): the next pen or mouse stroke is a lasso (no ink), then the popover
- [ ] the pen button while an ask is open dictates into it; a lasso drawn while holding it is a new ask
- [ ] a recording indicator next to the pen: pulsing red dot, live level bars, Listening; then Transcribing

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T10.4.1 | US-10.4 | **Ask button, lasso and recording indicator** — Stroke interceptor in AbstractInputHandler (pen / mouse only, consumed); dashed lasso on the overlay; pen button decides on release (lasso → new ask, else dictation); xournalai-stt level events (~20/s; the fake mic plays in real time); ThinkingOverlay recording pill. Fixes: the interceptor removed itself while running (crash); ui_interact clicks tool buttons' inner button (toggles). | `ai: ask button and recording indicator (T10.4.1)` | ✅ done `a704dc14c` |
| T10.4.2 | US-10.4 | **Visible thinking for asks, stylus-sized popover** — An ask delegated to a background subagent lost its zone when the coordinator ended its turn (the owner saw nothing while it worked): a turn that delegated (PreToolUse Agent/Task) keeps its zones until their transaction ends, thinking done/fail, Stop or 10 min; subagents claim their zone first and always end it. Zones get an animated status pill (waiting: breathing dot; thinking: orbiting dots; done: tick; failed: cross). The Ask popover is sized for a stylus (labelled 64x56 command buttons, 16 px text). The layer selector showed an agent's hidden draft layer as current (display only). | `ai: visible thinking for asks, stylus-sized popover (T10.4.2)` | ✅ done `db62f8e3b` |
| T10.4.3 | US-10.4 | **Reliable speech: silence check, instant microphone, feedback, log** — The owner's microphone is quiet (voice peaks ~0.015 RMS): the speech check (3x the quietest 20% of the clip, at least 0.012) judged continuous or quiet speech silent while the level bars moved. Now 3x the quietest 10%, capped to 0.005..0.02, and clearly loud clips go to whisper. The helper transcribes on a worker queue, so the microphone opens at once after a previous request. The client's state follows the facts (a late transcript no longer hides Listening). "Didn't catch that" in the pill. Diagnostics: ~/.cache/xournalai/speech.log and last-silent.wav. The fake microphone plays in real time (tests speak for 3 s). | `ai: reliable speech (silence check, instant microphone, feedback, log) (T10.4.3)` | ✅ done `39048bd34` |

## E11 — Recordings for the AI (release 1.7.2)

When a Xournal++ audio recording ends, the serving session hears about it (file, length, page, the strokes written meanwhile with their moment in the audio) and transcribes it with the owner's remote transcriber, or the local English-only one as a fallback.

### S11.1 — Recordings for the AI → v1.7.0

**US-11.1** — As the owner, I want the embedded Claude session to know about my audio recordings and use them.

- [ ] a recording that ends becomes an audio_recorded event and a for-your-information line for the serving session (setting assistant.share_recordings, on)
- [ ] strokes and texts written during a recording show their moment in it (audio: file, t)
- [ ] audio_transcribe: the local fallback (English, timestamped segments, off the UI and apart from Ask), transcripts kept in ~/Music/transcripts
- [ ] the companion instructions: the remote transcriber first, then the fallback; align with the strokes; act only when asked or with Auto-improve

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T11.1.1 | US-11.1 | **Recording notification and stroke audio times** — Recording observer in AudioController::stopRecording; EventHub audio_recorded; EventPump line; ElementJson audio {file, t}; test hook. | `ai: tell the serving session about recordings (T11.1.1)` | ✅ done `e569140c0` |
| T11.1.2 | US-11.1 | **audio_transcribe (local fallback)** — xournalai-stt reads any libsndfile format, long files, timestamped segments; a separate helper process per file. | `ai: audio_transcribe (local fallback) (T11.1.2)` | ✅ done `148855bce` |
| T11.1.3 | US-11.1 | **Release 1.7.0** — Companion instructions, tests, docs, CHANGELOG, tag. | `ai: release 1.7.0 (recordings for the AI) (T11.1.3)` | ✅ done `f05faf42b` |

### S11.2 — Paper and recording comfort → v1.7.1

**US-11.2** — As the owner, I want a quieter default paper and to always see that the recorder is on.

- [ ] a 'Fine graph' paper: squares half the size of Graph (2.5 mm), light thin lines; the default for new documents
- [ ] while the Xournal++ recorder records, the status line shows it: a pulsing red dot, the live microphone level, the time and a Stop button; hidden otherwise
- [ ] it works without the AI too (its own place at the bottom), and sits in the AI status line when that is there

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T11.2.1 | US-11.2 | **Fine graph paper, the default** — pagetemplates.ini: graph with r1=7.087, lw=0.35, f1=#e0e0e0; the default page template. | `ai: fine graph paper, the default (T11.2.1)` | ✅ done `b13a94eb9` |
| T11.2.2 | US-11.2 | **Recording indicator in the status line** — gui/RecordingIndicator (dot, level, time, Stop); PortAudioProducer peak level; AudioController recording listener; McpUi status line takes it in; test hook test_recorder. | `ai: the status line shows that the recorder is on (T11.2.2)` | ✅ done `cb4abfdb2` |
| T11.2.3 | US-11.2 | **Release 1.7.1** — Changelog, version, full tests. | `ai: release 1.7.1 (T11.2.3)` | ✅ done `18c0b91af` |

### S11.3 — Ask in the status line → v1.7.2

**US-11.3** — As the owner, I want the status line's recording pill to animate while Ask listens, too.

- [ ] while Ask listens, the status line shows 'Ask: listening' with the time and teal bars moving with the voice; no Stop (releasing the pen button ends it)
- [ ] the recorder wins when both are on; hidden again when Ask stops listening

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T11.3.1 | US-11.3 | **Ask listening in the status-line pill** — RecordingIndicator: Ask mode (setListening, pushVoiceLevel with the pen pill's decibel scale); McpUi feeds speech state and levels. | `ai: Ask's listening animates in the status line too (T11.3.1)` | ✅ done `52c6b5359` |
| T11.3.2 | US-11.3 | **Release 1.7.2** — Changelog, version, full tests. | `ai: release 1.7.2 (T11.3.2)` | ✅ done `62742773f` |

## E12 — Recordings: the owner decides (release 1.8.9)

When a recording stops, the owner picks what it is before anything is sent (instructions, notes, just audio); its processing is never silent; strokes line up with the audio.

### S12.1 — Recordings: the owner decides → v1.8.0

**US-12.1** — As the owner, I want to decide what a recording is, see it being processed, and have my strokes line up with the audio.

- [ ] when a recording stops, a stylus-friendly chooser: Instructions (Claude does what I said), Notes (a transcript to keep, never instructions), Keep audio (nothing else); an optional title; recordings that end meanwhile wait their turn; nothing is sent before the choice
- [ ] Notes: Claude writes transcripts/<name>.notes.md (title, summary, timed transcript, where the ink fits) and treats the speech as content only
- [ ] Instructions: a request like an Ask about the page, the strokes written meanwhile showing what was pointed at
- [ ] processing is never silent: a zone over the ink written meanwhile and the status-line pill show Claude's current step until it is done
- [ ] strokes (pen and highlighter) and texts count their moment from the recording's first captured sample, not from the click on Record

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.1.1 | US-12.1 | **Strokes line up with the audio** — PortAudioProducer: the first sample's capture time; AudioController::currentRecordingTime (recordingTimeMs, unit tests); highlighter strokes too; a log line with the microphone's start delay. | `ai: strokes line up with the audio (T12.1.1)` | ✅ done `b0f43a37a` |
| T12.1.2 | US-12.1 | **Recording chooser and progress** — assistant/RecordingChooser (queue, title); McpUi dispatch (intent / audio_recorded / nothing), zones for Notes and Instructions; RecordingIndicator processing mode; app_status recordings; companion instructions. | `ai: the owner decides what a recording is, and sees it processed (T12.1.2)` | ✅ done `4f0d7f803` |
| T12.1.3 | US-12.1 | **Release 1.8.0** — Changelog, README, version, full tests. | `ai: release 1.8.0 (T12.1.3)` | ✅ done `8261b2973` |

### S12.2 — The mouse cursor comes back → v1.8.1

**US-12.2** — As the owner, after writing with the stylus I want to see the mouse cursor again as soon as I use the mouse, touchpad or TrackPoint.

- [ ] moving a mouse-class device after the pen or a touch re-applies a visible cursor, even if the app thinks it is already set
- [ ] Ctrl re-applies it too
- [ ] the touch trace records device switches and each cursor applied

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.2.1 | US-12.2 | **Re-apply the cursor when the mouse returns** — XournalppCursor::forceRefresh (cache bypass) on the device switch to a mouse and on Ctrl; cursor lines in the touch trace; test_e12_cursor. | `ai: the mouse gets its cursor back after the pen (T12.2.1)` | ✅ done `1b1e417fe` |
| T12.2.2 | US-12.2 | **Release 1.8.1** — Changelog, version, full tests. | `ai: release 1.8.1 (T12.2.2)` | ✅ done `ace766b77` |

### S12.3 — Connect any agent → v1.8.2

**US-12.3** — As the owner, I want ready-made ways to connect other agents (Codex, Gemini, …) and a universal one, not only a Claude Code command.

- [ ] AI Agent → Connect an Agent… lists: universal mcpServers JSON (stdio, HTTP), URL and header, Claude Code (stdio, HTTP), Codex, Gemini CLI, OpenCode; paths and token filled in; Copy
- [ ] the commands are what the clients accept (checked against Claude Code, Codex and Gemini CLI)
- [ ] the dialog shows the token: agents can neither open nor read it
- [ ] mcp.json _help.connect has the same

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.3.1 | US-12.3 | **Connect an Agent dialog** — mcp/ConnectSnippets (unit tests); McpUi::showConnect; user-only dialog and action; _help.connect; README, CLIENTS, COVERAGE. | `ai: connect any agent (universal JSON, Claude Code, Codex, Gemini, OpenCode) (T12.3.1)` | ✅ done `9b6c4daaf` |
| T12.3.2 | US-12.3 | **Release 1.8.2** — Changelog, version, full tests. | `ai: release 1.8.2 (T12.3.2)` | ✅ done `01a59f7db` |

### S12.4 — What should AI do? → v1.8.3

**US-12.4** — As the owner, I want to say what the AI should do with a recording when I choose, by typing, by voice or with quick chips.

- [ ] the chooser has 'What should AI do?' with a mic (hold) and Send; the pen button dictates into it while it is open
- [ ] chips: Summarize, Key points, Action items, Flashcards, Translate…; they add to the request
- [ ] Enter / Send = Notes with the request; the request reaches Claude as the user's own instruction, the recording's speech stays content
- [ ] Keep audio with words: they become the recording's label (transcripts/<name>.label.txt)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.4.1 | US-12.4 | **'What should AI do?' in the recording chooser** — RecordingChooser request field, mic, chips, status; McpUi dictation routing (the chooser first), request in the hand-off, Keep label; companion; tests. | `ai: say what AI should do with a recording (T12.4.1)` | ✅ done `f32079c8e` |
| T12.4.2 | US-12.4 | **Release 1.8.3** — Changelog, version, full tests. | `ai: release 1.8.3 (T12.4.2)` | ✅ done `6a1134075` |

### S12.5 — Hangs leave evidence; painting never breaks → v1.8.4

**US-12.5** — As the owner, when the app freezes and I kill it, I want the cause on record; and a long status must never stop the window from painting.

- [ ] a UI stall over 2 s is written to the stall trace while it lasts (HANG line, what the UI thread is doing, every 10 s), with the stack of every thread once
- [ ] the stall trace says when each run started (pid, wall clock)
- [ ] zone statuses are cut by whole characters and drawn as valid UTF-8 (a cut Cyrillic status had broken all painting for minutes)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.5.1 | US-12.5 | **Hang reports with stacks; UTF-8-safe zone texts** — StallWatch: HANG lines while stalled, SIGUSR2 + backtrace_symbols_fd per thread, start line; ThinkingOverlay: cut by characters, g_utf8_make_valid; test_e12_hang. | `ai: hangs leave evidence; zone texts never break painting (T12.5.1)` | ✅ done `a8f7213be` |
| T12.5.2 | US-12.5 | **Release 1.8.4** — Changelog, version, full tests. | `ai: release 1.8.4 (T12.5.2)` | ✅ done `3d96e0112` |

### S12.6 — No tooltip freezes; a frozen app rescues the document → v1.8.5

**US-12.6** — As the owner, I don't want the app to freeze because of my pen, and if it ever freezes, my work must survive.

- [ ] no GTK tooltips for pen/eraser/touch hover outside the canvas (the 2026-10-05 freeze: a tooltip waited forever on XIQueryPointer while the Wacom pen was stuck in proximity); the mouse keeps tooltips
- [ ] a UI frozen for 15 s saves the document to the emergency file from the watchdog thread; the next start offers to restore it; a clean quit removes it
- [ ] hang reports keep coming after 60 s (the monotonic clock does not count suspend)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.6.1 | US-12.6 | **Tooltip guard, rescue save, long-hang reports** — gui/TooltipGuard (gdk event filter, unit tests); CrashHandler::rescueSave/discardRescue via StallWatch::setHangHandler; no SUSPEND cut-off; test_e12_hang. | `ai: no tooltip freezes with a pen; a frozen app rescues the document (T12.6.1)` | ✅ done `fd49b16a5` |
| T12.6.2 | US-12.6 | **Release 1.8.5** — Changelog, version, full tests. | `ai: release 1.8.5 (T12.6.2)` | ✅ done `f2e4f10e1` |

### S12.7 — A silent X server: exit instead of freezing; record the protocol → v1.8.6

**US-12.7** — As the owner, when the X server stops answering the app, I want it to save my work and close by itself, and I want evidence of which request went unanswered.

- [ ] after the rescue save, if the UI thread is stuck in libxcb (waiting for a reply or to send), the app closes its X connection and exits; the next start offers the rescue
- [ ] HANG lines and the rescue line carry the X request counters (last sent / last answered)
- [ ] tools/xournalai-xtrace.sh records the X11 traffic (xtrace, last 20000 lines) for the next freeze

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.7.1 | US-12.7 | **X hang recovery and protocol evidence** — control/XHangRecovery (reporter, rescue + shutdown of the X socket); StallWatch uiThreadIn (frames kept from the stack signal), setHangReporter; test_e12_x_hang (a helper grabs the test Xvfb); tools/xournalai-xtrace.sh. | `ai: a silent X server makes the app rescue and exit, with the request counters on record (T12.7.1)` | ✅ done `a709b0cb7` |
| T12.7.2 | US-12.7 | **Release 1.8.6** — Changelog, version, full tests. | `ai: release 1.8.6 (T12.7.2)` | ✅ done `016379678` |

### S12.8 — No stuck Ask; no deadlock behind a dialog → v1.8.7

**US-12.8** — As the owner, a pen-button Ask must end when I let go, even when the pen driver loses the release.

- [ ] the pen leaving the tablet's range (also a force-eject by the driver, which sends no release) releases the button
- [ ] a release or range exit anywhere in the window (not only over the canvas) ends a held Ask
- [ ] an Ask stops by itself after 2 minutes without a release
- [ ] while a modal dialog runs (Print holds the document lock all along), tools are refused at once with a clear reason instead of deadlocking the app

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.8.1 | US-12.8 | **Lost pen-button releases** — StylusInputHandler: PROXIMITY_OUT releases; penBarrelGone hook from the global event filter; AskController MAX_LISTEN_MS; test_pen op=out; test_e10_ask test_13. | `ai: an Ask never stays stuck listening (T12.8.1)` | ✅ done `008bfaf81` |
| T12.8.3 | US-12.8 | **No tools inside a dialog's nested loop** — McpProtocol::callTool refuses while g_main_depth() > 1 (Ctrl+P during agent work deadlocked on 2026-10-05); test_modal hook; test_e12_modal. | `ai: no tools while a dialog runs (no deadlock behind Print) (T12.8.3)` | ✅ done `50ec4596a` |
| T12.8.2 | US-12.8 | **Release 1.8.7** — Changelog, version, full tests. | `ai: release 1.8.7 (T12.8.2)` | ✅ done `1e330e036` |

### S12.9 — Crash evidence; restart after a frozen display; safer hint popover → v1.8.8

**US-12.9** — As the owner, after a crash or a frozen display I want my work back with the least fuss, and the next crash must name its cause.

- [ ] after an X freeze the app rescues the document, closes its display and restarts itself (at most twice in a row), offering the rescue
- [ ] the 'drawn here' hint popover is reused and closed, never destroyed while shown
- [ ] with the stall trace on: every destroyed popover is logged (name, what it points at, still shown?) and the first GTK 'not a widget' error gets a stack

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.9.1 | US-12.9 | **Restart, safer popover, GTK crash evidence** — XHangRecovery relaunch (XOURNALAI_RELAUNCHES); McpUi::flash reuses one popover; gui/GuiDiagnostics (destroy emission hook, log writer); stall::note; test_e12_popovers. | `ai: restart after a frozen display; safer hint popover; GTK crash evidence (T12.9.1)` | ✅ done `d126c41bd` |
| T12.9.2 | US-12.9 | **Release 1.8.8** — Changelog, version, full tests. | `ai: release 1.8.8 (T12.9.2)` | ✅ done `44d7ee1aa` |

### S12.10 — Name the request the X server leaves unanswered → v1.8.9

**US-12.10** — As the owner, I want the next frozen display to say exactly which request the X server never answered, without running a tracer.

- [ ] the app keeps the last 64 X requests it sent (type and number), always on, at negligible cost
- [ ] HANG and rescue lines list the unanswered requests by name (core requests and extensions) and the ones just before

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T12.10.1 | US-12.10 | **Built-in X request log** — XHangRecovery: exported xcb_writev records request headers (ring of 64) and calls the real one; extension names from XListExtensions; reporter lists unanswered/before; test_e12_x_hang checks it. | `ai: HANG reports name the X requests left unanswered (T12.10.1)` | ✅ done `84580fd01` |
| T12.10.2 | US-12.10 | **Release 1.8.9** — Changelog, version, full tests. | `ai: release 1.8.9 (T12.10.2)` | ✅ done `964ee5f9b` |

## E13 — Reach for it: terminals you control, tools under the thumb (release 1.9.1)

The owner opens any terminal command (not only presets), closes tabs they don't want, and reaches the tools they use most with one big touch-friendly button.

### S13.1 — Terminal tabs the owner controls → v1.9.0

**US-13.1** — As the owner, I want to open a terminal tab with any command line I paste or type (e.g. one of my OpenCode configurations), reuse it later, and close tabs I don't need.

- [ ] the + menu has 'Custom command…': a command line, an optional tab name and folder
- [ ] custom commands are remembered and offered again in the + menu (most recent first, removable)
- [ ] every tab has a close button; closing ends its program; restart/close act on the tab shown even after reordering

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T13.1.1 | US-13.1 | **Close terminal tabs** — Close button on each tab label and in the header; SIGHUP the process; tabs map by page widget, not position; closing the serving tab reports its exit. | `ai: close terminal tabs (T13.1.1)` | ✅ done `2892712a9` |
| T13.1.2 | US-13.1 | **Custom command tabs** — 'Custom command…' popover in the + menu (command, name, folder); remembered in ai-terminals.json; recent ones in the menu with a way to forget them. | `ai: open a terminal tab with any command (T13.1.2)` | ✅ done `6cb67fc00` |

### S13.2 — Quick tools under the thumb → v1.9.0

**US-13.2** — As the owner, I want one big button I can press with a thumb, finger or the pen that shows the ten tools I use most as large buttons.

- [ ] a big Tools button starts the AI toolbar; F9 opens it too
- [ ] a grid of large buttons: pen, eraser, highlighter, select, hand, text, Ask, Command, Terminal, size, new page, undo, redo
- [ ] ordered by how often the owner uses each tool (remembered), ten shown

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T13.2.1 | US-13.2 | **Quick tools palette** — QuickTools: big toolbar button, popover grid of 64px tiles, actions through the action database and win.* actions; usage counts (palette and tool changes) in quick-tools.json rank the ten shown. | `ai: quick tools palette (T13.2.1)` | ✅ done `5c39a5a82` |

### S13.3 — Release 1.9.0 → v1.9.0

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T13.3.1 | US-13.1 | **Release 1.9.0** — Changelog, version, full tests. | `ai: release 1.9.0 (T13.3.1)` | ✅ done `625aac495` |

### S13.4 — One big Tools button for tablet mode → v1.9.1

**US-13.4** — As the owner working in tablet mode (pen, fingers, voice; no keyboard), I want one big, colourful Tools button in one convenient place, opening large controls, without another row cluttering the interface.

- [ ] the button stands to the right of the toolbars, as tall as all of them; no row of its own
- [ ] it opens large buttons in fixed groups: Select (lasso, rectangle, pointer), Draw, Size (all five thicknesses), Page, AI; the tool and size in use are marked
- [ ] hidden from AI Agent → Big Tools button; the choice is remembered
- [ ] a big Record button next to Tools (experimental) starts and stops recording; while recording it shows Stop with the time and rays and a core that follow the microphone level

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T13.4.1 | US-13.4 | **Big Tools button beside the toolbars** — QuickTools::placeButton: toolbar rows into a column, gradient button to their right; grouped popover incl. TOOL_SELECT_OBJECT; win.ai-tools-button (stateful, remembered). Replaces the Touch bar row tried first. | `ai: one big Tools button beside the toolbars (T13.4.1)` | ✅ done `01bdadc12` |
| T13.4.3 | US-13.4 | **Big Record button that listens** — QuickTools: record button beside Tools toggling AUDIO_RECORD; drawn face: idle red dot, recording: rays from RecordingIndicator::recentLevels, core breathing with the smoothed level, Stop m:ss; win.ai-record-button (remembered). | `ai: big Record button that follows the microphone (T13.4.3)` | ✅ done `c0abbe529` |
| T13.4.4 | US-13.4 | **Big Full screen button** — QuickTools: full-screen button beside Record toggling Action::FULLSCREEN; label follows the state (Full screen / Window, also after F11); win.ai-fullscreen-button (remembered). | `ai: big Full screen button (T13.4.4)` | ✅ done `282fa4da2` |
| T13.4.5 | US-13.4 | **Focus: only the big buttons** — QuickTools::setFocused hides the menu bar, every toolbar (all positions) and the sidebar, restoring each as it was without touching the saved settings; win.ai-focus (F9, remembered) and win.ai-focus-button. Tools menu: two columns, Colour (palette, 2x5), Shapes (line, rectangle, ellipse, arrow, recognizer: with the pen) and View (zoom). | `ai: Focus button, colours, shapes and zoom in the Tools (T13.4.5)` | ✅ done `cf453f4fd` |
| T13.4.2 | US-13.4 | **Release 1.9.1** — Changelog, version, full tests. | `ai: release 1.9.1 (T13.4.2)` | ✅ done `18f82e639` |

## E14 — Point while you speak: the AI highlighter (release 1.10.0)

Besides the AI lasso (mark first, then say), the owner can highlight what they mean while they say it; the marks stay as a record of the question.

### S14.1 — AI highlighter → v1.10.0

**US-14.1** — As the owner, I want to highlight places on the page while I explain what I want, and get the same Ask popover (transcript to edit, record more, commands) when I pause, so pointing and speaking happen together.

- [ ] the AI highlighter draws like the highlighter, in its own violet, and starts listening on the first touch
- [ ] strokes within a short pause belong to one request; the pause opens Ask with the transcript
- [ ] the request names the places highlighted and what is under them (not the highlights)
- [ ] the marks stay by default; AI Agent → AI highlighter keeps its marks switches to removing them on send
- [ ] the tool used before is given back; Ctrl+Alt+H, a Highlight button on the AI row and a tile in Tools
- [ ] in Focus, the strip's left side holds big icon buttons in priority order; full screen on/off keeps Focus

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.1.1 | US-14.1 | **AI highlighter** — McpUi::armHighlighter: highlighter in AI violet via a pass-through stroke interceptor; speech starts on pen-down; HIGHLIGHT_PAUSE_MS after the last stroke finishHighlight → showAsk with highlights and marks (new AI-violet highlighter strokes); submitAsk lists the places and the elements under them, removes marks unless assistant.highlighter_keeps_marks; win.ai-highlighter (Ctrl+Alt+H), win.ai-highlighter-keep; AI row button aiHighlighter; Tools tile ai-highlighter. | `ai: AI highlighter: highlight while speaking, then Ask (T14.1.1)` | ✅ done `d4df929f1` |
| T14.1.3 | US-14.1 | **Tablet buttons in Focus; Focus survives full screen** — QuickTools::buildTablet: icon tiles left of the strip in Focus (AI highlighter, AI lasso, pen + palette button while the pen is in use, eraser, select | command, terminal), the last giving way when narrow (scroller without scroll bars); reapplyFocus after FULLSCREEN changes; AI row Highlight icon-only. | `ai: tablet buttons in Focus, Focus survives full screen (T14.1.3)` | ✅ done `545b9108f` |
| T14.1.2 | US-14.1 | **Release 1.10.0** — Changelog, version, full tests. | `ai: release 1.10.0 (T14.1.2)` | ✅ done `399d196c1` |

### S14.2 — A smarter AI highlighter: marks that go away, shapes that count → v1.11.0

**US-14.2** — As the owner, I want the AI highlighter's marks to disappear once the AI has dealt with them (permanent highlighting is the regular highlighter's job), and the AI to understand the exact shape I highlighted, so I can point at small details: a few letters, one symbol, a corner of a drawing.

- [ ] AI marks are drawn as an overlay, not ink: no undo entries, never saved or exported, not seen by the AI as elements
- [ ] the marks stay while the AI works on the request and vanish when it is done; at once on Cancel; also when the request fails or its zone is dismissed
- [ ] several separate marks still make one request (the pause-based grouping is unchanged)
- [ ] the request carries each mark's footprint (simplified path + width), the elements the footprint touches (partly covered ones flagged), and the surrounding area as context
- [ ] the AI also gets a render of the area with everything outside the footprint faded
- [ ] the 'keeps its marks' setting is removed

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.2.1 | US-14.2 | **AI marks as an overlay** — Interceptor consumes the strokes (no ink); ThinkingOverlay draws them as translucent violet highlighter paths per request; removed on zone done / failed / dismissed and on Cancel; drop assistant.highlighter_keeps_marks and win.ai-highlighter-keep. | `ai: AI highlighter marks as an overlay that goes away (T14.2.1)` | ✅ done `a65e7516d` |
| T14.2.2 | US-14.2 | **Footprints in the request** — AskCapture keeps each mark's path + width; submitAsk hit-tests elements against the footprint (distance to path <= width/2), flags partial coverage, sends simplified paths (<= ~30 points each) and the area with a margin as context. | `ai: AI highlighter sends the shape highlighted (T14.2.2)` | ✅ done |
| T14.2.3 | US-14.2 | **Faded-outside render** — Render of the request area with everything outside the footprints faded, attached to the request for the serving session. | `ai: AI highlighter render of the highlighted detail (T14.2.3)` | ✅ done |
| T14.2.4 | US-14.2 | **Release 1.11.0** — Changelog, version, full tests. | `ai: release 1.11.0 (T14.2.4)` | ✅ done `ec5934b73` |

### S14.3 — The AI highlighter ends when the owner says so → v1.11.1

**US-14.3** — As the owner, I want to highlight as many places as I like at my own pace while I keep talking, and finish myself — no pause ends it behind my back and leaves a pen line on the page.

- [ ] no pause between strokes ends the request
- [ ] while on, the strip becomes a mode bar: level, time, marks, Discard and a big Done, at the strip's height (the page doesn't move)
- [ ] 10 s with no marks and no words finishes it by itself; the last 3 s are counted down on Done
- [ ] Escape discards; tapping the AI highlighter (or Ctrl+Alt+H) again, or picking a tool, is Done

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.3.1 | US-14.3 | **Mode bar and owner-ended AI highlighting** — QuickTools::showModeBar hides the strip's children and shows level / info / Discard / Done at the strip's height; McpUi highlightTick (250 ms): time, marks, quiet = no marks and no voice (rms > 0.03) for 10 s → finishHighlight, countdown in the last 3 s; Escape on the window discards; action off / tool picked with marks = Done. | `ai: AI highlighter ends when the owner says so (mode bar) (T14.3.1)` | ✅ done `c58b82002` |
| T14.3.2 | US-14.3 | **Release 1.11.1** — Changelog, version, full tests. | `ai: release 1.11.1 (T14.3.2)` | ✅ done `9a9fb8780` |

### S14.4 — A finger switch among the big buttons → v1.11.2

**US-14.4** — As the owner, I want to switch touch drawing on and off from the big buttons, so I can highlight with a finger without taking out the stylus, and switch back to palm-safe stylus-only.

- [ ] the switch is the first tablet button, with a gap after it (hard to hit by accident); also in Tools → View
- [ ] off by default, as before: only the stylus draws
- [ ] it toggles the app's own touch-drawing setting (the menu and settings agree)
- [ ] with touch drawing on, a finger in the AI highlighter or lasso marks (not ink); being a switch, it leaves them on
- [ ] Auto-improve is the second switch (same setting as the menu and Ctrl+Alt+I), also in Tools → AI

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.4.1 | US-14.4 | **Touch switch; fingers in the AI modes** — QuickTools: 'touch' tile first in TABLET (gap after it) and in Tools → View, toggling TOGGLE_TOUCH_DRAWING, marked when on; AbstractInputHandler::intercept passes touchscreen events when touch drawing is on. | `ai: touch-drawing switch first among the big buttons (T14.4.1)` | ✅ done `6b813ed54` |
| T14.4.3 | US-14.4 | **Auto-improve switch** — QuickTools: 'auto-improve' tile second in TABLET (switches group, gap after), labelled on/off, toggling win.ai-auto-improve; also in Tools → AI. | `ai: Auto-improve switch among the big buttons (T14.4.3)` | ✅ done `e80e4fe38` |
| T14.4.2 | US-14.4 | **Release 1.11.2** — Changelog, version, full tests. | `ai: release 1.11.2 (T14.4.2)` | ✅ done `b1051317f` |
| T14.4.4 | US-14.4 | **Release 1.11.3** — Changelog, version, full tests. | `ai: release 1.11.3 (T14.4.4)` | ✅ done `e7d174ca6` |
| T14.4.5 | US-14.4 | **AI lasso icon** — Violet lasso icon xopp-ai-lasso (three icon themes) for the AI lasso in the big buttons, Tools and the AI toolbar; tile label 'AI lasso'. | `ai: violet AI lasso icon (T14.4.5)` | ✅ done `33b9d5f84` |
| T14.4.6 | US-14.4 | **Release 1.11.4** — Changelog, version, full tests. | `ai: release 1.11.4 (T14.4.6)` | ✅ done `c52fbf7e8` |

### S14.5 — A see-through AI terminal → v1.12.0

**US-14.5** — As the owner, I want the AI terminal to float over the page with a see-through background, so I can see my notes while I talk to the AI.

- [ ] the terminal floats over the bottom of the page by default; the page keeps its full height
- [ ] an opacity slider in the terminal's header, 70% by default, remembered; the text stays solid
- [ ] a pin button switches back to the docked split (and to floating again)
- [ ] the floating terminal can be resized by dragging its top edge

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.5.1 | US-14.5 | **Floating see-through terminal** — TerminalDock: a GtkOverlay over the split; floating dock as an overlay child at the bottom with a resize grip; header slider (aiTerminalOpacity, 20–100%, 70% by default) sets the VTE background alpha; pin toggle (aiTerminalPin) docks it; settings in ai-terminal-view.ini. | `ai: floating see-through AI terminal (T14.5.1)` | ✅ done `be2da72eb` |
| T14.5.2 | US-14.5 | **Release 1.12.0** — Changelog, version, full tests. | `ai: release 1.12.0 (T14.5.2)` | ✅ done `1b769b201` |

### S14.6 — A readable see-through terminal → v1.12.1

**US-14.6** — As the owner, I want the see-through terminal to stay readable over white paper, so its text doesn't vanish into the page.

- [ ] automatic colours (on by default): dark text on a see-through white background over a light page, light on black otherwise
- [ ] a toggle in the terminal's header switches automatic colours off (always dark); remembered

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.6.1 | US-14.6 | **Automatic terminal colours** — TerminalDock: aiTerminalAutoColors toggle; the current page's background brightness (polled every 0.5 s) picks a light look (white background, light-background palette) or VTE's own dark one; auto_colors in ai-terminal-view.ini. | `ai: automatic light/dark colours for the AI terminal (T14.6.1)` | ✅ done `3ee7e641c` |
| T14.6.2 | US-14.6 | **Release 1.12.1** — Changelog, version, full tests. | `ai: release 1.12.1 (T14.6.2)` | ✅ done `a4efee3dd` |

### S14.7 — The AI highlighter hears a quiet microphone → v1.12.2

**US-14.7** — As the owner, I want the AI highlighter to stay on while I'm still talking, and to see clearly when it is about to finish by itself.

- [ ] talking keeps it on through the built-in microphone (speech ~0.01 RMS): voice is judged against the microphone's own noise floor, not a fixed 0.03
- [ ] the last 5 s before it finishes by itself are counted down on Done: an orange button, 'Done in N…' and a draining bar

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.7.1 | US-14.7 | **Adaptive voice detection and a visible countdown** — assistant/VoiceActivity: a level is voice above 3x the 10th percentile of the last ~10 s, clamped 0.005..0.02 (as the speech helper's silence check); McpUi uses it for lastVoiceUs. Countdown 5 s, tick 100 ms; QuickTools::setModeCountdown draws a draining bar on Done (xoai-mode-counting). Level meter scaled for quiet mics. VoiceActivityTest, test_e14_highlighter_quiet_voice.py. | `ai: the AI highlighter hears a quiet microphone; a visible countdown (T14.7.1)` | ✅ done `42c4bfde2` |
| T14.7.2 | US-14.7 | **Release 1.12.2** — Changelog, version, full tests. | `ai: release 1.12.2 (T14.7.2)` | ✅ done `8102f4366` |

### S14.8 — A crash never leaves a dead app behind → v1.12.3

**US-14.8** — As the owner, I want a crash to end the app cleanly (document rescued), so the next start works, its AI terminal included.

- [ ] a crash whose handler hangs (heap corruption: the allocator's lock held) rescues the document and ends the app within seconds; a 25 s deadline ends it in any case
- [ ] the crashing thread's stack is written first, without allocating (journal and ~/.cache/xournalai/crash.log), so a repeat names its culprit; the corruption of 2026-10-17 was looked for under AddressSanitizer

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.8.1 | US-14.8 | **A stuck crash handler ends the app** — 2026-10-17 11:25:39: heap corruption (malloc_consolidate: unaligned fastbin chunk) aborted the app; the crash handler's own allocation (getVersionInfo → g_get_os_info → realloc) waited forever on the allocator's lock, so the dead process kept its window, MCP port 7474 and helpers, and the next start's AI terminal could not connect. CrashHandler: alarm(25) deadline, crashing(); XHangRecovery: after the rescue, a crashed app _exits. test hook test_crash_stuck; test_e12_crash_stuck.py. | `ai: a crash whose handler hangs no longer leaves a dead app holding the MCP port (T14.8.1)` | ✅ done `242bb6977` |
| T14.8.2 | US-14.8 | **Look for the heap corruption; a crash writes its evidence first** — AddressSanitizer build (build-asan; XOAI_BINARY runs the integration tests against it; run.py --file): the whole suite and a 7-minute soak of that session's mix (soak_session.py: see-through terminal with scrolling output, auto colours over light and dark pages, opacity/dock/resize, AI highlighter, zoom and scroll over text pages, thinking zones) found no memory error, so the corruption is not reproduced. CrashHandler now writes the crashing thread's stack (backtrace_symbols_fd) to stderr and crash.log before anything allocates, and prints a version text prepared at start (building it was where the handler hung). | `ai: a crash writes its stack first, without allocating; ASan soak of the session (T14.8.2)` | ✅ done `747a61b6f` |
| T14.8.3 | US-14.8 | **Release 1.12.3** — Changelog, version, full tests. | `ai: release 1.12.3 (T14.8.3)` | ✅ done `4f180c1cc` |

### S14.9 — A freeze restarts one app, and its report names the X request → v1.12.4

**US-14.9** — As the owner, I want a frozen app to come back as exactly one app with my document, its AI terminal working, and the freeze report to say what the X server left unanswered.

- [ ] once an agent's bridge has seen the app running, it never starts another: a tool call while the app is closed or restarting is answered with 'not open', and the tools come back when the restarted app appears
- [ ] the X request log runs on the owner's laptop (app_status ui.x_requests_logged > 0), so the next HANG report names the unanswered requests
- [ ] the restart after a freeze works also when an update replaced the binary while the app ran
- [ ] a freeze report says whether the X server answers a second connection (it ignores only this app, or it is busy or grabbed)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.9.1 | US-14.9 | **An agent's bridge never starts a second app after a freeze** — 2026-10-17 22:44: the X server stopped answering (UI thread in XSync from GDK's paint); the watchdog rescued the document and restarted the app, and at the same moment the bridge of the agent in the dying app's AI terminal got 'connection refused' on a tool call and started another app (--mcp, in the terminal's scope). That one took port 7474 and the rescue; the restarted one had the file but no MCP port. The bridge now answers a tool call offline once the app has been seen (closed or restarting) instead of starting it. test_e5_bridge_lifecycle: a call first after the app went away. | `ai: an agent's bridge never starts a second app after a freeze (T14.9.1)` | ✅ done `29dc3ffc5` |
| T14.9.2 | US-14.9 | **The X request log runs on the laptop** — None of the 29 HANG reports on the laptop named a request, though the log works in tests: it was enabled only if XGetXCBConnection (libX11-xcb) was already loaded when the watchdog started, and nothing but PulseAudio's library loads it (a race). Now dlopen'ed; HANG lines say how many requests were logged; app_status ui.x_requests_logged. | `ai: the X request log runs on the laptop; app_status counts what it logged (T14.9.2)` | ✅ done `2911ffc50` |
| T14.9.3 | US-14.9 | **The restart after a freeze works after an update** — 2026-10-17 23:04 and 23:17: the frozen app was rescued and closed, but its restart never came: an install had replaced the binary while it ran, so /proc/self/exe ended in " (deleted)" and the restart's exec failed silently (only the old bridge's launch, or the owner, brought the app back). Util::getExeFile() strips the suffix (the path names the updated binary) and is used for the restart, the companion's hooks and MCP command, the stdio bridge and the connect snippets; the restart warns if the file is gone. test_e12_x_hang_relaunch: the app runs from its own link to the binary, an update replaces it, a grab freezes it, and the restart must exec (XOURNALAI_TEST_RELAUNCH). | `ai: the restart after a freeze works after an update replaced the binary (T14.9.3)` | ✅ done `2e9cdd3fd` |
| T14.9.4 | US-14.9 | **A freeze report says whether the X server answers others** — 23:17 the first freeze with the request log on: the server answered up to CopyArea (the end of a frame) and nothing after (FreePicture, FreePixmap, SYNC SetCounter, then GDK's GetProperty for an event), and only the UI thread was in X. To tell a server that ignores only this app from one that is busy or grabbed, the first HANG report opens a second connection (xcb, its own thread) and times a round trip; HANG lines and the journal's 'Frozen' message say whether it was answered. | `ai: a freeze report says whether the X server answers a second connection (T14.9.4)` | ✅ done `2370f4854` |
| T14.9.5 | US-14.9 | **Release 1.12.4** — Full check: build-noterm and build-nomcp ok; unit tests 204/208 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 244/245 (test_e13 test_7_record_button_beside_tools failed once, its file passes 17/17 on rerun). | `ai: release 1.12.4 (a freeze restarts one app, and its report names the X request) (T14.9.5)` | ✅ done `b478a34b0` |

### S14.10 — Closing and X errors do not crash the app → v1.12.5

**US-14.10** — As the owner, I want a logout or kill to close xournalai cleanly, and an X error to be reported instead of ending the app.

- [ ] SIGTERM saves the document for recovery and exits without a crash
- [ ] an X protocol error during drawing names the failing request and does not end the app

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.10.1 | US-14.10 | **SIGTERM no longer crashes at exit** — 2026-10-17 23:36: SIGTERM (force close: emergency save, then exit(1)) ended in 'terminate called without an active exception': the stall watchdog's static std::thread was still joinable when exit() ran the static destructors, so every logout or kill was a crash. StallWatch stops itself at exit (atexit, before the thread's destructor); stop() detaches when exit() runs on the watchdog itself. test_e12_sigterm: SIGTERM to the app alone (the harness's group kill ended Xvfb first, hiding it). | `ai: SIGTERM no longer crashes xournalai at exit (T14.10.1)` | ✅ done `c361face9` |
| T14.10.2 | US-14.10 | **An X error during drawing is logged and survived** — 2026-10-18 00:00:08: the app ended with SIGTRAP in GDK's X error handler, reached from GTK's drawing (cairo_mask -> XRenderCreatePicture; the error came in with the events). Its text was lost (stderr went to a deleted file). Xlib asks the display's wire-to-error converters (XESetWireToError) before the error handler, for every error; ours logs and drops RENDER's errors and BadAlloc, and passes the rest on (GDK and its traps unchanged). XESetError was no use: Xlib lets extensions suppress only errors seen by _XReply. app_status ui.x_errors_survived; test hook test_x_error (a FreePicture of no picture: BadPicture); test_e12_x_error. | `ai: an X error during drawing is logged and survived (T14.10.2)` | ✅ done `60f765233` |
| T14.10.3 | US-14.10 | **Release 1.12.5** — Full check: build-noterm and build-nomcp ok; unit tests 204/208 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 247/247. | `ai: release 1.12.5 (closing and X errors do not crash the app) (T14.10.3)` | ✅ done `13eedb528` |

### S14.11 — Exports do not freeze the pen → v1.12.6

**US-14.11** — As the owner, I want the companion's exports to run without freezing my pen, and a freeze report to say at once whether the X server is to blame.

- [ ] export png and svg render off the UI thread
- [ ] a freeze report says whether the UI thread waits on the X server, and whether the server answers a second connection

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.11.1 | US-14.11 | **export renders png and svg off the UI thread** — 2026-10-18 04:27 and 04:32: 2.7 s and 2.6 s freezes while the companion exported pages (the UI thread in cairo_stroke from renderPage, called by export on the UI thread; 32 such stalls in the trace, the longest 9.8 s). png and svg now render on the render thread (runOffUi, as render does); paths and overwrite permission are checked first on the UI thread. renderSvg copies the page under a short lock, as renderPage. test_e9 test_5. | `ai: export renders png and svg off the UI thread (T14.11.1)` | ✅ done `603f5b649` |
| T14.11.2 | US-14.11 | **A freeze report says whether the UI waits on X** — The 2-s HANG report asked the second connection and printed its state at once: 'no answer either (0 ms)', also when the UI thread was only busy drawing an export (04:27), which looked like an X server stall. The report now says whether the UI thread is on X and waits briefly for the second connection. | `ai: a freeze report says whether the UI waits on X (T14.11.2)` | ✅ done `b2aba4b44` |
| T14.11.3 | US-14.11 | **Release 1.12.6** — Full check: build-noterm and build-nomcp ok; unit tests 203/208 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 247/248: test_e13 test_7 read the toolbar row's geometry before it was laid out again after test_6 (1 in 2 runs); it now waits for it (5/5). | `ai: release 1.12.6 (exports do not freeze the pen) (T14.11.3)` | ✅ done `bebcb63a6` |

### S14.12 — PDF exports do not freeze the pen → v1.12.7

**US-14.12** — As the owner, I want the companion's PDF and xopp exports to run without freezing my pen.

- [ ] export pdf and xopp run off the UI thread

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T14.12.1 | US-14.12 | **export writes pdf and xopp off the UI thread** — 2026-10-19 08:33-09:45 (1.12.6): with png and svg off the UI thread, the remaining freezes were PDF exports of the magazine, 2.1-3.8 s each, while the companion exported issue PDFs (and file_open, 1-2 s, which replaces the document anyway). pdf and xopp are now written on the render thread (runOffUi). The exporters read the live document, so it is held for reading during a PDF export (an edit then waits for it; before, everything waited); xopp prepares under the lock and writes without it. test_e9 test_5 exports pdf and xopp too (1.12.6: 1565 and 981 ms stalls). | `ai: export writes pdf and xopp off the UI thread (T14.12.1)` | ✅ done `97a77ada2` |
| T14.12.2 | US-14.12 | **Release 1.12.7** — Full check: build-noterm and build-nomcp ok; unit tests 204/208 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 248/248. | `ai: release 1.12.7 (PDF exports do not freeze the pen) (T14.12.2)` | ✅ done `f455b1dc6` |

## E15 — Your keyboard, your view: agents work beside you (release 1.15.1)

While agents save, add pages and draw, the owner keeps typing and dictating where they type, and keeps looking at the page they work on; the view moves only when they ask (docs/mcp/FOCUS.md).

### S15.1 — The keyboard stays where you type → v1.13.0

**US-15.1** — As the owner, I want the AI terminal and the AI input fields to keep my keyboard while saves, page changes and agents' work happen, so that typing and dictation are never cut off.

- [ ] a save (mine or an agent's) leaves the keyboard in the terminal or the AI input field
- [ ] a page becoming current (my scrolling, an agent's page work) does not take the keyboard from them
- [ ] an agent's save does not grey out the window or show a busy cursor, and keeps my selection
- [ ] drawing while a key is held in the terminal (dictation) keeps the keyboard there; otherwise drawing takes it, as before

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.1.1 | US-15.1 | **Programmatic focus moves leave the keyboard in text inputs** — 2026-10-20: after every save the canvas took the keyboard (BlockingJob), and so did every page that became current (XojPageView::setSelected), also from the owner's own scrolling; a space held for dictation then scrolled the canvas a screen per repeat. XournalView::requestFocus no longer takes the keyboard from a visible, sensitive text input (the AI terminal, an entry, a text view); the user's own clicks and strokes on the canvas still do. | `ai: the keyboard stays in the terminal and the AI input fields (T15.1.1)` | ✅ done `9fdb87f3c` |
| T15.1.2 | US-15.1 | **Agents' saves are quiet** — file_save / file_save_as from agents (after every commit in the magazine workflow) greyed out all actions, set a busy cursor, showed the status bar (the layout jumped) and dropped the user's selection and text editing. Agents' saves now run without blocking the window; the AI strip shows the running save. A selection stays: its elements are written at their places from copies; text being edited is saved as it was before the edit. | `ai: agents' saves do not block the window or drop the selection (T15.1.2)` | ✅ done `7c127d3f3` |
| T15.1.3 | US-15.1 | **Drawing while a key is held keeps the keyboard** — Holding space in the terminal (Claude Code's push-to-talk) while starting a stroke moved the keyboard to the canvas. While a key is held in a text input (pressed and still repeating, so a lost release cannot stick), a stroke draws but leaves the keyboard there. Its own commit, so that it can be reverted alone. | `ai: drawing while a key is held in the terminal keeps the keyboard there (T15.1.3)` | ✅ done `82afa5b46` |
| T15.1.4 | US-15.1 | **Release 1.13.0** — Full check: build-noterm and build-nomcp ok; unit tests 208/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 255/255. | `ai: release 1.13.0 (the keyboard stays where you type) (T15.1.4)` | ✅ done `d5f50c1d9` |

### S15.2 — The agent works without moving your view → v1.14.0

**US-15.2** — As the owner, I want agents to change pages and layers without scrolling me away, and to move my view only when I ask for it.

- [ ] page_manage and layer_manage change any page without scrolling the view or changing the current page
- [ ] inserting or deleting a page above the visible one keeps what I see in place
- [ ] an agent's work drops my selection only when it works on the same page
- [ ] where an agent works is shown as a chip 'AI -> p. N' that takes me there
- [ ] agents move the view only through the navigation tools, and not in the middle of my stroke or selection

**US-15.3** — As the owner, I want undo, redo, the highlighter and the object pointer among the big buttons, so that in focus mode I reach them with the pen without the toolbars.

- [ ] the big-button panel has undo, redo, the highlighter and the select-object pointer
- [ ] they work in focus mode, where the toolbars are hidden

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.2.1 | US-15.2 | **Page operations without the view** — page_manage drove the window: it scrolled to the page and made it current before and after each window action (insert jumped the owner's view to the new page). insert, delete, duplicate, move, background and size now work on the given page in the document, with the same undo steps; only goto (and an explicit show) moves the view. On the way: a size change on another page re-fitted the zoom to that page (ZoomControl now follows the current page only), and 'Page n of N' and the goto actions followed only a change of the current page (now also any insert or delete). An insert with a background is one undo step. test_e15_view 3/3 (all fail on 1.13.0). | `ai: page operations no longer move the user's view (T15.2.1)` | ✅ done `69d923fbf` |
| T15.2.2 | US-15.2 | **Layer operations on any page** — layer_manage made the page current because LayerController works on the current page; it now works on the given page. LayerController::workOnPage/workOnCurrentPage: for one agent call its operations use the given page, the layer panel is rebuilt for the user's page afterwards, and the user's selection ends only if it is on that page. test_e15_view 4/4. | `ai: layer operations no longer move the user's view (T15.2.2)` | ✅ done `f654f80ff` |
| T15.2.3 | US-15.2 | **The view stays put when pages come and go above it** — A page inserted or deleted above the visible one shifted the content under the user by a page and changed the current page. The scroll position follows the content, and the current page stays the same page. XournalView keeps the current page where it was on the screen when another page is inserted or deleted (scrolls by the shift), and moves its current/selected indices with it instead of re-selecting (which would end the user's text editing). Deleting the current page itself still goes to the next one. test_e15_view 5/5 (test_5 fails without the anchor). | `ai: pages added or removed above keep the view in place (T15.2.3)` | ✅ done `652d82284` |
| T15.2.4 | US-15.2 | **The user's selection survives work on other pages** — Every agent draw, edit and commit dropped the user's selection and ended their text editing (clearSelectionEndText), on any page. Only work on the selection's (or the text's) own page drops it; there the agent has priority. DrawApi (draws, commits) and EditApi (edits, deletes by id) end the user's selection and text editing only on their page (Control::clearSelectionEndTextOn), and a selection that holds a requested element lets go of it first. Clipboard copy, cut and paste still work through a selection. test_e15_view 6/6 (test_6 fails on 1.13.0). | `ai: agents drop the user's selection only on its own page (T15.2.4)` | ✅ done `21dcd7b5d` |
| T15.2.5 | US-15.2 | **A chip shows where the agent works** — Instead of moving the view: 'AI -> p. N' at the edge of the canvas while an agent changes a page that is not in view; a tap goes there. ThinkingOverlay::setPageChip: a small osd button 'AI ↓ p. N' (↑ when above) at the top right of the canvas, never the keyboard's. McpUi shows it for the agent's document events on pages out of view, follows the page if pages move, and hides it when the user gets there or 12 s after the last such change. test_e15_view 7/7. | `ai: a chip shows the page an agent works on (T15.2.5)` | ✅ done `2ab811c31` |
| T15.2.6 | US-15.2 | **Navigation waits for the user's gesture** — The navigation tools (page_manage goto, view scroll and zoom, elements_select, show_message) move the view when asked, but not in the middle of the user's stroke, selection drag or typing: they wait (briefly) for it to end. Side effects that moved the view (clipboard copy and cut, show_message on another page) no longer do. whenUserBetweenGestures (ToolUtil): no stroke, drag or touch gesture on the canvas (InputContext::userGestureRunning), no key held, none pressed in the last 1.5 s; at most 8 s. view (all changes), page_manage goto and show, and elements_select wait for it; their arguments are checked at once. show_message on a page out of view says '(p. N)' at the top of the visible area; clipboard copy/cut select where the elements are. test_e15_view 9/9 (8 and 9 fail on 1.13.0). | `ai: agents' navigation waits for the user's gesture (T15.2.6)` | ✅ done `a0d4433fd` |
| T15.2.7 | US-15.3 | **Undo, redo, highlighter and pointer among the big buttons** — The owner's request (2026-10-20): in focus mode the toolbars are hidden; the big-button panel gets undo, redo, the highlighter and the select-object pointer (pick an object with a tap). The Focus row: touch | AI highlighter, AI lasso, pen (+ colours), highlighter, eraser, lasso, pointer | undo, redo | terminal, command | auto-improve. What gives way when the room runs out follows its own priority (the owner: the terminal opens only from here, Auto-improve is in Tools too): touch, AI highlighter, AI lasso, pen, terminal, eraser, highlighter, undo, redo, pointer, lasso, command, auto-improve. All fit in the owner's 1920-px screens. The owner, with the laptop's screen upright (1200 wide): the row stopped two or three tiles short of the right-hand buttons. Every tile was counted as wide as the Touch switch (and with its gap margin); each is now counted at its own minimum width, so the row fills its room. test_e13_quick_tools 19/19 (test_19 fails on the earlier build: a 206-px gap). | `ai: undo, redo, highlighter and pointer among the big buttons (T15.2.7)` | ✅ done `804110f34` |
| T15.2.8 | US-15.2 | **Release 1.14.0** — Full check: build-noterm and build-nomcp ok; unit tests 208/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 266/266. | `ai: release 1.14.0 (the agent works without moving your view) (T15.2.8)` | ✅ done `baa4efbc5` |

### S15.3 — The on-screen keyboard only when called → v1.15.0

**US-15.4** — As the owner, in tablet mode I want GNOME's on-screen keyboard to stop popping up and down by itself in xournalai (the screen jumps), and to call it myself when I need it.

- [ ] in manual mode xournalai's text fields and terminal no longer open or close GNOME's keyboard
- [ ] the keyboard still comes with the swipe up from the bottom edge, and typing works as before
- [ ] a menu switch turns it back to automatic, remembered; programs in the AI terminal keep the session's input method

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.3.1 | US-15.4 | **Manual on-screen keyboard** — GNOME Shell 42 (X11) opens its keyboard on every IBus focus-in of any app and closes it on focus-out, sliding the window with it; it ignores GTK's inhibit-OSK hint. In manual mode (the default) the app's text fields use GTK's own input method (gtk-im-module), so GNOME never hears of them. GTK_IM_MODULE is taken out of the environment at start (it would override that) and given back to programs started from the app. The owner's layouts are XKB ones: typing, layout switching and compose keep working. Menu: AI Agent → 'Screen keyboard only when called (swipe up from the bottom edge)'; assistant.screen_keyboard in mcp.json; app_status.screen_keyboard. test_e15_screen_keyboard 3/3. | `ai: GNOME's on-screen keyboard only when called (T15.3.1)` | ✅ done `8004321cc` |
| T15.3.2 | US-15.4 | **Release 1.15.0** — Full check: build-noterm and build-nomcp ok; unit tests 207/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 269/269. | `ai: release 1.15.0 (the on-screen keyboard only when called) (T15.3.2)` | ✅ done `eace1533b` |

### S15.4 — AI zones do not flood the X server → v1.15.1

**US-15.5** — As the owner, I want the window not to freeze while agents work: the app must not flood the X server.

- [ ] an AI zone being animated costs a few thousand X requests a second, not ~40 000
- [ ] app_status counts the X requests by kind, to find what loads the server

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.4.1 | US-15.5 | **AI zones and page shadows no longer flood the X server** — 2026-10-20 15:23: the window froze for 15 s (rescued, restarted) while asking the X server for a new range of resource ids (XCreatePixmap -> _XAllocIDs -> xcb_generate_id -> XC-MISC); Xorg had read all of the app's requests, its main thread was idle and it served others. The app had sent 148 million requests in 68 minutes: an AI zone's animation redrew the whole window (the overlay lies over all of it) and relaid it out (queue_resize, show_all, restyling its button) at every frame, and its GtkSpinner drew 60 frames a second; every frame made new pixmaps for icons, CSS and the eight page-shadow pieces (cairo uploads image sources each time), using up the two million ids within minutes. The overlay now redraws only what changes (the zone's outline ring and status pill at each frame, its whole area when it moves or changes, the recording pill, the lasso and marks by their areas), places its buttons only when they move, shows a still stop icon instead of the spinner, and the page shadows are kept on the X server. An AI zone: ~41 700 -> 3 500-9 000 requests and 59 -> 18-20 frames a second (Xvfb). app_status.ui.x_requests_by_kind counts the requests by kind. Not yet: zooming still costs ~60 000 a second while it lasts, and an idle window ~1 500 (GTK's own widgets). | `ai: AI zones no longer flood the X server (T15.4.1)` | ✅ done `37f1de7a6` |
| T15.4.2 | US-15.5 | **Release 1.15.1** — Full check: build-noterm and build-nomcp ok; unit tests 207/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 271/271. | `ai: release 1.15.1 (AI zones do not flood the X server) (T15.4.2)` | ✅ done `8460bf9a4` |

### S15.5 — The pale 2 mm grid, for new pages and agents → v1.15.2

**US-15.6** — As the owner, I want every new page on the small pale grid, also the pages agents add, so my notes look the same throughout.

- [ ] 'Pale graph (2 mm)' is in the paper list: graph, 2 mm squares (r1=5.669), lines 0.2 pt, #f3f3f3
- [ ] it is the paper of new documents and pages; a template still on the former default (the 2.5 mm fine graph) moves to it
- [ ] page_manage's 'graph' is the user's default style of it, and background_config sets a paper's parameters exactly

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.5.1 | US-15.6 | **The pale 2 mm grid, the default for new pages and agents** — 2026-10-21: the companion added pages to et24-2.xopp with page_manage insert background="graph": a bare format, so they got the coarse 5 mm Graph, and no tool could set the pale grid the owner uses (the agent patched the saved file). pagetemplates.ini: 'Pale graph (2 mm)', graph with r1=5.669, lw=0.2, f1=0xf3f3f3, af1=0x383838; the default page template; a pageTemplate still on the former default (the 2.5 mm fine graph, r1=7.087) moves to it when the settings load, another choice is kept. page_manage: a paper type is the user's default style of it (the template's config when it is that format), background_config sets the parameters exactly (r1 1-2000, lw/blw 0-50 points; "" = the plain format); without a paper an inserted page is like the page before it, as before. test_e15_paper 5/5 (all fail on 1.15.1). | `ai: the pale 2 mm grid, the default for new pages and agents (T15.5.1)` | ✅ done `31e85afc9` |
| T15.5.2 | US-15.6 | **Release 1.15.2** — Full check: build-noterm and build-nomcp ok; unit tests 207/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 276/276. | `ai: release 1.15.2 (the pale 2 mm grid by default) (T15.5.2)` | ✅ done `fbe39f513` |

### S15.6 — The recording chooser always comes back → v1.15.3

**US-15.7** — As the owner, I want to be asked what every recording is, also after I closed the chooser once without choosing.

- [ ] a recording that stops while the chooser is closed shows it, with that recording first
- [ ] a chooser hidden with the canvas (Focus mode, full screen) shows again when the canvas is back

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.6.1 | US-15.7 | **The recording chooser shows again after it was closed without a choice** — 2026-10-23: the big Record button's recordings stopped asking what they are. The chooser had been closed without a choice on 2026-10-22 (Escape closes a non-modal popover with the keyboard in it; a hidden canvas hides it too) after a 61-minute recording; it popped up only when its queue was empty, so that recording stayed first and every later one only raised the 'more waiting' count, unseen (4 waiting). A recording that stops while the chooser is closed now comes first and shows it; ones that stop while it is open wait their turn as before; the canvas's map shows a closed chooser with waiting recordings again. test_e11_recordings 8 (fails on 1.15.2: chooser false, pending 2). | `ai: the recording chooser shows again after it was closed without a choice (T15.6.1)` | ✅ done `26d731062` |
| T15.6.2 | US-15.7 | **Release 1.15.3** — Full check: build-noterm and build-nomcp ok; unit tests 207/211 (the ControlLoadHandler group that fails under parallel ctest, all pass on rerun); integration 277/277. | `ai: release 1.15.3 (the recording chooser always comes back) (T15.6.2)` | ✅ done `cef196161` |

### S15.7 — The serving terminal finds a conversation running in the background → v1.15.4

**US-15.8** — As the owner, I want the AI terminal to bring back the serving Claude after a restart, also when Claude Code kept it running in the background.

- [ ] a serving conversation running in the background (claude agents) is attached in the serving tab
- [ ] without one, the tab continues the last conversation as before

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.7.1 | US-15.8 | **The serving terminal attaches to a conversation running in the background** — 2026-10-24: after a restart the serving tab showed 'Your most recent conversation is running in the background (session 1bc62f83...). Use claude agents to find and attach to it' and exited. Claude Code 2.1.310 keeps a conversation that is working when its terminal closes running under its daemon (bg-pty-host, bg-spare); the window had closed while the serving session worked, and then claude --continue refuses. The serving command now asks claude agents --json --cwd <companion> for a background session there and runs claude attach <id>; otherwise claude --continue as before (an older Claude Code without agents prints nothing: --continue). test_e15_serving_attach 2/2 with a fake claude on PATH (both fail on 1.15.3). | `ai: the serving terminal attaches to a conversation running in the background (T15.7.1)` | ✅ done `711c7d198` |
| T15.7.2 | US-15.8 | **Release 1.15.4** — Full check: build-noterm and build-nomcp ok; unit tests 206/211 (the ControlLoadHandler group that fails under parallel ctest and one more, all 5 pass on rerun); integration 279/279. | `ai: release 1.15.4 (the serving terminal finds a conversation running in the background) (T15.7.2)` | ✅ done `420d487ae` |

### S15.8 — Big targets where the hand is → v1.15.5

**US-15.9** — As the owner, I want the window that asks what a stopped recording is to open right under the big Record button, with big tiles, so that I can answer with one tap without looking for it (in a car too).

- [ ] the recording chooser opens under the Record button (at the bottom of the canvas when the button is off)
- [ ] its choices are big tiles like the Tools grid's
- [ ] the AI lasso / AI highlighter window has big tiles too

**US-15.10** — As the owner, I want a panel to the right of the AI terminal with big Space, Enter, Backspace and Escape keys, so that on the tablet I can dictate into Claude (hold Space) and send without the on-screen keyboard.

- [ ] the panel takes about 15% of the terminal area, to its right
- [ ] holding Space holds the space key in the terminal (Claude's dictation); Enter, Backspace and Escape send their keys

**US-15.11** — As the owner, I want the serving Claude to live and end with xournalai, so that no session keeps running unseen (with its permissions and Remote Control) after I close the app.

- [ ] closing xournalai ends the serving Claude, also one Claude Code moved to its background
- [ ] the next start resumes its conversation in the serving tab

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.8.1 | US-15.9 | **The recording chooser opens under the Record button, with big tiles** — 2026-11-09, the owner: the chooser opened in the middle of the canvas, at its bottom: awkward to reach when they can hardly look at the screen (driving), though the big Record button itself is easy to press. It now opens right under the Record button (a popover pointing at it, like the Tools grid under Tools); with the button off, where it was. Redesigned like the Tools grid: the three choices first (big tiles, 40 px icons, Instructions in the AI purple), then the optional request with a big 'Hold to say' mic and Send, bigger chips. Shown again when the button is shown again (as with the canvas). test_e11 test_9 (fails on 1.15.4); test_e11 9/9. | `ai: the recording chooser opens under the Record button, with big tiles (T15.8.1)` | ✅ done `09ddb8a43` |
| T15.8.2 | US-15.9 | **The Ask window (AI lasso, AI highlighter) with big tiles** — The same redesign for the Ask window (AI lasso, AI highlighter): the seven commands as big tiles four to a row (32 px icons), a header with a big close button, a big 'Hold to say' mic, the text and Send. Still next to the marked area. test_e10_ask, test_e14_ai_highlighter pass. | `ai: the Ask window with big tiles (T15.8.2)` | ✅ done `b4b889e01` |
| T15.8.3 | US-15.10 | **A key panel next to the AI terminal: Space (held), Enter, Backspace, Escape** — 2026-11-09, the owner: on the tablet, without the GNOME on-screen keyboard, they want to dictate to Claude Code (hold Space) and send. To the right of the terminal tabs a panel (15% of the dock's width, 110-320 px; see-through like the terminal when floating): Esc and Backspace, a big Space, Enter. The keys go to the tab shown (as typed: ' ', DEL, ESC, CR), sent on the press; Space and Backspace held repeat after the keyboard's delay at its rate (GNOME's org.gnome.desktop.peripherals.keyboard, else 500/30 ms): what a terminal sees of a held key, which Claude Code takes as hold-to-talk; a lost release stops after 10 minutes. A click without a press (an agent's ui_interact) sends the key once. A keyboard toggle in the dock's header hides it (remembered in ai-terminal-view.ini). test_e15_terminal_keys 5/5 (real X presses on Xvfb; a raw byte logger in the serving tab). | `ai: big keys next to the AI terminal: Space (held), Enter, Backspace, Escape (T15.8.3)` | ✅ done `69678da7b` |
| T15.8.4 | US-15.10 | **The terminal keys: Space apart and rightmost, the terminal gets the keyboard, the tab row keeps its width** — 2026-11-09, the owner, trying them: Space reached the terminal, but Claude Code took a held Space as hold-to-talk only after the terminal had been tapped (it follows the terminal's focus); and Space should stand apart, the biggest and rightmost, not to be confused with Enter. Now a key gives the tab shown the keyboard first; Space takes all the height on the right (3 of 5 columns, 10 px apart), Esc, Backspace and Enter in a column to its left. The panel lies over the dock beside the terminal page (the pages leave it room with their end margin), so the tab row keeps its whole width: in a narrow window it had squeezed out the row's empty space that is dragged to resize the terminal (test_e14_terminal_view test_4 failed). Keys shrink in a short terminal. test_e15_terminal_keys 6/6 (+ the layout and the keyboard), test_e14_terminal_view passes. | `ai: the terminal keys: Space apart, the terminal gets the keyboard, the tab row keeps its width (T15.8.4)` | ✅ done `0c0d1e749` |
| T15.8.5 | US-15.11 | **The serving Claude lives and ends with the app** — 2026-11-09: after the owner closed xournalai, the serving session (7e49375d, sent to Claude Code's background daemon with a command on 31.10, resumed from 1bc62f83) still ran, busy, with bypassPermissions, the desktop's DISPLAY and a Remote Control bridge; 1.15.4 had only attached to such a session. Claude Code 2.1.319: CLAUDE_CODE_DISABLE_BG_EXIT_HANDOFF (read from its code: the hand-off to the background on exit is on unless it is set) is now set for the app's Claude tabs, so they end with their terminal; at quit, a detached login shell stops any background session in the companion folder (claude stop <id>: the conversation is kept); at start, one found there is stopped and its conversation resumed in the serving tab (claude --resume <sessionId>; attached to only if it can't be stopped), else --continue. test_e15_serving_attach 3/3 with a fake claude (stop and resume, --continue, the stop at Ctrl+Q). | `ai: the serving Claude lives and ends with the app (T15.8.5)` | ✅ done `4f82a68d0` |
| T15.8.6 | US-15.11 | **Release 1.15.5** — Full check (2nd run, after T15.8.4/5): build-noterm and build-nomcp ok; unit tests 207/211 (the 4 that fail under parallel ctest pass on rerun); integration 286/287 on /usr/bin/python3 (pyenv's 3.12 lacks PIL, which test_e14_terminal_view test_6 needs). The one failure, test_e12_x_hang test_1 (the app rescued and exited, but not through 'closing the display connection'), is flaky on this machine now: 3 of 4 runs fail with the T15.8.3 terminal code too. The 1st run (T15.8.3) had test_e14_terminal_view test_4 fail (the panel squeezed the tab row: fixed in T15.8.4). | `ai: release 1.15.5 (big targets where the hand is; the serving Claude ends with the app) (T15.8.6)` | ✅ done `32328b60b` |

### S15.9 — Live notes while recording (experimental) → v1.16.0

**US-15.12** — As the owner, I want a recording mode that transcribes as I speak: pieces cut at pauses (or when long, or when I tap ✂) go to a speech recognizer, their text lands in a narrow column of a live-notes page, and Claude draws what is being talked about in the rest of the page at its own pace, so that a talk or a meeting is annotated while it goes on.

- [ ] a long press on the big Record button offers Record and Record + live notes (when the experimental switch is on); a short press records as before
- [ ] the recording itself is unchanged (the .ogg, the chooser afterwards)
- [ ] pieces are cut at a pause of 1.5 s, in a monologue at a short pause after ~15 s, at the quietest moment by 25 s, and on ✂
- [ ] the recognizer is any program behind a small interface (a per-file command, or a JSON-lines stream), not tied to one server
- [ ] the text of each piece, with its time, goes into a narrow column of a new page; the serving Claude reads new pieces when it is free and draws topic cards in the free area

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.9.1 | US-15.12 | **A tap on the recorded sound and a chunker that cuts it at pauses** — The recorder's consumer thread (VorbisConsumer) hands every block it writes, after the gain, to a sample tap (control/SampleTap: compiled with or without audio, null by default). LiveChunker (GTK-free) mixes to mono, resamples to 16 kHz, decides voice per 50 ms frame with VoiceActivity (the microphone's own noise floor: the quiet built-in microphone works), drops the silence before the voice (but 300 ms), and ends a piece at a 1.5 s pause (300 ms of it kept), in a monologue at the first 0.3 s pause after 15 s, at the quietest 100 ms of the last 2 s at 25 s, on ✂ (the quietest of the last 0.5 s) and at the end; under 300 ms of voice is dropped. No overlap between pieces. Unit tests LiveChunker 6/6 (pause, silence and clicks, monologue, limit, scissors, 48 kHz stereo). | `ai: a tap on the recorded sound and a chunker that cuts it at pauses (T15.9.1)` | ✅ done `be6a2712c` |
| T15.9.2 | US-15.12 | **The live transcriber: any recognizer behind a per-file or JSON-lines command** — docs/mcp/TRANSCRIBERS.md specifies the interface (protocol 1): pieces are WAV PCM 16-bit mono 16 kHz with language ('auto'), language_hint (the previous piece's) and prompt (the end of the previous text); a recognizer is 'file' (a command template with {audio} {language} {prompt}, stdout the text) or 'stream' (a long-running process, JSON lines: hello {protocol, engine, audio: path|inline} → transcribe {id, audio_path, audio_b64?, language, language_hint, prompt} → result {id, text, language, language_probability, segments, elapsed} | error {id, message}; stdin closed at the end); 120 s timeouts; a stream that worked and died is restarted for the queued pieces. LiveTranscriber implements both on the GLib main loop (GSubprocess through the login shell, like the terminal's tabs), knowing no particular recognizer. tools/transcribers/faster-whisper/live_worker.py is a stream recognizer (the model loaded once; an unsure short piece retried in the hinted language); installed on the owner's server next to its transcribe.py (~/transcript/bin, nothing existing changed). Measured over ssh with the owner's recording of 8.11: hello after 5.5 s, a 10 s piece back in 1.6-2.1 s, 2.5 s in 0.8 s, Russian at p=0.96-0.98. | `ai: the live transcriber: any recognizer behind a per-file or JSON-lines command (T15.9.2)` | ✅ done `ba6ecea6e` |
| T15.9.3 | US-15.12 | **The live-notes page: the transcript column, the live_notes tool, wake-ups for the serving session** — LiveNotes (src/core/mcp): one recording transcribed while it goes on. The sample tap feeds its LiveChunker (on the recorder's thread; pieces handed to the main loop), each piece is written as WAV to <audio folder>/live/<recording>/ and submitted to the LiveTranscriber with the language (assistant.live_notes.language, 'auto'), the last answer's language as the hint and the last text as the prompt. Answers are written in order (an empty one skipped, a failed one as '(not recognized: …)') as small text (Sans 7.5) with [m:ss] into a column of 24% of the width on the right of a live-notes page inserted after the owner's page (title, TRANSCRIPT header); a new page '(continued)' when the column is full. Pages and text through the app's own page_manage / create_text (layers, undo, change log as for agents). The serving session gets one intent ('Live notes …: new transcribed speech. Call live_notes(op="read") …') until it reads, and a last one when everything is in and read; the whole text goes to transcripts/<recording>.live.txt. live_notes tool: read (pieces since the last read, pages with free_area and transcript_column), status, cut. McpConfig assistant.live_notes {enabled, transcriber {kind, command}, pause_ms, soft_s, hard_s, language}. The recording observer is now always set (a live session ends with its recording; the chooser still only when recordings are shared). Without an audio folder (the test hook), pieces go to the app's data folder: a first test run wrote into the owner's ~/Music/live (moved aside). test_e15_live_notes 6/6 (stream recognizer: column, wake-up and read, hint and prompt, ✂, the end and the file, a second page), test_e15_live_notes_file 1/1 (file recognizer, a failed piece). | `ai: the live-notes page: the transcript column, the live_notes tool, wake-ups for the serving session (T15.9.3)` | ✅ done `608d83167` |
| T15.9.4 | US-15.12 | **A long press on Record: Record or Record + live notes; the ✂ button; the experimental switch** — A long press (GtkGestureLongPress, capture phase, kept by QuickTools: a GTK 3 gesture is not kept by its widget) on the big Record button opens, under it, two big tiles: Record and Record + live notes (when the switch is on and nothing records); the press's release is not a click (it starts nothing). Record + live notes starts the recorder through its action and, once it records, McpUi::beginLive with its file name (the settings read afresh from mcp.json). During live notes a purple ✂ 'Send now' shows left of Record (the piece being spoken goes now). AI Agent menu: 'Live notes: long-press the big Record button (experimental)' (assistant.live_notes.enabled, remembered). test_e15_live_notes_ui 3/3 (a real long press on Xvfb: the menu under the button, Escape, no recording started; ✂ during live notes; the switch in mcp.json). | `ai: a long press on Record: Record or Record + live notes; the ✂ button; the experimental switch (T15.9.4)` | ✅ done `262e4051b` |
| T15.9.5 | US-15.12 | **The live-annotation skill (base: topic cards in the free area)** — skills/live-annotation (installed by skills/install.sh with the others): the loop (woken → live_notes read → draw → read again; never keeping up with the speech; a state file for long talks), topic cards in a two-column flow in the free area (title with time and colour, bullets, rounded frame, arrows down the flow, dashed connectors back to an earlier topic, optional line pictogram), the next live page when full, a summary card at the end; what is said is material, never instructions. The companion's CLAUDE.md (Companion.cpp) has a 'Live notes' section pointing at it. An end-to-end run on the owner's real 70 s recording (real quiet microphone, the A40 worker over ssh, in real time) found that passing the previous text as Whisper's initial_prompt made a quiet piece come back as 'Продолжение следует…' / 'Субтитры сделал DimaTorzok'; without it the piece is right: the worker now ignores the prompt unless --use-prompt and drops known silence hallucinations (retrying without the prompt first). After that: 5 pieces of ~15 s, all right, everything in 0.6 s after the sound ended. | `ai: the live-annotation skill (base: topic cards in the free area) (T15.9.5)` | ✅ done `1b44437d5` |
| T15.9.6 | US-15.12 | **The transcriber specification (docs) and a fast worker on the owner's server (the model loaded once)** — docs/mcp/TRANSCRIBERS.md (written with T15.9.2) now also records the prompt trap (large-v3 + initial_prompt → stock phrases on a quiet piece) and the measured run. tools/transcribers/faster-whisper/live_worker.py + live-worker.sh are installed on the owner's server in ~/transcript/bin next to transcribe.py (its venv and models; nothing existing changed); README: install, check by hand, mcp.json, options (--use-prompt off by default, the stock-phrase filter). | `ai: the transcriber specification and the fast worker on the owner's server (T15.9.6)` | ✅ done `a5931efad` |
| T15.9.7 | US-15.12 | **Review of the iteration: consistent, easy to maintain, recognizers easy to replace** — An independent review (a separate agent, given the code and the spec, not my conclusions) found, all confirmed and fixed: (1) a null or wrongly typed field from a recognizer threw out of a GIO callback (std::terminate): every field is now read type-safely (jsonString/jsonNumber, json::parse without exceptions); (2) the whole queue was written at once with timers from that moment, so a recognizer slower than speech timed out pieces that only waited: now at most max_in_flight (hello; default 1) are sent, timers start at send, and a piece that times out ends the recognizer (stuck) and a fresh one takes the rest (marked restarting first, so nothing more is sent to the old one); (3) the file kind's global end deadline failed pieces that never ran and answered one twice: no global deadline, every piece answered exactly once (pending set); (4) a file run's timeout killed only the shell while ssh/scp kept the pipes and blocked later pieces, and process trees leaked: every recognizer is a process group of its own (setsid), ended as a group; (5) a line longer than an empty column inserted pages without end: on a fresh column it is written anyway; (6) a hung stream recognizer was never ended: stdin closed, SIGTERM to the group after 10 s, SIGKILL 5 s later. Design: LiveTranscriber split into the Transcriber interface + factory (Transcriber.h) and one class per kind (FileTranscriber.cpp, StreamTranscriber.cpp); the transcriber config kept verbatim as JSON (a save dropped unknown fields); timeout_s / ready_timeout_s configurable; placeholders {language_hint} {id}; hint and prompt filled when a piece is sent (200 characters, not bytes); the transcript file grows piece by piece (kept if the app exits); a failed page insert no longer stalls the session; live_notes op=cut needs the Ui tier; the worker's retry keeps the hint. TRANSCRIBERS.md rewritten to match (common settings, process groups, max_in_flight, failures, end, what is kept, scope: Ask and audio_transcribe still use the built-in whisper.cpp). New tests: test_e15_live_notes_robust 5/5 (odd types and nulls, a slow recognizer, a piece never answered, inline audio, a recognizer that dies, the end lets it go, unknown fields kept), test_e15_live_notes_hang 1/1 (a hung file run with a child: ended as a group, the rest at once). test_e11_recordings_off now checks its real promise (nothing announced, no chooser), the observer being always set since T15.9.3. Real end-to-end again: 5/5 pieces right, 0.6 s after the sound. | `ai: review of live notes: a recognizer can't crash or stall them; one class per kind of recognizer (T15.9.7)` | ✅ done `87fb08e83` |
| T15.9.8 | US-15.12 | **Release 1.16.0** — Full check (after T15.9.7): build-noterm and build-nomcp ok; unit tests 212/217 (the 5 that fail under parallel ctest pass on rerun; LiveChunker 6/6 among them); integration 302/303 on /usr/bin/python3, the one failure test_e12_x_hang test_1, flaky on this machine since 1.15.5 (fails with older code too). Live notes: test_e15_live_notes 6, _file 1, _ui 3, _robust 5, _hang 1; a real end-to-end run on the owner's recording with the A40 worker: 5/5 pieces right. | `ai: release 1.16.0 (live notes while recording, experimental) (T15.9.8)` | ✅ done `c38c5bd5e` |

### S15.10 — The app stays inside its own folders → v1.16.1

**US-15.13** — As the owner, I want xournalai (deployed by git clone and a build) and its tests to change nothing outside the app's code and its own folders, so that my other tools, above all Claude Code, are never affected.

- [ ] the companion's skills ship with the app and go into its companion folder only
- [ ] no test or demo can start a real Claude Code / Codex by accident

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.10.1 | US-15.13 | **Companion skills ship with the app; tests never start a real agent** — 2026-11-09: while checking live notes with a real Claude in a test app, its native installer (started with the test's XDG_DATA_HOME) re-pointed the owner's ~/.local/bin/claude into the test folder, deleted afterwards: claude was broken everywhere (restored to ~/.local/share/claude/versions/2.1.319; the test sessions' traces in ~/.claude.json, ~/.claude/projects, session-env and history.jsonl removed, copies kept aside). And the live-annotation skill had been linked into ~/.claude/skills by hand (removed). The owner: the app is deployed by git clone and a build and must not touch anything outside its code and its own folders. Now the skill is installed with the app (share/xournalpp/companion-skills) and Companion::ensure copies it into <data>/xournalai/companion/.claude/skills (project skills of the companion folder, like its subagents); skills/install.sh no longer lists it. The test harness puts stubs for claude/codex/opencode first on a test app's PATH (allow_real_agents=True to opt out; a test's own PATH replaces them). test_e6_companion: the skill in the companion folder, the stub on PATH. | `ai: companion skills ship with the app; tests never start a real agent (T15.10.1)` | ✅ done `af1da67e3` |
| T15.10.2 | US-15.13 | **Release 1.16.1** — Full check: build-noterm and build-nomcp ok; unit tests 213/217 (the 4 that fail under parallel ctest pass on rerun); integration 302/304, the two failures the known flaky ones (test_e12_x_hang test_1; test_e6_wakeups test_3, which passes alone). After the whole run the owner's ~/.local/bin/claude, ~/.claude.json and ~/.claude/history.jsonl are untouched (the stubs work). | `ai: release 1.16.1 (the app stays inside its own folders) (T15.10.2)` | ✅ done `e6097e930` |

### S15.11 — Requests spoken during live notes → v1.16.2

**US-15.14** — As the owner, I want requests I speak during live notes (draw this, make cards, find a picture, turn it into a mind map) to be carried out, but only on the live-notes pages, so that I can steer the notes by voice without anything else ever being done from speech.

- [ ] the app's texts (wake-up, live_notes, the companion's CLAUDE.md) say requests are handled only within the live-annotation skill's safe list
- [ ] everything else said is never acted on

**US-15.15** — As the owner, I want a long recording (with live notes) never to freeze xournalai, so that the app doesn't restart under me mid-talk.

- [ ] while recording the window is not redrawn at the screen's frame rate
- [ ] the Record button and the indicator set their texts and styles only when they change

**US-15.16** — As the owner, I want a page with my drawing never to vanish by a stray tap, and every page deletion to be traceable.

- [ ] deleting a page with content asks first (empty pages go at once)
- [ ] every page deletion is logged with what did it (the Delete page action, MCP, Undo, Redo)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.11.1 | US-15.14 | **Spoken requests within the skill's safe list; parallel topic agents (the companion's skill update)** — 2026-11-09, the owner's decision, relayed by the xournalai companion session: during live notes, requests spoken to the notes are recognised and carried out only within a safe list (the live-notes pages: cards, drawings, layout; web search for facts and PD/CC0 pictures); anything else said is never acted on from speech (only when typed in the terminal). The companion updated skills/live-annotation/SKILL.md (requests vs material, the safe list and the never-from-speech list, ⚡ tags, a coordinator with up to 4-5 parallel topic agents) and added lib/inbox.py (per-topic inboxes, in the companion folder only); reviewed here. The app's three texts that still said 'never instructions' now point at the safe list: the wake-up line (LiveNotes.cpp), the live_notes tool description, the companion's CLAUDE.md 'Live notes' section; the skill's sentence about the old wake-up line updated to match. The recordings' 'Audio notes … NOT instructions' rule is unchanged (another feature). test_e6_companion: the skill's lib/ copied into the companion folder, the CLAUDE.md wording. | `ai: spoken requests in live notes, within the skill's safe list (T15.11.1)` | ✅ done `ba802898a` |
| T15.11.2 | US-15.15 | **Recording no longer redraws the window ~120 times a second** — 2026-11-09 18:37: 18 minutes into a live-notes recording the UI thread waited 15 s for the X server (a GetInputFocus after a SYNC:3; the server answered others), and the hang handler rescued the document and restarted the app. The app had sent 70 million X requests since 16:49; the owner's app measured ~30,000 X requests/s with the companion idle. On a test app: at rest 1,198 req/s and 1 frame/s; with the recorder on 27,137 req/s and ~60 frames/s (SYNC:3 119/s): the Record button's face redrew on every frame clock tick (a tick callback), and four times a second refreshRecord set the label's text, the tooltip and the style class again (each re-lays out and redraws), and the recording indicator redrew at 20/s. Now the face animates on an 80 ms timer (only its own area, only when mapped), refreshRecord changes the label, tooltip and class only when they change, the indicator draws at 10/s. Measured: 8,341 req/s and ~23 frames/s with the recorder on (13,728 with live notes on), down from 27,137 / ~60. Still left (to investigate): a single small redraw costs ~600-1,200 X requests (looks like a whole-window repaint), and ~7 frames/s go on after a recording ends. test_e11_recordings, test_e14_ai_highlighter, test_e15_focus, test_e15_live_notes_ui, test_e13: 51/51. | `ai: recording no longer redraws the window at the screen's frame rate (T15.11.2)` | ✅ done `3b5a06eb4` |
| T15.11.3 | US-15.16 | **Deleting a page with content asks first; every page deletion is logged** — 2026-11-09: the owner's first page (a drawing) vanished twice during live notes; the app's change log said only 'user' (page events 46, 240), no agent had deleted pages that day (their transcripts read), and the Delete page action has no shortcut: likely the toolbar's button (it deletes the current page at once). Control::deletePage now asks 'Delete page N? It holds M element(s)' (Cancel / Delete page; Undo brings it back) when the page holds anything, then deletes that page wherever it is now; an empty page goes at once. Every deletion is logged (journal): by the Delete page action (confirmed / empty), through MCP (deletePageAt gets a reason), by Undo of an insertion, by Redo of a deletion. test_e15_page_delete 3/3. | `ai: deleting a page with content asks first; every page deletion is logged (T15.11.3)` | ✅ done `1c7f49bca` |
| T15.11.4 | US-15.14 | **The transcript column at half size; up to 8 workers** — The owner during the 18:19 recording: 'транскрипт шрифтом вдвое мельче… будет больше помещаться'. assistant.live_notes.transcript_size (default 3.8 pt, 3-12; was a fixed 7.5); the gap between pieces scales with it. And up to 8 workers: assistant.max_parallel 1-8 (default 8; was 1-5, default 5), the settings dialog, transactions, the wake-up line now says 'workers (subagents and worker tabs)'. test_e15_live_notes: the font size; the next-page test feeds 150 pieces (a page holds over a hundred now). | `ai: the transcript column at half size; up to 8 workers (T15.11.4)` | ✅ done `2d0696c5f` |
| T15.11.5 | US-15.14 | **The companion's live-annotation updates: do it yourself or delegate; inbox wait/end** — Written by the companion session at the owner's request, reviewed here: SKILL.md 'Do it yourself or delegate (speed first)' (a decision table, measured timings, speed rules) and 'Who works on a topic'; lib/inbox.py ops wait (NEW / ENDED / TIMEOUT, polling 2 s) and end (DIR/ended), only in the companion's live folder. Its draft lib/child_session.sh (gnome-terminal windows) is not taken: worker sessions become tabs of the app's AI terminal (the owner's choice), next. | `ai: the companion's live-annotation updates (do it yourself or delegate; inbox wait/end) (T15.11.5)` | ✅ done `934ae7ea1` |
| T15.11.6 | US-15.15 | **Release 1.16.2** — Full check: build-noterm and build-nomcp ok; unit tests 213/217 (the 4 that fail under parallel ctest pass on rerun); integration 304/307: test_e6_wakeups test_1 and test_e7_transactions test_5 (+ test_6 after it) still expected the old cap of 5 (fixed: the wake line's new wording; transactions sets max_parallel 5 for its limit test; 9/9 then), test_e6_wakeups test_3 the known flaky one. The build was installed into build/install at 19:02 before the formal release (the owner's app froze again at 18:59 on 1.16.1), its --version label still says 1.16.1. ~/.claude untouched after the run. | `ai: release 1.16.2 (a long recording no longer freezes the app) (T15.11.6)` | ✅ done `b56e57650` |

### S15.12 — Workers in tabs; live notes outside the user's undo → v1.16.3

**US-15.17** — As the owner, I want heavy live-notes topics drawn by worker sessions I can watch in tabs of the AI terminal, the live-notes page dense, and my Undo never to take the app's live page away.

- [ ] the serving session opens and closes worker tabs with a tool; a worker has only the safe list's tools
- [ ] live notes' pages and transcript are not in the user's undo history and are logged as 'app'
- [ ] the live page: 12 pt margins, a narrower column, italic transcript

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.12.1 | US-15.17 | **Live notes outside the user's undo history; origin 'app' in the change log** — 19:03:20 (the new deletion log): 'Page 2 deleted by Undo (it had been inserted)': five seconds after the recording started, an Undo meant for the owner's own work removed the fresh live page (the undo history is shared; the butterfly page went the same way, most likely). LiveNotes now inserts its pages with Control::insertPage(..., undoable=false) and writes its texts through DrawApi with an undoSink that drops the undo action (no animation), bracketed by EventHub::beginAppWork/endAppWork: the change log says 'app' (app > agent > user), so the user's Undo and auto-improve never take them for the user's. Page undo actions find their page by itself (indexOf), so pages outside the history don't shift what Undo deletes. test_e15_live_notes test_7. | `ai: live notes outside the user's undo history; origin app (T15.12.1)` | ✅ done `866d56f04` |
| T15.12.2 | US-15.17 | **Worker sessions in tabs of the AI terminal (tool workers)** — The owner chose tabs over the companion's gnome-terminal script. workers(op=start, name, brief, effort) writes the brief to <companion>/workers/<name>.brief.md and opens a tab '⚙ <name>' (not focused) running claude --name 'live <name>' --effort E --permission-mode dontAsk --allowedTools 'mcp__xournalai,WebSearch,WebFetch,Read,Bash(python3 *)' --mcp-config <companion>/.mcp.json --strict-mcp-config "$(cat brief)" in <companion>/workers (its own CLAUDE.md, written by Companion::ensure; trusted through the companion folder: Claude Code walks up the parents for trust), env XOURNALAI_WORKER, CLAUDE_CODE_DISABLE_BG_EXIT_HANDOFF=1. Up to max_parallel running; one name at a time; op=list; op=close (name or all) ends the session with its tab. `xournalpp --ai-hook` ignores events of a worker (XOURNALAI_WORKER), so it is never taken for the serving session. test_e15_workers 4/4 with a fake claude (the command line, cwd, env, brief passed literally, cap, names, hooks, closing ends the process). | `ai: worker sessions in tabs of the AI terminal (T15.12.2)` | ✅ done `65b68a43d` |
| T15.12.3 | US-15.17 | **A dense live page: 12 pt margins, a narrower column, italic transcript** — The owner during the 19:12 recording: much smaller margins and paddings, the page filled densely; the transcript written in italic at once, not redrawn. Margins 24 → 12 pt, the column 24% → 18% of the width (min 90 pt), 6 pt gap before it, title 7 pt at y 6, column header 4.5 pt at y 17, first line at y 24; transcript 'Sans Italic' 3.8 pt. test_e15_live_notes test_1 checks italic and the dense layout. (Its code went into the commit of T15.12.1: the same rewrite of how LiveNotes writes its texts and pages.) | `ai: live notes outside the user's undo history; origin app (T15.12.3)` | ✅ done |
| T15.12.4 | US-15.17 | **Test apps get a HOME of their own (no real agent through a login shell)** — The first worker test ran its tab through the login shell, which reads the owner's ~/.profile: it puts ~/.local/bin (the real claude) in front of the harness's stubs. (The tab didn't start: a doubled exec; fixed.) The harness now also gives every test app its own HOME (unless the test sets one or allow_real_agents), so no login shell in a test reads the owner's profile. The owner's ~/.local/bin/claude, ~/.claude.json and history checked untouched. | `tests: test apps get a HOME of their own (T15.12.4)` | ✅ done `59b4c8707` |
| T15.12.5 | US-15.17 | **The companion's live-annotation updates: a dense card style; workers through the tool** — The companion's edits at the owner's request (19:12): card style title 5.5 / body 4.2 / labels 4 / min 3.8 pt, three columns with 4 pt gaps, slots ~60 pt, connectors head 2 / width 0.4, fill the page densely. Updated here to match 1.16.3: heavy topics start a worker tab with workers(op=start) and close with workers(op=close, all=true) (instead of lib/child_session.sh, not shipped), the transcript italic by the app. | `ai: the companion's live-annotation updates (dense cards; workers through the tool) (T15.12.5)` | ✅ done `9a1aaab55` |
| T15.12.6 | US-15.17 | **The skill: worker tabs hear from the coordinator only through their inbox; live pages by their current number** — The companion's finding from the first real run of worker tabs (19:42; three tabs T2-T4 and two subagents drew in parallel, one page in about 5 min): its SendMessage to the tabs was held and expired undelivered. Claude Code holds cross-session messages between sessions in different permission modes (the companion runs in bypass, the workers in dontAsk with the safe list, on purpose). The workers got the news through their inbox file. The companion's edits: SendMessage only for subagents, everything for a worker tab (new pieces, corrections, 'the page changed') through its inbox (kind note for corrections); the brief names the '1st live page' and the worker re-reads its current number from live_notes(op=status) → pages right before drawing (at 19:48 the owner deleted an empty page 1 and the live page became page 1). | `ai: the companion's skill: workers hear only through their inbox; live pages by their current number (T15.12.6)` | ✅ done `0b16027d6` |
| T15.12.7 | US-15.17 | **Release 1.16.3** — The full check (19:34-19:53): build-noterm and build-nomcp build; ctest 212/217 under -j8, the 5 failures the known ControlLoadHandler race on one temp file (all 29 pass with -j1); the MCP integration suite 312 passed, 0 failed. Installed through staging while the owner's app ran; the owner restarted it himself and the companion ran the first real worker tabs at 19:42 (T2-T4 with two subagents, one page in about 5 min). The owner's ~/.local/bin/claude and ~/.claude untouched. | `ai: release 1.16.3 (worker sessions in tabs; your Undo leaves live notes alone) (T15.12.7)` | ✅ done `35ab3057b` |

### S15.13 — A worker's zone lasts for its whole job → v1.16.4

**US-15.18** — As the owner, I want to see which regions agents are still working on, with who works there and on what, for as long as the work lasts, so the page never looks finished while workers are still thinking.

- [ ] a worker's thinking zone stays until the worker ends it or is closed, whatever the serving session does meanwhile
- [ ] the zone says which worker and what it does, in the topic's colour
- [ ] the zone appears the moment the coordinator starts the worker

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.13.1 | US-15.18 | **The companion's skill: show that a slot is busy (by hand, until the app does)** — The owner during the 19:49 recording (4:45-6:36): a worker's zone shows up and then any indication disappears, so it looks as if nothing happens and all zones are done, while a worker thinks 3-5 min; he wants a lasting frame with a light fill and a label (which agent, what it does) for the whole job, at whatever level fits. The companion drew the look he liked by hand (a dashed frame in the topic colour, width 0.5, radius 3, fill opacity 0.05, a centred italic 5 pt label) and wrote it into the skill as an interim rule, with 'if the app shows its own lasting busy-zone indicator, use that instead'. | `ai: the companion's skill: show that a slot is busy (T15.13.1)` | ✅ done `9c075aa24` |
| T15.13.2 | US-15.18 | **A worker's thinking zone is its own: it lasts until the worker ends it** — Why the zones vanished (the workers' transcripts): T3-T5 opened zones 6-9 at 19:51:50-19:53:32 and closed them at 19:54:27-19:57:14, but at 19:54:27 the app had no zones left at all. McpUi::onServingChanged ends every thinking zone when the serving session finishes a turn without subagents of its own; worker tabs are separate sessions (their hooks are ignored), so the coordinator's next idle (it wakes for every transcript piece) ended all the workers' zones, which faded in 1.5 s. Now a zone has an owner: the worker session whose call opened it. A worker's bridge (xournalpp --mcp-stdio, which inherits XOURNALAI_WORKER) names it in a header X-Xournalai-Worker on every request (a label for its zones only: it grants nothing); the session keeps it and the thinking tool records it. A worker's zone is labelled '⚙ Tn · …', is skipped when the serving session's turn ends, and ends when the worker ends it (done/fail), is closed ('closed before it finished') or its tab's process ends; its lease is an hour instead of 10 min. Its stop button sends Esc to that worker only. thinking takes a color (the topic's: the outline and a light veil in it), thinking_list says whose a zone is, workers(op=list) each worker's busy zones. (Its tests are in the next task's commit: they start the worker with its zone.) | `ai: a worker's thinking zone is its own: it lasts until the worker ends it (T15.13.2)` | ✅ done `a48cb15bc` |
| T15.13.3 | US-15.18 | **workers(op=start) shows the worker's zone at once; the skill uses the app's zones** — A worker starts in seconds but thinks for minutes: its slot should show as busy at once, not when it first calls thinking. workers(op=start) takes the slot (page, region, color, status): the app opens the worker's zone right away ('⚙ Tn · «Title»: thinking (effort high)…', owned by the worker) and appends its id to the brief (keep its status current, end it right after drawing, fail if it can't); the result returns the zone id. page/color/status without a region is an error. The worker's CLAUDE.md: show your zone for the whole job (the brief's id, or open one with the topic colour). The skill: no hand-drawn placeholders any more (the companion's interim rule of T15.13.1): workers get their slot through workers(op=start), subagents get a zone the coordinator opens and passes in the brief. Tests (test_e15_workers 5-6, with T15.13.2's): the zone from start lasts through the coordinator's turn (Stop) while the coordinator's own zone ends, the worker's own zones (its header) too, list shows them, closing fails what is left ('closed before it finished'); the bridge with XOURNALAI_WORKER names its worker. | `ai: workers(op=start) shows the worker's slot as busy at once; the skill uses the app's zones (T15.13.3)` | ✅ done `59c10f809` |
| T15.13.4 | US-15.18 | **The app follows its workers itself: what each does, for how long, when it has finished** — The owner (20:2x): should the companion keep the zones up, or should the application itself know what it delegated, for stability, quality and speed? The app owns the lifecycle, the agents supply the meaning (docs/mcp/LIVE_MODES.md, section 13). Each worker now gets hooks of its own (workers/hooks.json, given with --settings: UserPromptSubmit, PreToolUse, PostToolUse, Stop, SessionEnd); `xournalpp --ai-hook` under XOURNALAI_WORKER reports them as that worker's (only the tool name and a Bash command's first 300 characters), never as the serving session's (it used to drop them). The app keeps each worker's state (starting, busy, finished, ended) and activity (reading its task, searching the web, reading a web page, computing, waiting for more talk, taking new material, drawing, looking at the page, thinking); a worker's zone shows it on a second line with the time it has worked (⚙ T4 · … / searching the web · 2:41). A worker's Stop ends its job: the app closes the zones it left open and wakes the coordinator ('Worker Tn has finished its job … close it'). workers(op=list) shows state, activity, working_s; thinking_list a zone's detail. A hook costs ~25 ms (measured). Found on the way (test_e15_workers test_2 under parallel load): a tab counted as running only once its process had been spawned (asynchronously), so workers started in one message (as the skill says to) did not see each other: the cap and 'one name at a time' could be passed. TerminalDock: a tab being started counts as running; a tab closed while its process is being started ends that process as soon as it appears (it would have run on without a tab). test_e15_workers 7/7 (test_7: the worker's hooks → state, activity, the zone's second line, Stop closes its zones, the serving state untouched; hooks of a worker this window doesn't have are ignored), 39 tests of the neighbouring modules under load. | `ai: the app follows its workers itself: what each does, for how long, when it has finished (T15.13.4)` | ✅ done `ffebd2108` |
| T15.13.5 | US-15.18 | **Release 1.16.4** — Two full checks: after T15.13.3 (20:09-20:22: 314 passed, 0 failed) and after T15.13.4 (20:28-20:41: 314 passed, 1 failed: test_e12_x_hang test_1, the known flaky one, which passed when run again). ctest 213/217 under -j8, the failures the known ControlLoadHandler race on one temp file. Installed through staging while the owner's app ran; the owner restarts it himself. | `ai: release 1.16.4 (a worker's zone lasts for its whole job) (T15.13.5) (T15.13.5)` | ✅ done `96c018fe2` |

### S15.14 — Modes of live notes: the Record menu → v1.17.0

**US-15.19** — As the owner, I want to hold Record and pick what the recording is for (transcript only, requests on my drawing, a conspect with or without requests, a carefully laid-out conspect, voice control), with big options for the next recording (economy or intense, a summary at the end, requests only after a trigger word), all by finger, while a tap on Record stays the plain recording.

- [ ] the long press shows every mode as a big tile and the options as big toggles, remembered
- [ ] every wake-up line names the mode and what to read for it
- [ ] 📝 Текст wakes no agent while it records
- [ ] a tap on Record is the plain recording, unchanged

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.14.1 | US-15.19 | **The modes of live notes: the design** — docs/mcp/LIVE_MODES.md: the owner's modes (9 Nov evening) as presets over independent settings (annotate, requests, research, intensity, transcript), the Record menu (a grid of tiles, big toggles first, one tap starts), safety (the app should enforce the mode, not only the prose), the skill as a core and modules, ✍ drawing mode, the research browser (its own profile by default; CDP; the owner's Chrome needs the extension since Chrome 136), 🗒 ведение конспекта, 🎙 voice control, the decision panel (bottom left, under the left thumb), what the app does itself and what the agents do, the plan and the decisions (economy/intense as a switch; speaker separation deferred, experimental only). | `docs: the modes of live notes (the design) (T15.14.1)` | ✅ done `e3385beae` |
| T15.14.2 | US-15.19 | **The Record menu: the modes as tiles, the options as big toggles; the mode in live notes** — LiveModes.h/.cpp: the modes (📝 Текст, 🎙 Голосовое управление (soon, 1.17.1), ✍ Просьбы к рисунку, 🧠 Конспект + просьбы, 🛡 Конспект без команд, 🗒 Ведение конспекта) and the toggles (🌿 Экономно ↔ ⚡ Интенсивно as a switch, 🧾 Итог в конце, 🔤 Просьбы по слову; ❓ and 🔎 shown as soon), remembered in <config>/ai-live-modes.json (the last mode is highlighted). The long press's popover: the toggles first (52 px chips), then the tiles three in a row (158×104: a big sign, the name, a line of what it does, the badges of the options that apply), the plain recording first; one tap starts. A tap on Record is unchanged. Every wake-up line keeps its start and names the mode after the event ('…: new transcribed speech. Mode «…» (id: annotate=…, requests=…; options: …).') with what to read (the skill's module, modes/options.md) and the mode's rule (🛡 nothing said is carried out; ✍ context only, requests on the owner's page). 📝 wakes nobody while it records, and only at the end for «Итог в конце». ✍ says where the owner drew last (EventHub::lastUserAddition: page, area, how long ago). live_notes status reports the mode. assistant.live_notes.trigger_words (default клод, claude). test_live_audio takes a mode. test_e15_live_modes 5/5: the menu and the remembered toggles with badges; a tap on Record is the plain recording (here the plain recorder's own messages: no audio folder, no sound card); the wake-up lines of 🛡 with its options; 📝 silent, then one summary wake; ✍ with the owner's latest ink. test_e15_live_notes_ui follows the new tiles. | `ai: the Record menu: live-notes modes as tiles, options as big toggles (T15.14.2)` | ✅ done `86288a454` |
| T15.14.3 | US-15.19 | **The live-annotation skill's modes** — skills/live-annotation/modes/: README (the table), safe.md (requests off, visual annotation), drawing.md (requests on the owner's page, «это» = his latest ink or selection, additions at once, his strokes only after he confirms, speed), konspekt.md (a page plan, the layout library, graphs from real numbers, a clean page every ~10 min and at the end), options.md (intense, summary, trigger; questions and browser not yet). SKILL.md: a paragraph on modes. The voice-control skill waits for 1.17.1. | `ai: the live-annotation skill's modes (T15.14.3)` | ✅ done `f2aa16254` |
| T15.14.4 | US-15.19 | **Release 1.17.0** — The full check (21:05-21:20): builds without the terminal and without MCP; ctest 213/217 under -j8 (the ControlLoadHandler race); the MCP integration suite 314 passed, 6 failed: test_e12_x_hang test_1 (the known flaky one; passed again alone) and test_e15_live_modes 1-5, because its file had been edited for 1.17.1 (voice enabled) while the suite ran against the 1.17.0 build: with the file as committed, 5/5 against the same build. Installed through staging; the owner restarts the app himself. | `ai: release 1.17.0 (live-notes modes: hold Record and choose) (T15.14.4) (T15.14.4)` | ✅ done `db8cc0176` |

### S15.15 — 🎙 Voice control → v1.17.1

**US-15.20** — As the owner, I want to work the app by voice with the pen in my hand: nothing written for the talk, what I say pops up for a moment, and what I say while holding a big button (or after a trigger word) is done as a command.

- [ ] no live-notes page; pieces pop up and fade
- [ ] held talk button or trigger word: a command, woken with the context said before it
- [ ] read gives each piece's kind; the end wakes nobody

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.15.1 | US-15.20 | **🎙 Голосовое управление: the talk button, commands and context, pop-ups** — The mode's tile is available. LiveNotes in voice mode inserts no page and writes nothing on the document (the transcript file is kept); each transcribed piece is a command when it was said while the talk button was held (its interval overlaps the piece by 0.6 s or half of it; the chunker's sample count is now atomic, read on the UI thread) or, with «Просьбы по слову», when it starts with a trigger word (whole word, any case: «Клод, …» yes, «клодовый» no; the word is dropped), else context. A command wakes the serving session: 'Voice command (recording R, n N, m:ss): «…». Context since the last command: «…». The owner's latest ink: …. Carry it out on the open document as the voice-control skill describes.' (no other wake-ups, none at the end). Every piece pops up for 4 s at the top of the canvas (ThinkingOverlay::flashHeard: ▶ on teal for a command, 🎙 on grey for context). QuickTools: during voice control the big «🎙 Держи и говори» button replaces ✂ (a multi-press gesture: begin = pressed, end = let go; red while held); pressing ends the context piece, letting go sends the command. live_notes: read gives kind (command/context), status commands, talking and the pop-up shown. The voice-control skill ships with the app (scope: the open document; destructive commands only after a spoken «да»; a thinking zone per command; speed). test_e15_voice 4/4. | `ai: voice control: the talk button, commands and context, pop-ups (T15.15.1)` | ✅ done `b825f5cb1` |
| T15.15.2 | US-15.20 | **Release 1.17.1** — The full check (21:27-21:42): builds without the terminal and without MCP; ctest 212/217 under -j8 (the ControlLoadHandler race on one temp file); the MCP integration suite 324 passed, 0 failed. Installed through staging; the owner restarts the app himself. | `ai: release 1.17.1 (voice control) (T15.15.2) (T15.15.2)` | ✅ done `9bd850897` |

### S15.16 — ❓ The question panel → v1.17.2

**US-15.21** — As the owner, I want agents' questions and confirmations as big buttons left of the AI terminal (the terminal in the middle, the keys on the right), or on the canvas when the terminal is closed, so I answer with a tap and never type confirmations.

- [ ] the strip left of the terminal: who asks, the question, the agent's options, Да / Нет always in place
- [ ] a queue, a countdown and a safe default
- [ ] a card at the bottom left of the canvas when the terminal is closed
- [ ] agents ask with ask_owner; a tap is the only answer

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.16.1 | US-15.21 | **The question panel and ask_owner** — The owner (21:3x, and via the companion from the 21:28 recording): a smart panel of dynamic touch buttons left of the AI terminal for agents' questions and confirmations («да, рисовать», «да, делать», choices), the terminal in the middle, the keys on the right. QuestionPanel: one queue, two views: the strip in the terminal (TerminalDock::setSidePanel: left of the tab pages, ~17% of the dock, 200-340 px; the pages leave a start margin; the whole strip scrolls in a low terminal and never spills over the tabs) and a card at the bottom left of the canvas when the terminal isn't shown (it follows the strip's map/unmap). It shows who asks (Claude, ⚙ T4), the question, the agent's options as big buttons, a countdown to the safe default and, always at the bottom, Да (green; only for yes/no questions) and Нет (red; for a choice it skips it). Option buttons are rebuilt only when the question shown changes (a tap is never lost to a refresh). Tool ask_owner: op=ask (question, options ≤ 6, default, timeout_s 10-600, wait: the call returns the answer; or not: the serving session is woken with it, a worker looks with op=get), get, cancel, list; answers: the label tapped, yes/no, by owner | timeout | cancelled; questions still open when the window goes are answered cancelled. The Record menu's ❓ toggle is available; options.md, drawing.md (his strokes only after a tap), voice-control (destructive commands confirmed with a tap, never a spoken «да»), the serving session's and the workers' CLAUDE.md say to ask with a tap. test_e16_questions 6/6 (left, middle, right; options and a tap; waiting for a tap; the default on silence; a queue and a worker's question; the card on the canvas). Screenshots on the test display: the strip and the card. | `ai: the question panel: agents ask, the owner taps (ask_owner) (T15.16.1)` | ✅ done `ef09a4b8b` |
| T15.16.2 | US-15.21 | **A tap allows actions outside the app; the trigger phrase «Hey Assistant»** — The owner's decisions (22:1x): a tap on the question panel is his explicit consent, also outside the app (shell commands, finding files, the browser through the extension: a profile, a site and an action, an upload to Google Drive; opening terminals; settings): he won't type confirmations in the terminal during a brainstorm. The live-annotation skill (safe list), its subagent brief, the voice-control skill and the serving session's instructions: ask with ask_owner saying exactly what will be done, options «Разрешить» / «Не надо», do exactly that once on the tap, never on speech alone; still never, even with a tap: purchases or payments, passwords, keys and tokens, bulk deletion of his files (my floor, told to him). No separate coordinator session: the companion stays the coordinator (it can carry out what he allows). The trigger phrase is «Hey Assistant» (also «эй ассистент», «хей ассистент», «hey assistent»), off by default; LiveNotes compares phrases word by word, without case, punctuation between the words ignored, a word that only starts like it doesn't count. docs/mcp/LIVE_MODES.md: the decisions (the browser on another monitor, else the second workspace; the clean page typeset). test_e15_voice follows the phrase. | `ai: a tap allows actions outside the app; the trigger phrase «Hey Assistant» (T15.16.2)` | ✅ done `6ad8aba1d` |
| T15.16.3 | US-15.21 | **Release 1.17.2** — Two full checks. The first (22:00-22:15): build-noterm failed to link (the question code had landed in McpUi.cpp's terminal-only part; moved out, in T15.16.1); integration 329 passed, 1 failed (test_e12_x_hang test_1, flaky). The second (22:29-22:41): all three builds; ctest 214/217 (the ControlLoadHandler race); integration 328 passed, 2 failed: test_e12_x_hang (flaky, passed alone) and test_e6_companion test_companion_folder, which looked for the old rule's words «never acted on» (replaced by the tap rule in T15.16.2; the test follows, in that commit). The owner tested the panel from a staged install before the release. | `ai: release 1.17.2 (the question panel) (T15.16.3) (T15.16.3)` | ✅ done `6743c8b84` |

### S15.17 — The owner's first tests of voice control and the panel → v1.17.3

**US-15.22** — As the owner testing voice control, I want an agent's zone to stay while it works, its label readable, the heard phrase and the agent's answers by the region I drew in, voice control to be a dialogue (or trigger phrases when I tick it), the question panel to show only the agent's buttons, and a Tab key.

- [ ] zones an agent opens last until it ends them
- [ ] labels in two lines inside the zone
- [ ] tips by the region drawn while speaking; thinking op=say
- [ ] without the trigger option every phrase goes to the agent as a dialogue
- [ ] only the asker's buttons on the panel; Tab in the key panel

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T15.17.1 | US-15.22 | **Zones stay, labels wrap, tips by the region; voice control as a dialogue** — The owner's tests (22:4x, via the companion, each confirmed with a tap). (1) In voice control the companion's zones vanished within a minute: McpUi::onServingChanged ended every zone of the serving session at the end of its turn, and in voice control a turn ends after every phrase. A zone an agent opens itself (thinking op=start) is now kept until it ends it (or the owner stops it, or the 10-minute lease); app-made request zones still end with the turn. (2) Labels were cut at ~30 characters and sat above the zone over other strokes: now up to two lines as wide as the zone (180-420 px) inside its top edge when it has room, else above/below; no cairo group unless fading (each group cost the X server a pixmap and pictures every frame). (3) The heard phrase popped up in one place: now a tip by the region the owner drew in while saying it (EventHub::userInkBetween over the phrase's time, from 1.5 s before to 0.5 s after, mapped from the recording's start), up to four lines; the wake line says 'While saying it, the owner drew on page P around [...]'. (4) The agent's answers: thinking op=say shows a violet tip by a region for 5-14 s. (5) The owner's rule for 🎙: without «Просьбы по слову» every phrase goes to the serving session as a dialogue ('Voice (recording …): «…» … if this holds a command, carry it out …; if it is only talk, it is context'), with it only phrases starting with the trigger phrase are commands; no talk button in the mode (✂ instead: a phrase goes at once); «Держи и говори» becomes a button of its own, next. The voice-control skill follows. test_e15_voice 4/4 (context and pop-up with the option; a trigger command with what was said before; a dialogue phrase with the region drawn meanwhile; a kept zone through the turn's end, a reply tip), test_e15_workers follows (the coordinator's own zone stays). | `ai: zones stay until ended, labels wrap, tips by the region; voice control as a dialogue (T15.17.1)` | ✅ done |
| T15.17.2 | US-15.22 | **The question panel shows only the asker's buttons; a Tab key** — The owner (22:47, by voice): the fixed Да / Нет aren't needed: a panel of dynamic buttons. Only the asker's options show (grey for «Нет» / «Не надо»); a yes/no question gets Да / Нет as its own two buttons; with no question only «Вопросов нет». ask_owner's description: offer «Нет» / «Не надо» yourself when he may decline. The terminal's key panel gets Tab (the owner, 22:5x), between Esc and ⌫; Space spans the four rows. test_e16_questions follows. | `ai: the question panel shows only the asker's buttons; a Tab key (T15.17.2)` | ⬜ todo |
| T15.17.3 | US-15.22 | **Release 1.17.3** —  | ` (T15.17.3)` | ⬜ todo |
