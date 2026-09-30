# xournalai changelog

The fork has its own version (`XOURNALAI_VERSION` in `CMakeLists.txt`). The application reports it as
`<upstream version>+ai.<fork version>`, e.g. `1.3.7+ai.0.1.0`. The embedded MCP server reports the fork version.
See [ROADMAP.md](ROADMAP.md) for the versioning rules.

## Unreleased

## 1.4.0 — Deep zoom (epoch E8)
- **Deep zoom toggle** on the toolbar, next to Zoom in (magnifier icon; the `ZOOM_DEEP` toolbar item, action
  `zoom-deep`): raises the maximum zoom from 700% to 3000%, remembered in the settings. Turning it off brings a deeper
  zoom back to 700%. The zoom slider, the zoom action and the MCP `view` tool follow the current range.
- **Rendering only what is visible** when a whole page no longer fits one buffer (about 32 megapixels): the visible
  part plus a margin is rendered, and scrolling beyond it renders the new part. PDF backgrounds render straight into
  that part instead of a whole-page cache. Memory stays flat at any zoom (about 140 MB at 3000% in the tests,
  where a whole A4 page would need gigabytes).

## 1.3.1 — Pinch owns the zoom (epoch E8)
- **Touchscreen pinch zoom follows the fingers 1:1.** Gesture daemons such as Touchégg (default config on
  Pop!_OS) turn the same 2-finger pinch into repeated Ctrl+KP_Add / Ctrl+KP_Subtract keystrokes. Each 10% step
  ended the pinch's zoom sequence, after which the pinch factor multiplied the already changed zoom, so the zoom
  raced to 700% or 30%. Now, while two fingers are down (and 0.4 s after), one-step zooms (keys, Ctrl+wheel, the
  zoom action) are ignored, and a pinch whose sequence was ended elsewhere re-anchors instead of compounding.
- **Touch/zoom trace** for diagnosis on real hardware: create `~/.cache/xournalai/trace-touch` (or set
  `XOURNALAI_TRACE_TOUCH=<file>`) to log touch events, zoom steps, redraw times and scrolling to
  `~/.cache/xournalai/touch-trace.log`.
- **Test hook** `test_touch` (only with `XOURNALAI_TEST_HOOKS=1`): synthetic touchscreen events through GTK's normal
  event path; `test_e8_pinch.py` replays a pinch with Touchégg-style keystrokes in the middle.

## 1.3.0 — Parallel serving session (epoch E7)
The serving session coordinates up to five background subagents: they think in parallel, and each one's edits
land as one ordered transaction.
- **Edit transactions** (`transaction_begin` / `transaction_commit` / `transaction_abort` / `transaction_list`):
  claim an area, prepare new content in a private draft, then commit an ordered list of operations (draw the draft
  instantly or like a stylus, delete, restyle, move) played in order as ONE undo step ("AI: …"). Commits are
  serialized, so edits never interleave; they may complete in a different order than they began.
- **Conflict check** at commit: if strokes a transaction depends on (its base and every stroke it deletes or edits)
  changed since it began, or the user drew over them, the commit is refused with the reason; the draft is kept for a
  corrected commit. Restyles count as changes (the change log now fingerprints colour, width, fill and points).
- **Hard limits:** at most `max_parallel` (1-5, settings) open transactions, no two overlapping claims, a 10-minute
  lease; Stop aborts every open transaction and finishes a playback at once.
- **Subagents** in the companion folder: `canvas-quick` (fast model: formulas, handwriting, colours, `*!`, `*w!`)
  and `canvas-artist` (strong model: `**!`, `*r!`, sketches, diagrams, Revise); the serving session delegates in the
  background and ends its turn, so it stays responsive.
- **Visible:** every transaction has a thinking zone (the request's zone travels in the wake-up line as
  `[zone N]`); zones close with their transaction, not when the coordinator goes idle; the status strip counts
  working subagents.

