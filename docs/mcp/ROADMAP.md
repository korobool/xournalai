# xournalai MCP — implementation roadmap

> Generated from `docs/mcp/tasks.json` by `tools/monitor/track.py roadmap`. Do not edit by hand.
> Design: [PLAN.md](PLAN.md). Live board: `tools/monitor/run.sh` → http://127.0.0.1:8765

## Versioning & commits

- The fork has its own semantic version, XOURNALAI_VERSION, kept in CMakeLists.txt. The application version string becomes <upstream>+ai.<XOURNALAI_VERSION>, e.g. 1.3.7+ai.0.2.0.
- Each task is exactly one commit. The commit message is '<planned message> (<task id>)'.
- The last task of a stage bumps the PATCH version (0.E.s), in the same commit.
- The last task of an epoch is a release task: MINOR bump (0.E+1.0, or 1.0.0 for the final epoch), CHANGELOG entry and a local git tag ai-v<version>.

Current version: **0.1.0**

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
| T1.1.2 | US-1.1 | **DocumentApi snapshots** — api/DocumentApi: document, page, layer and element snapshots; RDP simplification; JSON mapping. | `mcp: add DocumentApi with element snapshots and simplification (T1.1.2)` | ✅ done |
| T1.1.3 | US-1.1 | **page_elements & pdf_text** — Tools with filters (page, layer, region, detail, tolerance) and pagination. Bump 0.1.1. | `mcp: add page_elements and pdf_text tools (T1.1.3)` | ⬜ todo |

### S1.2 — Rendering & layout understanding → v0.1.2

**US-1.2** — As an agent, I want renders of pages, regions and content blocks plus a layout analysis, so that I can read handwriting and interpret diagrams.

- [ ] page_render returns PNG image content and a file path, with dpi or max size, grid, highlight and layer options
- [ ] layout_analyze returns blocks with kinds, reading order and connectors
- [ ] blocks_render returns tight high-dpi crops
- [ ] shapes_recognize uses the built-in ShapeRecognizer
- [ ] guide and the prompts give summarize, extract_text and explain_figure recipes

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T1.2.1 | US-1.2 | **RenderApi & page_render** — Cairo image surface via DocumentView, PNG encoding, grid overlay, element highlighting, pixel/point scale metadata. | `mcp: add RenderApi and page_render tool (T1.2.1)` | ⬜ todo |
| T1.2.2 | US-1.2 | **layout_analyze** — Stroke clustering, block classification, reading order and connector detection; unit tests. | `mcp: add layout analysis of pages into content blocks (T1.2.2)` | ⬜ todo |
| T1.2.3 | US-1.2 | **blocks_render & shapes_recognize** — Crops per block; ShapeRecognizer applied to given strokes. | `mcp: add blocks_render and shapes_recognize tools (T1.2.3)` | ⬜ todo |
| T1.2.4 | US-1.2 | **guide tool & prompts** — Recipes and conventions (coordinates, pressure, anchors) as the guide tool and as MCP prompts. Bump 0.1.2. | `mcp: add guide tool and understanding prompts (T1.2.4)` | ⬜ todo |

### S1.3 — Files & export → v0.2.0

**US-1.3** — As an agent, I want to open, create, save and close documents and export content in standard and lossless formats, so that I can work with my notes end to end.

- [ ] file_open supports open, annotate_pdf, image_as_page and template modes, with an on_unsaved policy
- [ ] export supports pdf, png, svg, xopp and xjson, for a document, page range, region, layers or selection, delivered as a path or inline
- [ ] xjson is lossless (widths, styles, text, images, LaTeX source)

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T1.3.1 | US-1.3 | **file_* tools** — file_open, file_new, file_save, file_save_as, file_close, file_recent, file_info. | `mcp: add file management tools (T1.3.1)` | ⬜ todo |
| T1.3.2 | US-1.3 | **xjson format** — Serializer and parser for the lossless element interchange format; unit tests. | `mcp: add xjson lossless element interchange format (T1.3.2)` | ⬜ todo |
| T1.3.3 | US-1.3 | **export tool** — pdf, png, svg, xopp and xjson; scopes; path or inline delivery. | `mcp: add export tool (T1.3.3)` | ⬜ todo |
| T1.3.4 | US-1.3 | **E1 integration & release 0.2.0** — Integration scenario for epoch 1, CHANGELOG, tag. | `mcp: release 0.2.0 (T1.3.4)` | ⬜ todo |

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
| T2.1.1 | US-2.1 | **PressureModel** — Hardware-equivalent mapping, profiles, speed-awareness, resampling, jitter; unit tests. | `mcp: add stylus-like pressure model (T2.1.1)` | ⬜ todo |
| T2.1.2 | US-2.2 | **DrawApi core** — Target layer resolution (AI layer), attribution, grouped undo, animated insertion. | `mcp: add DrawApi core with AI layer, attribution and undo (T2.1.2)` | ⬜ todo |
| T2.1.3 | US-2.2 | **create_strokes & create_shapes** — Geometry generators for line, arrow, rectangle, ellipse, polygon, bezier, arc and coordinate system. | `mcp: add create_strokes and create_shapes tools (T2.1.3)` | ⬜ todo |
| T2.1.4 | US-2.2 | **create_text/latex/image/link** — Text elements, LaTeX via the existing pipeline, image elements, links. | `mcp: add text, LaTeX, image and link creation tools (T2.1.4)` | ⬜ todo |
| T2.1.5 | US-2.2 | **create_from_svg** — nanosvg flattening into strokes, <text> pre-pass, fit modes, warnings. | `mcp: add SVG to editable strokes conversion (T2.1.5)` | ⬜ todo |
| T2.1.6 | US-2.2 | **Placement helpers** — find_free_space, relative anchors, page new or current. Bump 0.2.1. | `mcp: add placement helpers and anchors (T2.1.6)` | ⬜ todo |

