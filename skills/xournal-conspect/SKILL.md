---
name: xournal-conspect
description: Make or redraw study notes ("conspects") in the owner's handwritten cheat-sheet style — their own handwriting glyphs, code in boxes, LaTeX formulas in wavy clouds, braces, dot-anchored connectors, mini plots — and draw them into xournalai via its MCP tools; also the rules for live co-editing on the canvas. Use when asked to write/redraw a conspect, cheat sheet or lecture notes in "my style", to convert a photo of paper notes, or to watch and beautify what the owner writes in xournalai.
---

# Conspects in the owner's style

Reference: the owner's paper conspect `~/Desktop/LinearRegression.jpg` ("Logistic Regression as a NN") and its
redraw `examples/logistic_regression.py` → preview `examples/logistic_regression_preview.png`. Match that look.

## Style guide (what makes it "theirs")
- **Page**: A4 landscape (842 × 595 pt), very dense — small sizes are fine, it is meant to be zoomed.
- **Grid**: 2–3 columns separated by long ruled lines; horizontal rules split a column into topics.
- **Headings**: handwritten, underlined (`Sheet.heading`), mixed case ("NumPy implementation of Logistic Regression").
- **Font**: v3 (default) — their own letters, cleaned. Never replace their shapes with designed ones; improve by
  cleaning/choosing variants. To complete the set, ask them to write a specimen page (a–z, A–Z, 0–9, symbols,
  each char separated by spaces), capture it with capture_glyphs.py (or parse the saved .xopp), then review a
  variant sheet and update PICK. Watch for letters that misread when small (their n→a, f→t, capital I→J).
- **Where the handwriting lives**: the captured glyphs are personal, so they are not in the repo:
  `$XOURNALAI_GLYPHS`, else `~/.local/share/xournalai/glyphs_user_raw.json` (capture_glyphs.py writes there).
  Without them, `Font()` falls back to the designed v2 font.
- **Prose**: handwritten (`Sheet.text`), short explanatory sentences, ~3.5–4.2 pt x-height, line ≈ 2.6× size.
- **Code**: handwritten too (`Sheet.code`), each function in its own **rectangle**, stacked in the middle column;
  indentation preserved; `FP[` / `BP[` style **left brackets with vertical labels** mark phases (`Sheet.bracket`).
- **Math**: LaTeX via xournalai `create_latex` (never handwritten), placed in **wavy "cloud" boxes** (`Sheet.cloud`)
  or plain rectangles for derivation groups; small tag boxes (rounded) for names like "cross-entropy loss function".
- **Links**: right-side **curly braces** (`Sheet.brace`) on code lines, joined by **connector lines ending in a dot**
  (`Sheet.connector`) to the box that explains the math of those lines. Braces also carry side labels
  ("activation", "cost", "dw", "db").
- **Diagrams**: neuron-style boxes (`rrect` = `cloud(amp=0)`), arrows with small heads, mini plots with arrowed axes,
  tick labels in tiny handwriting, dashed asymptotes.
- **Ink**: single near-black colour (`#141414`) for this style. (Coloured variants are OK when asked.)
- **Corrections**: fix bugs/typos silently in the drawing, but list every correction in the chat reply.

## Pipeline
```
lib/conspect.py        Sheet: text, heading, code, rect, cloud, brace, bracket, arrow, connector, dot, axes, polyline,
                       latex(x, y, src, height, maxw=…)  → save(base) writes base.svg, base.latex.json, base.png
lib/glyphs_v3.py       DEFAULT font (v3) = the owner's OWN captured glyphs, cleaned (not redesigned): fit to guide
                       lines, spline-smoothed, near-straight strokes straightened, common slant, widths evened,
                       hand-picked good variants (PICK = raw indices into glyphs_user_raw.json), ×0.9 narrower (NARROW
                       in conspect.py). Owner verdict: v1 raw = "messy but mine", v2 = "looks synthetic", v3 = wanted.
                       Characters never captured (g j p q x z, most caps, digits, symbols) fall back to v2 shapes.
lib/glyphs_v2.py       redesigned font (fallback source only); glyphs_synth.py + raw json = v1 (Font(style="v1"))
lib/capture_glyphs.py  grow the library from strokes they write on the canvas (see docstring)
lib/beautify.py        rewrite a handwritten line with smoother/straighter strokes, optional letter swap (spell fix)
examples/logistic_regression.py   full worked example (layout coordinates = scan px × 0.6)
```
1. Write a script like the example (copy it; coordinates in page points, origin top-left). Use `maxw=` on every
   formula that sits in a box — widths are measured by compiling real LaTeX (pdflatex+pdftoppm, cached in
   `~/.cache/xournal-conspect`), so the formula shrinks to fit.