## 1.2.0 — Assistant Milestone 1: the serving session in the app (epoch E6)
Your canvas workflow inside xournalai, with no extra windows.
- **AI terminal dock** (VTE, Ctrl+`): collapsible, tabs for Claude Code, Codex, OpenCode or a shell; processes keep
  running while hidden. Build option `ENABLE_AI_TERMINAL`.
- **The serving session** starts with the app in the dock: `claude --dangerously-skip-permissions` (resumes its last
  conversation) in the companion folder `~/.local/share/xournalai/companion` (CLAUDE.md/AGENTS.md with its role,
  your markers, the assist contract and delegation; `.mcp.json` bound to this app; hooks). Settings: autostart,
  agent (Claude/Codex), permission mode.
- **The app watches, the session works:** Claude Code hooks (`xournalpp --ai-hook`) tell the app whether the session
  is idle, busy or waiting; canvas events are coalesced and typed into the idle session as one `[xournalai] …` line;
  a watchdog resends once, then reports "not responding". Never while paused or while you type in the terminal.
- **AI toolbar:** *! Improve, **! Illustrate, *w! Web, *r! Image, *c! Command…, Revise page, Auto-improve (with its
  rules), Stop, Pause, Terminal. Actions apply to the selection, else the last thing you drew, else the page, and are
  recorded as `intent` events for any agent.
- **Auto-improve toggle** (Ctrl+Alt+I, remembered): on = everything you write wakes the session (rules: formulas,
  text, diagrams, colours); off = markers and commands only.
- **Handwritten markers** (`*!`, `**!`, `*w!`, `*c!`, `*r!`) are detected in the app, geometrically and without a
  model, and handed to the session with their stroke ids.
- **Visible thinking:** a grey veil with an animated outline and a status line over the area being worked on, a
  spinner to stop it, a tick when done; the `thinking` tool lets the session mark its own work. Stop interrupts the
  session (Esc) and drops queued requests.
- **AI drawings always go to the layer set in the settings** (an agent's own choice only if you allow it).
- Fixed along the way: the overlay must not catch input (a windowed drawing area swallowed every click and stroke).
- Tests: 134 integration scenarios (a fake agent in the dock stands in for Claude), unit tests for markers and the
  companion folder.

### 1.1.0 — Always available
- The stdio bridge follows the app: while xournalai is not running it answers the handshake itself (no tools,
  nothing launched), polls, and sends `list_changed` notifications when the app starts or quits. Configure an agent
  once and it works whenever you run xournalai. A tool call while the app is closed starts it.
- The bridge remembers the tool lists (`~/.cache/xournalpp/mcp-bridge-lists.json`), so agents always see the tools;
  after the user closes the app, calls say "xournalai is not open, ask the user to open it" instead of reopening
  it. Only the first call of a session starts an app that was never running.
- Fixed: `create_from_svg(profile="match_user")` failed ("Unknown argument profile_options"); it now uses the user's
  pressure and keeps the SVG's own colors and widths.
- Pausing is unmistakable and hard to trigger by accident: when paused the status strip turns amber with bold
  "AI PAUSED - agents are blocked; your pen still works" and the button reads "Resume AI"; the button sits on the
  left next to the status (no longer under the zoom slider) and takes no keyboard focus. Agents are told since when
  the app is paused and where to resume (`app_status.paused_since`); pause/resume are logged.
- The HTTP server adopts session ids it does not know (app restarted, settings applied) instead of answering 404,
  so connected agents simply continue.
- xournalai has its own app id (`io.github.korobool.xournalai`), launcher entry and icon; `tools/register-desktop.sh`.
- Fixed: a normal quit crashed (see 1.0.1 notes); the AI Agent menu is built before the first request is served.
- README describes the features.
- Fixed: a xournalai window started while another one held the port ran without an MCP server for good. It now
  retries every 2 s and takes over the port when the other window closes; the status strip says it is waiting.
- The app is called xournalai everywhere it names itself: window title, About dialog, messages, `--version`
  ("xournalai 1.1.0 (based on Xournal++ 1.3.7)"), preferences, print jobs, launcher, what agents are told
  (`serverInfo.title`, instructions, guide). `app_status` reports `based_on` instead of `xournalpp_version`.
  Xournal++ is named only as the original project and for the `.xopp` file format.

### 1.0.1 — Draw into the current layer by default
- `default_layer` now defaults to `"current"`: agent drawings land in the layer you have selected, so the eraser
  and selection work on them immediately. `"AI"` (a separate layer) stays available in the settings or per call
  (`layer="AI"`). Existing `mcp.json` files keep their value.
- The AI Agent menu's layer actions act on the `"AI"` layer whenever the default is `"current"` (they found no
  layer before).
- Fixed: quitting the application crashed while removing the AI Agent menu (the main window is destroyed before
  the MCP server); the UI now tracks its widgets with weak pointers. A normal-quit scenario was added — the
  other tests stop the app with a signal and never ran this path.

## 1.0.0 — Polish (epoch E5)
The complete feature set: agents read, draw with stylus-like pressure, create content directly, control the whole
application, co-create with the user and remember what they learned.
- Reference documentation generated from the running server: [TOOLS.md](TOOLS.md) (56 tools with their tiers,
  arguments and defaults, prompts, resources) and [COVERAGE.md](COVERAGE.md) (all 133 actions, the main menu,
  dialogs, and every Lua plugin API function mapped to its MCP counterpart). `tools/mcpdoc/gen_docs.py`. Tools report
  their permission tier in `_meta["xournalai/tier"]`.
- Client matrix ([CLIENTS.md](CLIENTS.md), `tools/mcpdoc/client_matrix.py`): Gemini CLI and OpenCode connect and
  list the tools; the official MCP Python SDK passes a full protocol run over HTTP and stdio; Claude Code and Codex
  accept the configuration (their connection needs user approval or a session).
- Upstream fix: SIGTERM/SIGINT made the app save and `exit()` inside the signal handler, which could deadlock and
  leave a hung process; the GUI now handles them in the main loop (emergency save, exit in ~0.1 s).
- Fixed: applying the settings while a tool call was in flight (e.g. `wait_for_user`) crashed when the call later
  answered through the restarted server; `current_page` could point past the last page right after pages were
  removed.
- Tests: 108/108 integration scenarios, 176/176 unit tests; the MCP-off build is verified. The harness reaps every
  process it starts and keeps helper processes on the Xvfb display.

### 0.5.1 — Notes and performance
- `notes` tool: remembers transcripts, summaries and meanings for the document, a page or a region; stored next to
  the file (`<file>.ai-notes.json`), follows page moves, kept in memory until an untitled document is saved.
- Safety backups are captured synchronously but written in the background (deleting a page of a 72k-stroke
  document: 3.8 s → 76 ms); files appear only when complete; the app waits for pending backups on exit.
- `page_elements` stops at ~4 MB of JSON and continues with `next_offset`; HTTP requests over 64 MB get 413.
- Measured on 120 pages × 600 strokes: status/info/elements/layout/changes calls take 0–45 ms, `page_render`
  ~70 ms (no render cache: it would add staleness risks for little gain).
- Upstream fix: the render thread read the page layout while the UI thread rebuilt it (page insert/move/delete),
  crashing in `Layout::getPixelCoordinatesOfEntry`; repaint positions are now computed on the UI thread.

## 0.5.0 — Co-creation (epoch E4)
The agent works alongside the user: it notices what they draw, answers in their hand on its own layer, and the
user stays in charge.
- In-app controls (only when built with MCP): an **AI Agent** menu and a status strip at the bottom of the window
  (off / listening / N agent sessions / working: <tool>).
- **Pause** (menu, strip button or Ctrl+Alt+Esc): every tool except `app_status` is refused with a clear message;
  a running pen stroke lifts the pen and restores the user's tool, animations finish at once. `app_status` reports
  `paused_by_user`.
- AI layer actions: accept (merge down), show/hide, clear (undoable). A small "AI" marker briefly shows where the
  agent drew.
- **AI Agent Settings** dialog: enable, port, token (hidden, regenerate), permission tiers, default layer,
  animation, backups; applied immediately (the server restarts, the change log is kept).
- Agents cannot pause/resume themselves, open or operate the settings dialog (menu, actions, keys and widget
  automation all refuse).
- Guide topic `cocreate`. 100/100 integration scenarios, 176/176 unit tests; the MCP-off build is verified.
  Acceptance scenario: user sketches, agent waits, answers with the pen on the AI layer,
  talks on the canvas, is paused by the user, and the user accepts the layer.
- Upstream fix: `Menubar::menu` was read uninitialised before the menubar was populated.
- `view` reports the user's pointer (page coordinates), selection and active tool; `show_message` shows a
  non-modal callout on the canvas.
- `user_style` learns the user's pen from their strokes (width, pressure base/min, tapers, variation, a pressure
  curve); `match_user` profile for `create_strokes` and `pen_draw`.

### 0.4.1 — Events
- `EventHub`: every undoable change is diffed per page (added, removed or changed elements, with ids and area),
  plus page inserts/deletes and document replacements. Each change is attributed to the user or the agent; only
  document-changing tool calls count as agent work.
- `changes_get` (cursor-based log, origin filter, user_drawing flag) and `wait_for_user` (returns after the user
  changed something and paused; changes, area, image, current page; region/page filters; timeout).
- Resources `xournal://document`, `xournal://changes`, `xournal://page/{page}`, `xournal://page/{page}/image`,
  with subscriptions pushed as `notifications/resources/updated` over SSE, also through the stdio bridge.
- Test harness: real X mouse input for "user" strokes (xdotool, isolated to the Xvfb display).

## 0.4.0 — Application control (epoch E3)
Agents control the whole application, semantically and through the real interface.
- Permission tiers: the destructive tier also guards UI clicks such as Discard / Don't save / Quit.
- Safety backups (`mcp.json` `backups`, `backup_dir`): a copy of the document is saved before discarding changes,
  deleting pages or layers and deleting 20+ elements; the newest 30 are kept.
- Guide topics `control` and `ui`.
- Fixed: discarding changes on an untitled document failed (empty path added to recent files).
- Acceptance (the E3 milestone): the Go-to-Page dialog operated through its widgets; File/Open through the menu
  handling the unsaved-changes prompt and the file chooser; File/Save As through the file chooser; menus visibly
  navigated; keyboard shortcuts. 83/83 integration scenarios, 175/175 unit tests; the MCP-off build is also verified.

### 0.3.2 — UI automation
- `ui_windows`, `ui_inspect` (widget tree with stable ids, role, label, glade name, value, enabled, bbox; filter),
  `ui_screenshot`.
- `ui_menu_tree` (the whole main menu with actions, shortcuts, enabled and checked state) and `ui_menu_select`
  (by path or unique suffix; visible mode really opens each menu level before activating).
- `ui_interact` (click, set_value for entry/spin/scale/check/toggle/switch/combo/color/font/tabs/text,
  select_row, activate, focus, close), `ui_keys` (application shortcuts run their action; other keys are sent as key
  events; type text), `ui_file_chooser` (sets the path, waits for the selection, clicks the dialog's confirm button),
  `ui_wait_for_window`.
- Integration tests run the installed build (`build/install`), so resources are present and there are no startup
  error dialogs.

### 0.3.1 — Semantic application control
- `actions_list` / `action_run`: every application action (the win.* and app.* GActions behind menus, toolbar and
  shortcuts), with menu labels, enabled flag, parameter type and state; parameters and states are converted from
  JSON (or GVariant text). Quitting needs the `destructive` permission.
- `page_manage` (insert with background, delete, duplicate, move, background type/color, size, goto) and
  `layer_manage` (add, rename, show/hide, select, delete, copy, merge_down, move_up/down), both using the app's own
  undoable operations.
- `tool_get` / `tool_set` (tool, color, size, drawing type, line style, fill, eraser type), `view` (zoom, fit,
  100%, scroll to page/region, fullscreen, presentation, sidebar; visible region in page points), `clipboard`
  (copy/cut elements, paste with new ids, get/set text).

## 0.3.0 — Drawing (epoch E2)
Agents draw with stylus-like pressure, either by simulating the pen or by creating objects directly, and iterate
privately before committing.
- `draft` (begin/render/commit/discard/list): hidden draft layers without undo noise; a commit is one undo step and
  can be animated.
- Editing: `elements_edit` (move, scale, rotate, restyle including per-point width scaling, reorder, to_layer),
  `elements_delete`, `elements_select` (shows the selection in the app), `undo`, `redo`, `history`. Targets are
  element ids or the operation id of a drawing call. The app's own undo actions are used, so ids survive undo/redo.
- `import`: svg, image, xjson (lossless round trip verified), xopp pages, pdf pages (rendered, as new pages).
- Guide topics drawing, pressure, drafts, draw (text-to-drawing recipe) and pen; MCP prompt `draw`.
- Fixed a crash from iterating a temporary JSON object in `elements_edit`.
- Acceptance: text-to-drawing workflow (free space → draft → SVG scene with ink profile → render → animated commit
  as one undo step) and pen co-drawing/erasing. 64/64 integration scenarios.

### 0.2.2 — Pen engine
- `pen_draw`: a simulated stylus replayed through the app's real input pipeline (`InputContext::handleSynthetic`),
  at hand speed, faster, or instantly. Tools: pen, highlighter, `recognizer` (shape recognizer), eraser,
  lasso/rectangle selection, and the shape modes (line, rectangle, ellipse, arrow, double arrow, axes). Pressure is
  explicit or comes from a profile and is mapped exactly like hardware. The user's tool, color, size, drawing mode
  and selected layer are restored; a selection made by the pen stays active. Waits while the user is mid-stroke.

### 0.2.1 — Pressure model & direct drawing
- Stylus-like pressure model: width is computed exactly like hardware input
  (`max(minPressure, p·multiplier)·width`), with resampling to stylus density. Profiles: ink, brush, pencil,
  calligraphy (nib angle), marker, constant; tapers, variation and seed are adjustable, profiles are speed-aware
  when timestamps are given, and an optional hand tremor is available.
- `DrawApi`: content goes to an "AI" layer by default (created on top; the user's selected layer is kept). Every
  call is one undo step (including the new layer) and is tagged with an operation id (`op7`). Optional animation
  grows strokes point by point and stops safely if the document is replaced.
- Tools: `create_strokes` (points plus pressure, widths or times, or a profile), `create_shapes` (line, arrow,
  double_arrow, rectangle with rounded corners, ellipse, circle, polygon, polyline, bezier, arc, coordinate_system;
  fill; clean or hand-drawn), `create_text`, `create_link`, `create_image` (file or base64; aspect ratio kept),
  `create_latex` (the user's template; a built-in minimal template is used when standalone/scontents are missing),
  `create_from_svg` (paths, shapes, groups, transforms, colors, opacity, dashes, fills and `<text>`; fit into a
  target area), `find_free_space` (nearest free spot, optionally on one side of an element), and `new_page=true`
  on all drawing tools.
- Fixed an upstream crash: inserting undecodable image data (`Image::setImage`/`renderBuffer`).

## 0.2.0 — Files & Understanding (epoch E1)
Agents can open and save documents, read and see their content in detail, and export it.
- File tools: `file_info`, `file_open` (.xopp/.xoj, PDF to annotate, PNG, .xopt; `page`; `on_unsaved` =
  fail|save|discard), `file_new`, `file_close`, `file_save`, `file_save_as` (overwrite guard), `file_recent`.
  No dialogs are involved; discarding changes and overwriting files need the `destructive` permission.
- **xjson**: lossless JSON of elements (strokes with per-point widths, text, LaTeX with the compiled PDF, images,
  links; exact transforms), and lenient for hand-written input.
- `export`: pdf (the app's exporter; page and layer ranges), png (dpi, region), svg (vector, region), xopp (a copy)
  and xjson (pages, region, layers or element ids); written to a file or returned inline.
- `RenderApi` can also produce SVG. `whenReady()` helper for state the app updates asynchronously.
- Acceptance: an agent-style walk through "understand my page" on the handwriting benchmark (layout → line crops →
  overview → export); line crops read correctly as "This is a dumb text, written many times...".
- Verified: unit tests, 31/31 integration scenarios; builds with `-DENABLE_MCP=OFF`.

### 0.1.2 — Rendering & layout understanding
- `page_render`: a PNG of a page or region (dpi and max size, background toggle, layer filter, labelled coordinate
  grid, element highlights), returned inline and saved to a file, with pixel-to-point mapping metadata.
- `layout_analyze`: splits a page into blocks in reading order — handwriting (with text lines), figure,
  highlight, typed_text, latex, image, link — plus connectors (arrows/lines between blocks) and labels that belong
  to figures. Large strokes seed figures and dots are attached afterwards, so boxes with labels, arrows and dense
  handwriting are handled.
- `blocks_render`: high-resolution crops per block, or per handwritten line (`split_lines`), for transcription.
- `shapes_recognize`: the built-in shape recognizer on hand-drawn strokes (line, triangle, rectangle,
  quadrilateral, circle/ellipse with geometry).
- `guide` tool (conventions and recipes), plus MCP prompts `summarize`, `extract_text` and `explain_figure`.
- The test harness can generate `.xopp` documents (`xoai.make_xopp`) for scenario tests.

### 0.1.1 — Model access
- Stable element ids (`e42`) for the whole session. They survive moves, restyling and undo/redo, and are retired when
  the element object is destroyed (via a new `Element` destruction observer).
- `src/core/api`: `DocumentApi` (locate elements, list page content by layer and region) and `Geometry`
  (Ramer–Douglas–Peucker simplification that keeps pressure).
- Tools: `page_elements` (bbox, simplified or full detail; region, layer and type filters; pagination) and
  `pdf_text` (text of the PDF background, optionally per line with bboxes, or restricted to a region).
- Page background types are reported as `pdf`/`image` (not the internal `:pdf`/`:image`).

## 0.1.0 — Foundations (epoch E0)
Any MCP agent can now connect to a running xournalai and call its first tools.
- Tools: `app_status` (version, connection, permissions, document and active tool) and `doc_info` (pages, sizes,
  backgrounds, layers and element counts, paginated).
- Tool runtime: forgiving typed arguments (numbers and booleans are also accepted as strings; typos in argument
  names are reported), color parsing (hex, rgb(), CSS names), PNG and base64 helpers, and an export folder.
- End-to-end test harness `test/mcp_integration` (stdlib Python client, headless app, isolated config) and
  per-client setup guide `docs/mcp/CLIENTS.md` (Claude Code, Gemini CLI, OpenCode, Codex, Cursor, generic).
- Spikes (`docs/mcp/SPIKES.md`): synthetic pen events go through the real stylus pipeline with hardware-identical
  pressure; nanosvg is suitable for SVG → strokes; the highlighter never uses pressure.
  `InputContext::handleSynthetic()` hook added.
- Verified: 136/136 unit and GTK tests, 6/6 integration scenarios; also builds with `-DENABLE_MCP=OFF`.

### 0.0.3 — Protocol & transport
- JSON-RPC 2.0 / MCP protocol core (protocol versions 2025-06-18, 2025-03-26 and 2024-11-05): initialize, ping,
  tools, prompts, resources (with subscriptions) and logging. A tool registry that enforces portable schemas; an
  error barrier around every tool call.
- Streamable HTTP transport on `127.0.0.1:7474/mcp` (libsoup 3): sessions, bearer token, Origin/Host checks,
  paused replies for slow tools, SSE stream for notifications.
- `~/.config/xournalpp/mcp.json`: enabled flag, port, generated token, permission tiers (read, draw, ui, files and
  destructive; destructive is off by default), default layer, animation, export folder. Readable by the owner only.
- Command line: `--mcp`, `--no-mcp`, `--mcp-port=N`, and `--mcp-stdio` (a stdio bridge that starts the app if it
  isn't running).

### 0.0.2 — Build integration
- Recorded the upstream baseline: 119/119 tests passing (`docs/mcp/BASELINE.md`).
- `ENABLE_MCP` CMake option (default ON), with libsoup 3, nlohmann/json and a vendored copy of nanosvg.
- `src/core/mcp` module skeleton; `McpServer` is owned by `Control` and started with the main window.

### 0.0.1 — Planning & tooling
- Implementation plan, roadmap and task registry (`docs/mcp/`).
- Browser-based Kanban progress monitor (`tools/monitor/`).
- Fork versioning.