### S2.2 — Pen engine → v0.2.2

**US-2.3** — As the owner, I want the agent to draw through the same pen pipeline as my stylus, so that its strokes, erasing and selecting behave exactly like mine and I can watch it draw.

- [ ] pen_draw replays trajectories with pressure and timing through the input handlers
- [ ] pen_draw works with pen, highlighter, eraser, lasso/rect selection and the shape tools
- [ ] my tool state is restored afterwards
- [ ] replay waits while I'm mid-stroke

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T2.2.1 | US-2.3 | **SyntheticPen** — Synthetic pen-class InputEvents fed to the input handlers; page/view coordinate mapping; tool save and restore; collision queue. | `mcp: add synthetic pen input engine (T2.2.1)` | ⬜ todo |
| T2.2.2 | US-2.3 | **pen_draw tool** — Tool selection, speed modes, results with created, erased and selected ids. Bump 0.2.2. | `mcp: add pen_draw tool (T2.2.2)` | ⬜ todo |

### S2.3 — Drafts, editing & import → v0.3.0

**US-2.4** — As an agent, I want drafts, editing tools and import, so that I can iterate on a drawing and bring external content in.

- [ ] draft begin, render, commit and discard work
- [ ] elements can be selected, moved, scaled, rotated, restyled, reordered and deleted, with undo, redo and history
- [ ] import supports svg, image, xjson, xopp pages and pdf pages
- [ ] the xjson round-trip is lossless

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T2.3.1 | US-2.4 | **draft tool** — Draft layer lifecycle, render for self-critique, commit as one undo step. | `mcp: add draft workflow tool (T2.3.1)` | ⬜ todo |
| T2.3.2 | US-2.4 | **Editing & history tools** — elements_select, elements_edit, elements_delete, undo, redo, history. | `mcp: add element editing and history tools (T2.3.2)` | ⬜ todo |
| T2.3.3 | US-2.4 | **import tool** — svg, image, xjson, xopp pages, pdf pages; path or inline sources; placement. | `mcp: add import tool (T2.3.3)` | ⬜ todo |
| T2.3.4 | US-2.4 | **E2 integration & release 0.3.0** — Text-to-drawing scenario, xjson round-trip check, CHANGELOG, tag. | `mcp: release 0.3.0 (T2.3.4)` | ⬜ todo |

## E3 — Application control (release 0.4.0)

The agent can control everything in the app, semantically and through the real UI.

### S3.1 — Semantic control → v0.3.1

**US-3.1** — As an agent, I want to run any app action and manage pages, layers, tools, the view and the clipboard, so that I can operate the app reliably.

- [ ] actions_list and action_run cover all actions
- [ ] page_manage, layer_manage, tool_get, tool_set, view and clipboard work

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.1.1 | US-3.1 | **actions_list & action_run** — Via ActionDatabase, with enabled flag, state and parameters. | `mcp: add actions_list and action_run tools (T3.1.1)` | ⬜ todo |
| T3.1.2 | US-3.1 | **page_manage & layer_manage** — Insert, delete, move, background, size and goto; layer add, rename, visibility, select, merge, delete and copy. | `mcp: add page and layer management tools (T3.1.2)` | ⬜ todo |
| T3.1.3 | US-3.1 | **tool_*, view, clipboard** — Tool state get and set, zoom, scroll, layout, fullscreen, presentation, clipboard. Bump 0.3.1. | `mcp: add tool, view and clipboard control tools (T3.1.3)` | ⬜ todo |

### S3.2 — UI automation → v0.3.2

**US-3.2** — As the owner, I want the agent to operate the real interface — open menus, navigate them, operate dialogs — so that everything I can do, the agent can do.

