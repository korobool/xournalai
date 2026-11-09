# 🗒 Ведение конспекта (`konspekt`)

A conspect the owner could print: **layout quality and graphs first**. Requests work as in `SKILL.md`, within the
safe list.

What changes from `SKILL.md`:
- **A page plan before the cards.** When the first two or three topics are clear, plan the free area as a grid:
  - sections in reading order, two or three columns;
  - one bigger slot for the main scheme;
  - room kept for graphs.

  Workers draw into the plan's slots, never just "wherever there is room". Revise the plan when the talk turns,
  but don't move what is already drawn.
- **Typeset carefully.** Use the layout library of the konspekt-magazine skill if it is available (wrapped text
  with the app's real font metrics, LaTeX sizing, tables, timelines, card styles). Otherwise measure with
  `find_free_space` and `page_render`.
  - Text never overflows a card. Margins and gaps are the same everywhere.
  - One accent colour per topic. Titles 5.5 pt, body 4.2 pt, nothing under 3.8 pt.
- **Graphs and formulas:**
  - Draw a graph for every quantity, trend or function that is talked about, from real numbers when there are any
    (matplotlib → SVG → `create_from_svg`), marked «≈» or with a source.
  - Formulas in LaTeX.
  - Processes and structures as editable schemes, not as prose.
- **A clean page.** At the end, and every ~10 minutes of a long talk, one worker (effort `high`) composes a clean
  page after the live pages from the cards drawn so far: the same content, laid out as a page of a printed
  conspect. The live cards stay. Keep a list of what each pass covered in `DIR/state.json`.
- The summary card is at the end of the clean page.