2. `python3 script.py OUTDIR` → look at `OUTDIR/*.png` (Read it) and iterate on the layout before touching the canvas.
3. Draw into xournalai (don't pass `layer`: the app puts AI drawings on the layer the user chose in its settings):
   - new landscape page: `page_manage` (check its schema; A4 landscape = 842 × 595) — or ask the owner to add one;
   - `create_from_svg(path=OUTDIR/x.svg, x=0, y=0, scale=1, fit="none", profile="ink", animate=false)`;
   - for each entry of `x.latex.json`: `create_latex(latex, x, y, height, color, page=N)`;
   - `page_render` and compare with the preview.
   - **If a xournalai tool says "xournalai is not open"**: ask the owner to open xournalai and say when. The tools
     come back by themselves (no `/mcp` reconnect). Then continue. Never launch xournalai yourself and never start
     a second window; one window serves agents at a time.
   - **If no `mcp__xournalai__*` tools exist in the session at all** (a session from before the user-wide setup):
     ask the owner to run `/mcp` → xournalai → Reconnect, or to restart with `claude --continue`.
   - Only if the owner wants a file instead: `Sheet.save_xopp(path)` writes a native .xopp (editable strokes +
     LaTeX teximages); verify headlessly with `xournalpp FILE --create-img=out.png`, then give them the path. Big tool results (page_elements full, create_from_svg with
     hundreds of strokes) get saved to a file — read them with `jq`.

## Live co-editing rules (when the owner writes with the stylus)
Loop: `wait_for_user(idle_ms≈3500, timeout_s≤80)` → act → `wait_for_user(since=cursor)`. After ~2 empty timeouts, stop and
report; resume when they say "watch".
- **Formulas** (incl. later small edits/crossings-out to a formula) → re-typeset the whole formula as LaTeX at the same
  spot, delete their strokes.
- **Regular text / captions** → rewrite with better strokes *in their own handwriting* (beautify: smooth, deskew,
  even spacing), fixing spelling and grammar (swap wrong letters for their glyphs); delete originals.
  Tips (grammar hints) may go in small magenta text next to it when a rewrite isn't wanted.
- **Diagrams** → redraw geometrically accurate (clean rectangles/ellipses/arrows/axes, mesh surfaces for 3D sketches),
  colourise, keep their labels; delete the rough strokes.
- If they erase something you annotated, remove your now-orphaned annotations.
- **Drawings (objects)** → always colourise with a *consistent* palette: natural colours for the object, symmetric/
  corresponding parts get the same colour (both wing pairs alike, etc.), outlines darker than fills, keep the
  same palette for the same kind of object across the page.
- **Markers** the owner writes next to something (small asterisk and exclamation mark strokes; remove the marker
  strokes after acting):
  - `*!`  → recognise the object and REDRAW their drawing better: same composition/pose/size, clean confident
    professional strokes (smooth curves, correct proportions, symmetry), then colourise consistently.
  - `**!` → GENERATE a professional illustration of the object (detailed SVG, shading/details, consistent colours)
    in the same spot and size, replacing their sketch.
  - `*w!` → web-search the object/term (brontir advanced_web_search / WebSearch), add a short summary (2–4 lines)
    next to it in their handwriting (v3 font, small, grey-blue), cite nothing inline but mention the source in chat.
  - `*r!` → generate a REAL image/icon with a generative image model and insert it (xournalai create_image). Needs an
    image-generation tool/API; if none is available, say so, then: icon-style → render a flat PNG icon procedurally
    (matplotlib patches, transparent bg) and create_image it; realistic → a Public-domain/CC0 photo from Wikimedia
    Commons (API: generator=search&prop=imageinfo&iiprop=url|extmetadata, keep only LicenseShortName PD/CC0), cropped
    to the object, and name the file + licence in chat.
  - `*c!` → the handwritten text after the marker is a COMMAND to the AI: read it, do it, then remove the command
    strokes (and say in chat what was done).
- Ask before deleting anything that is not theirs-being-replaced; the app backs up before deletes
  (`~/.local/share/xournalpp/mcp-backups/`).

## Sketching with native strokes (the Einstein technique)
When a likeness or a realistic object is wanted and procedural drawing can't reach it: get a reference picture
(web search for a PD/CC0 image, e.g. Wikimedia Commons, or the user's own, or one generated with the image tool
your session has), then turn it into pencil strokes:
```
python3 lib/pencil.py photo.jpg OUT/sketch.svg --width 84 [--crop X0 Y0 X1 Y1] [--step 0.5] [--no-fade] --png OUT/p.png
```
Look at the PNG, then `create_from_svg(path=OUT/sketch.svg, x, y, scale=1, fit="none", profile="ink")`. ~3000
strokes for a portrait; bigger `--step` = lighter and fewer. `examples/architecture_flow_rtl.py` shows a dense,
right-to-left architecture/data-flow page (clouds, boxes, braces, connectors).

## As xournalai's serving session (in its AI terminal)
- Work arrives as `[xournalai] …` lines typed into your terminal (toolbar actions, markers, Auto-improve edits).
  Handle them, then end the turn; don't loop on `wait_for_user`.
- Zones for toolbar actions and markers are shown by the app; for your own work call `thinking` (op=start/done).
- Markers come with their stroke ids: delete them with `elements_delete` after acting.

## Pitfalls
- `create_from_svg(profile="match_user")` works since xournalai 1.1 (the owner's pressure, the SVG's own colors).
- Never pass `layer` unless the user asks: xournalai enforces its layer setting and tells you (`layer_note`) when it ignored yours.
- The local TeX lacks `standalone.cls`; `tex_png` uses `article` + auto-crop (preview/measuring only — xournalai uses its
  own template).
- The owner's captured `g` is really "ng" and `p` reads as `n` → those two come from the synthetic set (`USER_SKIP`).
- The xournalai MCP endpoint needs an auth token; never pull it out of config files — use the MCP tools.
- Exporting a canvas page as a PNG for docs: render the PAGE itself (`page_render` or `export`), not the source
  SVG/PNG. Anything added on the canvas separately (images via create_image, freehand icons, stroke portraits) is
  missing from the source render — this dropped the users/brokers icons (fleet 0.9.49) and the Einstein sketch (0.9.51).