- [ ] ui_menu_tree and a visible ui_menu_select work
- [ ] ui_windows, ui_inspect and ui_screenshot work
- [ ] ui_interact, ui_keys, ui_file_chooser and ui_wait_for_window work

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.2.1 | US-3.2 | **Widget registry & inspection** — Stable widget ids, ui_windows, ui_inspect, ui_screenshot. | `mcp: add UI inspection tools (T3.2.1)` | ⬜ todo |
| T3.2.2 | US-3.2 | **Menu automation** — ui_menu_tree from GMenuModel, ui_menu_select with visible navigation. | `mcp: add menu tree and visible menu navigation (T3.2.2)` | ⬜ todo |
| T3.2.3 | US-3.2 | **Widget interaction** — ui_interact, ui_keys, ui_file_chooser, ui_wait_for_window. Bump 0.3.2. | `mcp: add widget interaction, keys and file chooser automation (T3.2.3)` | ⬜ todo |

### S3.3 — Safety & release → v0.4.0

**US-3.3** — As the owner, I want permission tiers and automatic backups, so that an agent can't destroy my work.

- [ ] calls outside the granted tiers return an explanatory error
- [ ] a backup is taken before destructive or bulk operations

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T3.3.1 | US-3.3 | **Permission tiers & backups** — read, draw, ui and files tiers; backup snapshots before risky operations. | `mcp: add permission tiers and safety backups (T3.3.1)` | ⬜ todo |
| T3.3.2 | US-3.3 | **E3 integration & release 0.4.0** — Menu and dialog scenario, CHANGELOG, tag. | `mcp: release 0.4.0 (T3.3.2)` | ⬜ todo |

## E4 — Co-creation (release 0.5.0)

I draw, the agent watches, reacts and adds to my work in real time.

### S4.1 — Events → v0.4.1

**US-4.1** — As an agent, I want to know what the user changes and when they pause, so that I can react at the right moment.

- [ ] changes_get returns an ordered event log
- [ ] wait_for_user returns after the user draws and pauses, with a render of the region
- [ ] resource subscriptions push updates to clients that support them

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T4.1.1 | US-4.1 | **EventHub & changes_get** — Hooks in undo/redo and the document listener, in-progress stroke state, cursor-based log. | `mcp: add EventHub and changes_get tool (T4.1.1)` | ⬜ todo |
| T4.1.2 | US-4.1 | **wait_for_user** — Paused HTTP responses resumed on idle or timeout; region filter. | `mcp: add wait_for_user tool (T4.1.2)` | ⬜ todo |
| T4.1.3 | US-4.1 | **Resources & push** — xournal:// resources, subscribe, SSE notifications. Bump 0.4.1. | `mcp: add resources with subscriptions and SSE notifications (T4.1.3)` | ⬜ todo |

### S4.2 — Presence & in-app UI → v0.5.0

**US-4.2** — As the owner, I want to see when and where the agent acts, stop it at once and configure it, so that co-creation feels safe and natural.

- [ ] view get returns the visible area, pen position and selection
- [ ] show_message shows a callout on the canvas
- [ ] the match_user profile works
- [ ] a status indicator, a Stop agent action and highlights exist
- [ ] MCP settings are editable in the app

| Task | Story | Title | Commit | Status |
|---|---|---|---|---|
| T4.2.1 | US-4.2 | **Presence tools** — view get (visible area, pen position, selection), show_message callouts. | `mcp: add presence tools (T4.2.1)` | ⬜ todo |
| T4.2.2 | US-4.2 | **match_user profile** — Derive the pressure and width profile from the user's recent strokes. | `mcp: add match_user pressure profile (T4.2.2)` | ⬜ todo |
| T4.2.3 | US-4.2 | **Status, Stop & highlight** — Status indicator, Stop agent action and shortcut, highlight of agent additions, AI layer accept and clear. | `mcp: add agent status indicator, stop action and highlights (T4.2.3)` | ⬜ todo |
| T4.2.4 | US-4.2 | **Settings page** — MCP page in Settings: enable, port, token copy, tiers, defaults. | `mcp: add MCP settings page (T4.2.4)` | ⬜ todo |
| T4.2.5 | US-4.2 | **E4 integration & release 0.5.0** — Co-creation scenario, CHANGELOG, tag. | `mcp: release 0.5.0 (T4.2.5)` | ⬜ todo |

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
| T5.1.1 | US-5.1 | **Notes memory** — notes tool storing transcripts, summaries and meanings in the document. | `mcp: add persistent notes memory (T5.1.1)` | ⬜ todo |
| T5.1.2 | US-5.1 | **Performance** — Render cache, payload limits, pagination review. | `mcp: improve performance of rendering and large documents (T5.1.2)` | ⬜ todo |
| T5.1.3 | US-5.1 | **Reference docs & coverage** — TOOLS.md generated from the registry; COVERAGE.md mapping actions, menus and dialogs. | `mcp: add tool reference and coverage documentation (T5.1.3)` | ⬜ todo |
| T5.1.4 | US-5.1 | **Release 1.0.0** — Client matrix results, CHANGELOG, tag. | `mcp: release 1.0.0 (T5.1.4)` | ⬜ todo |
