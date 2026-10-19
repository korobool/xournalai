# xournalai MCP — implementation roadmap

> Generated from `docs/mcp/tasks.json` by `tools/monitor/track.py roadmap`. Do not edit by hand.
> Design: [PLAN.md](PLAN.md). Live board: `tools/monitor/run.sh` → http://127.0.0.1:8765

## Versioning & commits

- The fork has its own semantic version, XOURNALAI_VERSION, kept in CMakeLists.txt. The application version string becomes <upstream>+ai.<XOURNALAI_VERSION>, e.g. 1.3.7+ai.0.2.0.
- Each task is exactly one commit. The commit message is '<planned message> (<task id>)'.
- The last task of a stage bumps the PATCH version (0.E.s), in the same commit.
- The last task of an epoch is a release task: MINOR bump (0.E+1.0, or 1.0.0 for the final epoch), CHANGELOG entry and a local git tag ai-v<version>.

Current version: **1.12.6**

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
| T14.12.1 | US-14.12 | **export writes pdf and xopp off the UI thread** — 2026-10-19 08:33-09:45 (1.12.6): with png and svg off the UI thread, the remaining freezes were PDF exports of the magazine, 2.1-3.8 s each, while the companion exported issue PDFs (and file_open, 1-2 s, which replaces the document anyway). pdf and xopp are now written on the render thread (runOffUi). The exporters read the live document, so it is held for reading during a PDF export (an edit then waits for it; before, everything waited); xopp prepares under the lock and writes without it. test_e9 test_5 exports pdf and xopp too (1.12.6: 1565 and 981 ms stalls). | `ai: export writes pdf and xopp off the UI thread (T14.12.1)` | 🔨 doing |
| T14.12.2 | US-14.12 | **Release 1.12.7** —  | `ai: release 1.12.7 (PDF exports do not freeze the pen) (T14.12.2)` | ⬜ todo |
