# xournalai — Embedded MCP server: plan (rev. 3)

The MCP server is built **into** xournalai, in C++, inside the application process. It turns the running
application into a tool that any MCP-capable AI agent can use.

**Terminology**
- **Agent**: any AI harness that speaks MCP, e.g. Claude Code, Gemini CLI, OpenCode, OpenAI Codex CLI, Cursor,
  or my own scripts. Nothing in this design depends on one vendor.
- **Client**: the agent's MCP client component.
- **Me / the user**: the person holding the stylus.

---

## 0. Requirements

| # | Requirement |
|---|---|
| R1 | The agent can **draw** in the document |
| R2 | The agent can **control the whole application**: menus (open, navigate, select), dialogs, tools, view, files |
| R3 | The agent can **understand existing content**: summarise, extract text (handwritten and typed), interpret figures, diagrams, schematics and sketched ideas |
| R4 | **Co-creation**: I draw, and the agent watches, discusses, analyses and adds to my drawing |
| R5 | **Text-to-drawing**: the agent plans and executes a creative drawing from my description |
| R6 | The agent can **open files**: `.xopp`/`.xoj`, PDFs to annotate, images, templates, recent files. It can also create, save, save-as and close them |
| R7 | Agent drawing has **stylus-like pressure**: varying width along each stroke, tapering and speed, not a mouse's uniform line |
| R8 | Besides "drawing like a pen", the agent can **directly create** strokes, shapes and other objects with exact parameters |
| R9 | **Import and export through the MCP API**: documents, pages, regions, layers and individual elements in and out, in standard and lossless formats |
| R10 | **Agent-agnostic**: works with any MCP client, and doesn't depend on features only some clients implement |

---

## 1. Options considered: pros and cons

This analysis produced the decisions in §2. Each subsection ends with the decision that was taken.

### 1.1 Where the server runs
**In-process** (chosen)
- Pros:
  - direct access to the model, undo, rendering and the real input pipeline (needed for R7);
  - access to the GTK widget tree (needed for R2);
  - live events (needed for R4);
  - one binary and no second protocol.
- Cons:
  - a bug in the MCP code can crash the app and lose unsaved notes;
  - everything shares the GTK main thread, so heavy work can make the UI stutter;
  - rebasing onto upstream has more to merge.

**External server process talking to the app** (rejected)
- Pros: isolation, and it could use an official MCP SDK (Python/TS).
- Cons:
  - we would still need a large in-app bridge;
  - two protocols to maintain;
  - latency on streaming stylus drawing;
  - no widget-level automation.

**Mitigations of the in-process cons**
- Every tool call runs behind an exception barrier and strict input validation (limits on points, sizes and
  counts).
- An automatic backup snapshot is taken before destructive or bulk operations.
- Long work is chunked or moved to `XournalScheduler`.
- All new code lives in `src/core/mcp/` and `src/core/api/`, with thin hooks elsewhere.

### 1.2 Transport
**Streamable HTTP on localhost** (primary)
- Pros:
  - attaches to the app I already have open;
  - several agents can connect at once;
  - supported by most current clients.
- Cons:
  - an open port, which needs a token and an `Origin` check;
  - a few clients only support stdio.

**stdio** (compatibility)
- `xournalpp --mcp-stdio` is a tiny bridge mode. It forwards stdio to the running instance, or starts the app if
  none is running. This covers clients that only launch stdio servers.

**Decision:** HTTP first, with the stdio bridge in Phase 0, so R10 holds from day one.

### 1.3 How the agent draws: pen simulation or direct creation (R1, R7, R8)
There are two engines, both kept.

**Pen engine (stylus simulation)**
- The agent sends pen trajectories: points with pressure and timing. These are replayed as synthetic
  *pen-class* input events through the app's real input pipeline (`InputContext` → `StylusInputHandler` →
  tool handlers).
- Pros:
  - behaves exactly like my stylus: my pressure settings, the stabilizer, the shape recognizer, and every
    tool (pen, highlighter, **eraser**, lasso/rect **selection**, shape tools, fill);
  - it is naturally animated;
  - it is the most "creative" and human-like mode.
