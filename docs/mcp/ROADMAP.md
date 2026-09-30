# xournalai MCP — implementation roadmap

> Generated from `docs/mcp/tasks.json` by `tools/monitor/track.py roadmap`. Do not edit by hand.
> Design: [PLAN.md](PLAN.md). Live board: `tools/monitor/run.sh` → http://127.0.0.1:8765

## Versioning & commits

- The fork has its own semantic version, XOURNALAI_VERSION, kept in CMakeLists.txt. The application version string becomes <upstream>+ai.<XOURNALAI_VERSION>, e.g. 1.3.7+ai.0.2.0.
- Each task is exactly one commit. The commit message is '<planned message> (<task id>)'.
- The last task of a stage bumps the PATCH version (0.E.s), in the same commit.
- The last task of an epoch is a release task: MINOR bump (0.E+1.0, or 1.0.0 for the final epoch), CHANGELOG entry and a local git tag ai-v<version>.

Current version: **1.7.1**

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

## E11 — Recordings for the AI (release 1.7.1)

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
| T11.2.3 | US-11.2 | **Release 1.7.1** — Changelog, version, full tests. | `ai: release 1.7.1 (T11.2.3)` | ✅ done |
