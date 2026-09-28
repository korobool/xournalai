# xournalai changelog

The fork has its own version (`XOURNALAI_VERSION` in `CMakeLists.txt`). The application reports it as
`<upstream version>+ai.<fork version>`, e.g. `1.3.7+ai.0.1.0`. The embedded MCP server reports the fork version.
See [ROADMAP.md](ROADMAP.md) for the versioning rules.

## Unreleased

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