- Cons:
  - it depends on the current tool, zoom and scroll state (the target area must be laid out, though it
    doesn't need to be on screen);
  - it is slower (real-time or accelerated replay);
  - it can collide with me drawing at the same moment;
  - its feasibility needs a technical spike, because the events need a `GdkDevice`.

**Direct engine (object creation)**
- The agent specifies elements (strokes with per-point width, shapes, text, LaTeX, images, links), which are
  inserted straight into the model.
- Pros:
  - exact and fast;
  - batchable into one undo step;
  - doesn't depend on the view or tool state and never touches my current tool.
- Cons: bypasses tool behaviours such as the recognizer and stabilizer, and appears all at once (optionally
  animated as a visual effect).

**Decision:** both, with one shared pressure model (§3.3).
- Direct is the default for precise or bulk content, diagrams and import.
- The pen engine is used for expressive drawing, erasing and selecting "by hand", and whenever I want to watch.
- Collision handling: the pen engine only runs while I'm not mid-stroke (it queues otherwise). It saves my tool
  state and restores it afterwards. A small "agent pen" cursor shows where it is drawing.

### 1.4 Understanding content (R3)
**Vision by the agent's model plus structural help from the app** (chosen)
- Pros:
  - uses whatever intelligence the agent has;
  - no heavy local ML dependencies;
  - works for handwriting, diagrams and sketches alike.
- Cons:
  - costs tokens;
  - some models or clients have weak or no image support.

**Mitigations**
- Exact data wherever it exists: typed text, LaTeX source, PDF text.
- Layout analysis (blocks, reading order, connectors) and shape recognition reduce how much vision is needed.
- Tight, high-dpi block crops instead of whole pages.
- Images are returned as image content **and** as a PNG file path (for clients that can't show inline images).
- An optional local OCR engine later.

### 1.5 SVG as the creative channel (R5, R9)
**nanosvg** (header-only) flattening into strokes (chosen)
- Pros: LLMs are fluent in SVG, and the result is editable strokes.
- Cons: no `<text>`, limited CSS, and gradients are unsupported.

**Mitigations**
- `<text>` elements are converted to text elements by our own small pre-pass.
- Unsupported features are reported back to the agent as warnings.
- Pressure profiles can be applied to SVG paths so they don't look mechanical.

### 1.6 UI automation (R2)
**Semantic tools plus real widget-level automation** (chosen)
- Pros: everything I can reach, the agent can reach, including plugin menus and any dialog.
- Cons:
  - widget trees are fragile across versions;
  - the agent can interfere with what I'm doing;
  - some GTK widgets are hard to drive.

**Verified supporting facts**
- Menus are a `GMenuModel` bound to `win.*` actions (116 `Action` values).
- Dialogs are non-blocking popups (`PopupWindowWrapper`); only `XojMsgBox` uses `gtk_dialog_run`.

**Mitigations**
- Semantic tools come first; UI automation is a complete fallback.
- Stable widget ids from `GtkBuildable` names.
- A visible "agent is operating" hint, the Stop button, and the permission tiers.

### 1.7 Tool surface size and schema style (R10)
**Many fine-grained tools**
- Pros: clear for the model.
- Cons: some clients cap the total tool count (around 100–128 across all servers), and large tool lists cost
  context on every turn.

**Few mega-tools**
- Pros: small.
- Cons: complex schemas confuse models, and some providers reject `oneOf`/`anyOf`/`$ref`.

**Decision**
- About **45 tools**, grouped by domain, with an `op` enum where operations share parameters.
- Flat JSON Schemas: no `$ref`, no `oneOf`/`anyOf` at the root, and a documented `type` on every field.
- Names are `snake_case` and at most 40 characters.

### 1.8 Client-feature dependence (R10)
- Not every client supports MCP prompts, resources, subscriptions, SSE notifications or sampling.
- **Decision:** **everything is reachable through plain tools.**
  - Workflows are also available as `guide(topic)`, a tool that returns step-by-step recipes, in addition to
    MCP prompts.
  - Change tracking is available as the `wait_for_user` / `changes_get` tools, in addition to push notifications.
  - Export output is available as a file path or inline data, in addition to resources.

### 1.9 Data transfer for import and export (R9)
- **File path**
  - Pros: no size limit and cheap.
  - Cons: the agent must share the filesystem, which isn't true for sandboxed or remote agents.
- **Inline base64**
  - Pros: works anywhere.
  - Cons: token and size cost.
- **Decision:** both.
  - `delivery: "path" | "inline"` with a size cap for inline, defaulting to path.
  - Exports go to a known folder, `~/.local/share/xournalpp/mcp-exports/`, unless a path is given.

---

## 2. Key decisions (summary)

| Topic | Decision |
|---|---|
| Location | In-process C++: `src/core/mcp/` (transport, protocol, tool registry, JSON) and `src/core/api/` (typed services). Build option `ENABLE_MCP`. |
| Transport | Streamable HTTP on `127.0.0.1:7474` plus the `--mcp-stdio` bridge |
| Libraries | libsoup 3 (HTTP on the GLib main loop), nlohmann/json, nanosvg |
| Drawing | Pen engine (simulated stylus through the real input pipeline) plus the direct engine (model objects), sharing one pressure model |
| Understanding | Structure, layout analysis and crops from the app; interpretation by the agent's model |
| Agent-agnostic | About 45 flat-schema tools; everything reachable via tools; images as inline content plus a file path |
| Safety | Localhost only, token, `Origin` check, permission tiers, backups before destructive or bulk operations, all agent edits undoable, Stop button |
| Code reuse | `api/` services built from the logic in `plugin/luapi_application.h` (element I/O with undo, structure, export, actions) |

---

## 3. Capabilities in detail

### 3.1 Files — R6
- `file_open(path, mode)`, where mode is one of:
  - `open` (`.xopp`/`.xoj`);
  - `annotate_pdf` (a PDF as the page background);
  - `image_as_page`;
  - `template`.
- `file_open` has an `on_unsaved` policy for the current document: `save`, `discard` (needs permission) or
  `fail` (the default).
- `file_new(template?, page_size?, background?)`, `file_save`, `file_save_as(path)`, `file_close`,
  `file_recent`, and `file_info` (path, format, modified flag, page count, background PDF, autosave state).
- Files can also be opened through the real UI: the File menu, then the file chooser (§3.7).

### 3.2 Import and export — R9
**Export (`export`)**
- **Scope**: document, page range, one page, a region (rectangle), a layer range, or selected elements.
- **Formats**:
  - `pdf`, `png` (dpi, width or height; transparent or with background), `svg`;
  - `xopp` (whole document, or selected pages as a new document);
  - **`xjson`**: our lossless JSON of elements, with per-point width, style, text, image data and LaTeX source.
- **Delivery**: `path` or `inline` (§1.9). PNGs can also come back as image content, so the agent sees them.

**Import (`import`)**
- `xopp`: append or insert pages from another document.
- `pdf`: pages as backgrounds (annotate), or appended.
- `image`: as an image element or as a page background.
- `svg`: as editable strokes (§1.5) or as an image.
- `xjson`: lossless element round-trip. This lets the agent copy, transform (for example scale or recolour) and
  re-insert content programmatically.
- The source is a `path` or `inline` data. Placement uses an explicit page and position, the placement helpers,
  or "into the draft" (§3.5).

**Clipboard**: `clipboard(op: copy|cut|paste|get|set)`, using the app's existing clipboard handling for
selections, text and images.

### 3.3 Stylus-like pressure model — R7 (shared by both engines)
How the app works today (verified in `StrokeHandler`): each stroke point stores its own width, `z`.
- A real stylus reports pressure p ∈ [0,1].
- The app applies my *minimum pressure* and *pressure multiplier* settings, then multiplies by the tool's base
  width.
- The renderer then interpolates width between points.

Agent strokes can specify width in three ways:
1. **`pressure: [p…]`** (normalised 0–1). This is mapped **exactly like a hardware stylus**, through my pressure
   settings and the tool width. Pen-engine strokes are indistinguishable from my own.
2. **`widths: [w…]`** (absolute, in points). Exact control, used by the direct engine and `xjson`.
3. **`profile`**, a generator for when the agent doesn't want to compute arrays:
   - presets: `constant`, `ink` (slight taper and gentle variation), `brush` (strong swell and taper),
     `pencil` (light, low variation), `calligraphy` (width depends on stroke direction relative to a nib angle),
     `marker`;
   - parameters: `base`, `min`, `taper_in`, `taper_out`, `variation`, `nib_angle`, `seed`;
   - **speed-aware**: with timestamps `t`, faster segments get thinner, like real ink.

In addition:
- **Resampling**: paths are resampled to stylus-like point density, 0.5–1 pt by default, so width transitions are
  smooth and not polygonal.
- **Human feel (optional)**: `jitter` (sub-point tremor) and `timing` (a natural speed curve: acceleration at the
  start, deceleration into corners) for the pen engine.
- **Applies to**: `pen_draw`, `create_strokes` and `create_from_svg`. Shapes use it only when a profile is
  requested, and are uniform otherwise.
- **Limit**: pressure only has an effect where the app itself uses it (the pen tool and pressure-enabled strokes).
  The Phase 2 spike confirms the highlighter's behaviour. Tilt and rotation are not modelled, because the app
  ignores them.

### 3.4 Drawing — R1, R7, R8
**Pen engine: `pen_draw`**
- **Input**: `strokes: [{points:[[x,y,p?,t?],…] | path:"<svg d>" , profile?}]` plus `tool`
  (`pen` | `highlighter` | `eraser` | `select_lasso` | `select_rect` | `shape_rect` | `shape_ellipse` |
  `shape_arrow` | `shape_line` | `spline`).
- **Tool settings**: colour, size, fill, line style, and the recognizer on or off.
- **Replay**: `speed` (`realtime` | a multiplier | `instant`) and `page`.
- **Output**: ids of the created or erased or selected elements, plus an optional render of the affected area.
- The agent can therefore **erase** (stroke or whiteboard eraser) and **select by lasso** like a user does.

**Direct engine: `create_*`**
- `create_strokes` takes points plus `pressure` / `widths` / `profile`, colour, tool type
  (pen or highlighter), fill, opacity, line style and cap style.
- `create_shapes` takes line, arrow, double arrow, rectangle (optionally rounded), ellipse or circle, polygon,
  Bézier or spline, arc, and coordinate system. Each can have a fill, opacity and line style, and optionally a
  pressure profile.
- `create_from_svg` takes an SVG string or file, a target rectangle and a fit mode (`contain` | `cover` |
  `none`), with an optional pressure profile. It returns the element ids and any conversion warnings.
- `create_text`, `create_latex` (through the existing `LatexController`), `create_image`, `create_link`.

**Placement helpers** (for both engines)
- `find_free_space(page, w, h)`.
- Relative anchors such as `{right_of:"e42", gap:20}`, `{below:"block:b3"}` and `{center_in:[x,y,w,h]}`.
- `page` can be `"current"` or `"new"`.

**Targeting**
- Default layer: **"AI"** (created on demand), or `current`, or a named layer.
- Each call is one undo step.
- Elements are tagged with their origin (agent) and an operation id, which makes "remove what the agent
  did in operation X" possible.

### 3.5 Drafts — R5
- `draft(op: begin|render|commit|discard, page, region?)`.
- Between begin and commit/discard, all drawing goes to a draft layer.
- `render` returns an image for self-critique.
- `commit` merges the draft into the target as one undo step, optionally animated.
- `discard` leaves the document untouched.

### 3.6 Editing and structure
- **Elements**:
  - `elements_select`;
  - `elements_edit(op: move|scale|rotate|restyle|reorder|to_layer)`;
  - `elements_delete`.
- **History**: `undo`, `redo`, `history`.
- **Pages**: `page_manage(op: insert|delete|move|background|size|goto)`.
- **Layers**: `layer_manage(op: add|rename|visibility|select|merge_down|delete|copy)`.
- **Tools**: `tool_get`, `tool_set` (tool, colour, size, fill, opacity, line style, eraser type, stabilizer,
  recognizer).
- **View**: `view(op: get|zoom|scroll|layout|fullscreen|presentation)`.

### 3.7 Application control — R2
**Semantic**
- `actions_list`: all 116 actions with their enabled flag and state.
- `action_run(name, state?)`.

**UI automation**
- **Menus**:
  - `ui_menu_tree` returns the full menu hierarchy with labels, paths, accelerators, enabled flags and
    toggle/radio state. Dynamic menus (Recent, Plugins, page types) are included.
  - `ui_menu_select(path, visible=true)` opens the menubar menu, navigates the submenus with visible
    highlighting, and activates the target item.
- **Windows and dialogs**:
  - `ui_windows` lists the open windows and dialogs.
  - `ui_inspect(window, depth)` returns the widget tree with stable `wid`, role, label, value, enabled flag and
    bbox.
  - `ui_screenshot(window|wid)` returns a PNG.
- **Interaction**:
  - `ui_interact(op: click|set_value|toggle|select_row|close, wid, value?)`;
  - `ui_keys(op: shortcut|type, value)`;
  - `ui_file_chooser(window, path, accept)`;
  - `ui_wait_for_window(match, timeout)`.
- **Toolbar**: toolbar items are included in `ui_inspect` of the main window.

### 3.8 Understanding — R3
**Structure**
- `doc_info`.
- `page_elements(page, layer?, region?, detail: bbox|simplified|full, tolerance)`, paginated.
- `pdf_text(page, positions)`.

**Layout analysis (`layout_analyze`)**
- Groups strokes into **blocks**, using space, stroke adjacency and the creation order in the layer.
- Labels each block: `handwriting` | `figure` | `connector` | `highlight` | `typed_text` | `latex` | `image`.
- Returns the blocks in reading order, the links between them (arrows or lines connecting blocks), and the bbox
  of each.

**Vision**
- `page_render(page, region?, dpi|max_px, background, layers, grid, highlight_ids)`.
- `blocks_render(block_ids, dpi)` returns tight crops, with image content and file paths.

**Recognition aid**: `shapes_recognize(ids)` uses the app's `ShapeRecognizer`.

**Notes memory (optional)**: `notes(op: set|get, page, region, kind: transcript|summary|meaning|note, text)`.
*Implemented (0.5.x) as a sidecar `<file>.ai-notes.json` next to the document instead of inside the `.xopp`: an
unknown XML tag makes upstream Xournal++ report load errors, and the file parser stays untouched. Notes follow
pages by identity (moves, inserts), are kept in memory for untitled documents and written on the first save.*

**Recipes** (as MCP prompts and as `guide(topic)`): `summarize`, `extract_text`, `explain_figure`.

### 3.9 Co-creation — R4
- **Changes**:
  - `changes_get(since)` returns the event log: added, erased, moved, restyled, page, layer and tool changes.
  - `wait_for_user(idle_ms=1500, timeout_s=300, region?)` blocks until I've drawn something and then paused. It
    returns the changes, the affected region, a render of that region, and my current tool, colour and page.
- **Presence**:
  - `view(op:get)` gives the visible area, pen position and selection.
  - `show_message(text, anchor?)` shows a callout on the canvas.
- **Fitting in**:
  - `profile:"match_user"` derives width and pressure behaviour from my recent strokes.
  - Anchors place additions next to my content.
  - The AI layer is accepted (merged down) or discarded from the UI or via `layer_manage`.
- **Push** (for clients that support it): resource subscriptions and SSE `notifications/resources/updated`.

### 3.10 Meta and compatibility — R10
- `app_status`: version, capabilities, permission tiers, active document, whether the user is mid-stroke.
- `guide(topic)`: recipes and conventions (coordinates, pressure, anchors).
- **Setup**: `docs/mcp/CLIENTS.md` gives ready-made configuration snippets for Claude Code, Gemini CLI,
  OpenCode, Codex CLI and Cursor, plus a generic HTTP and stdio section.
- The settings page shows the URL and token, with copy buttons.

**Tool count**: about 45 (the list in §3 is the catalogue; the final list lives in `docs/mcp/TOOLS.md`).

---

## 4. In-app UI
- **Status indicator**: off, listening, N agents connected, agent drawing, agent operating the UI.
- **Agent pen cursor** during pen-engine replay, and a highlight on newly added elements.
- **Stop agent** button and shortcut: cancels the running operation and rejects further calls until re-enabled.
- **Settings**:
  - enable, port, token, permission tiers (read / draw / control UI / files and destructive);
  - default target layer, animation speed;
  - pen-engine collision policy (queue or refuse while I'm drawing), backup-before-bulk-ops.
- **AI layer actions**: accept (merge down), hide, clear.

---

## 5. Phases

| Phase | Content | Requirements | Milestone (acceptance) |
|---|---|---|---|
| **0 — Foundations and spikes** | Build deps, build upstream, tests green. `ENABLE_MCP`; libsoup, JSON and nanosvg wiring; HTTP and protocol layers; token; settings; `--mcp-stdio` bridge; `app_status`, `doc_info`. **Spikes:** (a) synthetic pen events through `StylusInputHandler`; (b) nanosvg to strokes; (c) highlighter and pressure behaviour | R10 | Two different agents (e.g. Claude Code over HTTP and Gemini CLI or OpenCode over HTTP or stdio) connect and call `doc_info`. Spike report written |
| **1 — Files and understanding** | `file_*`, `export` (pdf/png/svg/xopp/xjson, path and inline), ID registry, `page_elements`, `pdf_text`, `page_render`, `layout_analyze`, `blocks_render`, `shapes_recognize`, `guide` | R3, R6, R9 (export) | Open a real handwritten `.xopp` via MCP: correct summary, transcription and diagram explanation. Export a page as PNG, SVG and xjson |
| **2 — Drawing** | Pressure model, direct engine (`create_*`), pen engine (`pen_draw` incl. eraser and lasso), placement helpers, AI layer, attribution, drafts, animation, `import` (svg, image, xjson, xopp and pdf pages), editing and history | R1, R5, R7, R8, R9 (import) | Text-to-drawing of a described scene with visibly pressure-varied strokes. xjson round-trip is lossless. The agent erases and lasso-moves one of my strokes via the pen engine |
| **3 — Application control** | `actions_*`, `page_manage`, `layer_manage`, `tool_*`, `view`, `clipboard`, and UI automation (`ui_*`), permission tiers, backups | R2 | Scripted: File → Recent via the visible menu, change the page background in its dialog, export a PDF through the Export dialog, save as a new name |
| **4 — Co-creation** | `changes_get`, `wait_for_user`, SSE push, presence, `show_message`, `match_user`, pen cursor, Stop button, status indicator | R4 | I sketch half a flowchart. After I pause, the agent comments and completes it in my pen style |
| **5 — Polish** | Notes memory, performance (render cache, large docs), `TOOLS.md`, `CLIENTS.md`, `COVERAGE.md` (every action, menu item, dialog and Lua API function mapped), and optionally moving the Lua API onto `api/` | all | Coverage checklist green; tested with three or more different agents |

---

## 6. Testing
- **Unit** (gtest, `test/unit_tests/mcp/`):
  - JSON-RPC and handshake, schema validity, and a schema-portability lint (no `$ref`/`oneOf`, name rules);
  - pressure mapping (must equal the hardware path for the same p), profiles, resampling;
  - SVG flattening, xjson round-trip, layout clustering, ID registry, anchors, drafts.
- **Integration** (`test/mcp_integration/`, Python `mcp` SDK, `xvfb-run xournalpp --mcp`): one scenario per
  milestone, asserting on the saved `.xopp` and on renders.
- **Client matrix**: each phase is smoke-tested with at least two different agents.

## 7. Risks

| Risk | Mitigation |
|---|---|
| Crash in MCP code loses notes | Exception barriers, validation limits, a backup before bulk or destructive ops, autosave interplay |
| Pen-engine synthetic events don't fit the GTK device model | Spike in Phase 0; fallback is the direct engine plus the pressure model and animation (same look, minus tool behaviours) |
| Agent and I edit at the same time | Queue the pen engine while I'm mid-stroke; the direct engine never touches my tool; per-operation undo |
| Messy handwriting defeats vision | High-dpi block crops, notes memory, optional local OCR later |
| Token and payload size | bbox-only modes, simplification tolerance, pagination, path delivery |
| Client differences | Tools-only baseline, flat schemas, stdio bridge, client matrix tests |
| Upstream GTK4 migration | UI automation behind an interface; the existing `gtk4_helper` shims |

## 8. Environment (checked 2026-09-28)
Pop!_OS 22.04 (Ubuntu jammy), GCC 11. The build dependencies are being installed:
```sh
sudo apt-get install cmake ninja-build libgtk-3-dev libpoppler-glib-dev portaudio19-dev libsndfile-dev \
  texlive libxml2-dev liblua5.4-dev libzip-dev librsvg2-dev gettext libgtksourceview-4-dev help2man \
  libqpdf-dev libgtest-dev libsoup-3.0-dev nlohmann-json3-dev xvfb
```

## 9. Later (outside the MCP work)
- Google Drive: OAuth sign-in and Drive as a storage backend.
- In-app assistant panel and local handwriting OCR, built on the same `api/` layer.
