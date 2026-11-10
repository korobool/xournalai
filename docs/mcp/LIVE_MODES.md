# Live-notes modes: a proposal (2026-11-09)

> **Status: proposal, for the owner's decision.** Nothing here is built yet. Section 12 lists the questions
> that need the owner's answer.

## 0. Decisions (the owner, 9 Nov 2026, evening)

- **The Record menu** (long press): a **grid of tiles**, one tap starts. **Large toggle chips** at the top are
  remembered between recordings. Everything works by finger.
- **Tiles:**
  - ⏺ Запись (plain recording);
  - 📝 Текст (transcript only);
  - 🎙 Голосовое управление;
  - ✍ Просьбы к рисунку;
  - 🧠 Конспект + просьбы;
  - 🛡 Конспект без команд;
  - 🗒 Ведение конспекта.
- **Toggles:**
  - 🔎 Браузер-помощник;
  - ⚡ Интенсивно (this is the "intensive brainstorm": more workers, higher effort, more frequent wake-ups);
  - ❓ Панель вопросов;
  - 🧾 Итог в конце;
  - 🔤 Просьбы по слову (a trigger word).
- **The browser-assistant's browser:** by default, **a profile of xournalai's own** (its own folder; the owner
  signs in to Google once). In AI Agent Settings it can be switched to the owner's Chrome with a chosen profile,
  or to incognito.
- **Further decisions (the same evening):**
  - **No separate coordinator session:** the companion stays the coordinator. It does small things itself and
    delegates the rest. A tap on the question panel is the owner's explicit consent, also for actions outside the
    app, and the companion is the one able to carry them out.
  - **The browser assistant's window:** on another monitor if there are several, otherwise on the second
    workspace, where he works with it.
  - **The clean page** of 🗒 is typeset; his handwriting only when he asks for it.
  - **The trigger phrase is «Hey Assistant»,** off by default (requests are then recognised by meaning).
- **The research browser, as built (1.18.0, the owner, 10 Nov night):**
  - a search needs clicks, typing, a site's navigation and tabs, not only reading, so agents use the **Claude in
    Chrome extension** in it;
  - it is Chrome with a profile of the app's own («xournalai research»), into which the app copies the owner's
    native-messaging manifest of the extension so that it can reach Claude Code;
  - the app itself drives it through the DevTools protocol for the cheap part: opening searches, reading, pictures,
    «📌 Insert selection»;
  - Playwright, or a browser build of our own, are for later.
  - **Signing in (1.18.1):** Google refuses to sign in a browser a program is connected to (the owner's first try:
    «This browser or app may not be secure» on Claude in Chrome's «Continue with Google»). So signing in happens in
    the same profile restarted without the DevTools port (its first start, «🔑 Browser sign-in», `research
    op=signin`). When he closes it, it comes back connected; the sign-ins stay in the profile.
  - **In the background (1.18.2, the owner, 10 Nov):** it never takes the focus; on one monitor it is behind the
    notes (he goes there and back with one tap: «🔎 Browser» / «↩ Back», nothing added to the browser); its tabs
    never sleep; target pages (★) stay until he closes them, intermediate ones are closed by the agent as it goes;
    agents use exactly its device id, other browsers (his own, DataAnnotation) only when he names them.
  - **Later (brainstorm, not decided):** a small extension of our own over native messaging instead of the DevTools
    port (no port, no sign-in mode); a built-in WebKitGTK browser for research; our own Chromium build only if the
    extension hits a wall.
- **The Record menu, two questions (1.20.0, the brainstorm of 10 Nov):** tiles for what appears on paper (📝 🧠 🗒 🎙),
  one switch «Speech commands» (🛡 off / 🔤 only «Hey Assistant» / 💬 conversation), remembered, in sight on every
  tile and on a chip during the recording, changeable mid-recording. ✍ became 📝 + 💬 and 🛡 became 🧠 + 🛡 (their ids
  still work). Presets were left for later.
- **Fewer taps (10 Nov):** the question panel is only for unsafe operations outside the app and for disambiguation.
  An explicit command on the document, his own strokes included, is done at once as one undo step.
- **Deferred, experimental:** telling the speakers apart ("я / собеседник" in an interview; diarization on the A40
  server). Only as an experimental option, later.
- **Voice control:**
  - **push-to-talk by default:** a big 🎙 button appears during the recording; what is said while it is held is
    a command, everything else is context;
  - **a trigger word as an option:** with the 🔤 toggle, a phrase starting with it is a command too.

## 1. What the owner asked for

A long press on Record offers two choices today: plain recording, and "Record + live notes" (transcript, topic cards
and spoken requests within a safe list). The owner wants more choices, and default settings that cover:

1. **Annotate only.** The transcript is visible as text.
2. **Annotate and carry out spoken requests,** above all on the drawing the owner is making right now.
3. **Annotate, summarise automatically and carry out requests.** This is what live notes do today.
4. **Annotate and summarise** as schemes, mind maps, pictures and text blocks, **but never carry out requests.**
   This is a safer mode: no phrase spoken in it can steer anything.
5. *(added later)* **Intensive brainstorming with a browser as the search assistant.** Important entities and
   questions are searched in a browser of their own, one tab after another, on another workspace or screen. The
   owner can open any tab, and quotes and pictures from the tabs go straight onto the page. This should be a
   setting of its own. When it is on, the subagents can also work with the tabs: they pick links, illustrations
   and anything important, and propose inserting them.
6. *(added later)* **Ведение конспекта** (keeping a conspect). How it differs is still open, most likely
   conspect-taking tuned for a very good layout and graphs (section 7).
7. *(added later)* **A panel of questions the owner answers with a tap** ("yes", "no", "this one", "that one")
   when the agents face a dilemma. Either a floating panel, or a strip on the left of the terminal area, with
   fixed Yes / No buttons and dynamic buttons the system shows. The right strip already holds the keys for the
   right thumb in tablet mode. It needs a brainstorm of its own, including which modes it belongs in
   (section 9).
8. *(added later)* **Voice control, not annotation.** Nothing is logged and nothing takes room on the page. Each
   piece that comes in is either context or an instruction, depending on what it says. A piece shows for a while
   as a pop-up indicator on the page (AI thinking, with the text it is thinking about) and disappears when the
   command is done. To be added to the extended recording menu alongside the others (section 7a).

He also asked for configuration settings, my own view on which modes are worth having, and how the skills should
be split.

## 2. The idea: modes are presets over a few independent settings

The four or five modes differ along a few independent dimensions, or "axes". If the app models the axes, the modes
become presets: big tiles in the menu, each one a combination of settings. The owner can add his own presets
without anything being rebuilt.

| Axis | Values | What it controls |
|---|---|---|
| **annotate** | `none` · `cards` · `visual` | What is drawn without being asked:<br>• `none`: nothing;<br>• `cards`: today's topic cards and their flow;<br>• `visual`: cards plus schemes, mind maps, pictures and text blocks. |
| **requests** | `off` · `notes` · `drawing` | What a spoken request may change:<br>• `off`: nothing; requests are treated as material;<br>• `notes`: today's safe list, on the live-notes pages only;<br>• `drawing`: also the page being drawn on, including the owner's own ink (section 5). |
| **research** | `off` · `web` · `browser` | How facts and pictures are looked up:<br>• `off`: never;<br>• `web`: the agents' own web search (today);<br>• `browser`: the research browser (section 6). |
| **intensity** | `light` · `normal` · `high` | How hard the agents work: effort, number of workers, how often the coordinator wakes, and how eagerly it starts topics and searches. |
| **style** *(optional)* | `conspect` · `minutes` · `lecture` · `translate` | The kind of notes (sections 7, 10). |
| **layout** | `live` · `composed` | How the page is laid out:<br>• `live`: cards where there is room, as today;<br>• `composed`: a designed page, polished in passes (section 7). |
| **transcript** | `page` · `hidden` | Whether there is a live-notes page with the transcript column, or none: the pieces are kept only in the recording's folder (voice control). |

The requests axis gets one more value for voice control, **`commands`**: every piece is read as a possible
instruction for the open document, not only for the notes (section 7a).

The transcript column is always on. It is written by the app, so it costs nothing and needs no agent.

### Default presets: the menu's tiles

| Tile | annotate | requests | research | intensity | Who runs |
|---|---|---|---|---|---|
| 📝 **Только текст** (transcript) | none | off | off | — | the app alone: no agent, no tokens |
| ✍ **Помощник по рисунку** (drawing helper) | none | drawing | web | normal | live coordinator |
| 🧠 **Конспект + просьбы** (today's mode) | cards | notes | web | normal | live coordinator + workers |
| 🛡 **Конспект без команд** (safe) | visual | off | web | normal | live coordinator + workers |
| 🗒 **Ведение конспекта** | visual, style `conspect`, layout `composed` | notes | web | normal | live coordinator + workers + layout worker |
| 🔎 **Брейншторм с браузером** | visual | notes | browser | high | live coordinator + workers + research browser |
| 🎙 **Голосовое управление** | none, transcript `hidden` | commands | web | normal | voice session (section 7a) |

A small **⚙ Настроить…** tile opens a dialog with the axes as radio buttons and checkboxes. The owner can change a
preset or save a new one. Presets live in the config, so they can also be edited in the AI Agent Settings:

```json
"live_notes": {
  "default_mode": "conspect",
  "modes": [
    {"id": "transcript", "label": "Только текст",          "annotate": "none",   "requests": "off"},
    {"id": "drawing",    "label": "Помощник по рисунку",   "annotate": "none",   "requests": "drawing", "research": "web"},
    {"id": "conspect",   "label": "Конспект + просьбы",    "annotate": "cards",  "requests": "notes",   "research": "web"},
    {"id": "safe",       "label": "Конспект без команд",   "annotate": "visual", "requests": "off",     "research": "web"},
    {"id": "konspekt",   "label": "Ведение конспекта",     "annotate": "visual", "requests": "notes",   "research": "web",
     "style": "conspect", "layout": "composed"},
    {"id": "brainstorm", "label": "Брейншторм с браузером", "annotate": "visual", "requests": "notes",   "research": "browser", "intensity": "high"},
    {"id": "voice",      "label": "Голосовое управление",  "annotate": "none",   "requests": "commands", "transcript": "hidden"}
  ]
}
```

While recording, the recording indicator shows the mode as a chip. Tapping it switches the mode for the rest of
the recording, and the coordinator is told that the mode changed. This is a later step.

### The Record menu: everything with a finger (the owner, 9 Nov, evening)

Every mode the owner listed is a choice in the menu that opens on a long press of Record. Today it has two; it
gets all of them, each easy to tap and clearly labelled:
- voice control is in it;
- ведение конспекта is in it;
- the browser assistant is a **checkbox** in the same menu, and more checkboxes may follow.

The checkboxes are large enough for a finger. Everything works without a keyboard or a mouse.

The design:
- **One tap starts.** Tapping a tile starts that mode at once, as today.
- **The checkboxes come first, at the top** ("for the next recording"). They are remembered, so most of the time
  a recording is one long press and one tap.
- **Tiles in a grid,** like the Tools grid the owner liked: 3 columns of about 150×96 px. Each tile has an icon,
  a name in bold and one short line saying what it does. The last mode used is highlighted.
- **Checkboxes as big toggle chips** (about 56 px tall, ☐/☑ and a label), not small GTK checkboxes.
- **A tile shows which checked options apply to it** as small badges, such as 🔎 on the conspect tiles. Options
  that don't fit a mode don't apply to it: no browser for the plain recording.
- The Record button's short tap stays plain recording.

```
┌──────────────────────────────────────────────────────┐
│ [☑ 🔎 Браузер-помощник]  [☐ ⚡ Интенсивно]  [☐ …]      │  ← big toggle chips, remembered
├──────────────────┬──────────────────┬────────────────┤
│ ⏺  Запись        │ 📝 Текст          │ 🎙 Голосовое    │
│   только звук    │  транскрипт      │   управление   │
│                  │  на странице     │ команды, без   │
│                  │                  │ записи на стр. │
├──────────────────┼──────────────────┼────────────────┤
│ ✍  Просьбы к     │ 🧠 Конспект +     │ 🛡 Конспект     │
│   рисунку        │  просьбы      🔎 │  без команд 🔎 │
├──────────────────┼──────────────────┼────────────────┤
│ 🗒 Ведение        │                  │                │
│   конспекта   🔎 │                  │                │
└──────────────────┴──────────────────┴────────────────┘
```

### The browser assistant's browser (a setting)

The owner: start it in one of the Chrome installs and one of its profiles; without a profile there is no login and
search is worse; this belongs in the settings, either the active browser or an incognito one. What each choice
means:

| Setting | Search | What the app can do | Risk |
|---|---|---|---|
| **Your Chrome, a profile you pick** (logged in) | best: your Google account | open tabs in a new window of the running Chrome, which needs no automation. Reading a tab, or taking a quote or picture, needs the Claude in Chrome extension: Chrome (since 136) refuses remote debugging on its own default profile folder | agents act inside your logged-in profile. Mitigation: they get only read tools (open, read text, take a picture); never clicks, forms or anything else |
| **A separate xournalai profile** (its own folder, you sign in to Google once) | good: signed in, no other accounts | everything through CDP: open, read, grab, without the extension, cheap | small: only that Google sign-in is there |
| **Incognito / guest** | worse: no login, more captchas | everything through CDP, with a temporary folder | none |

Proposed settings, in AI Agent Settings:
- **Browser:** the separate profile / my Chrome (pick the install and the profile) / incognito;
- **Where:** workspace N / a screen / on top.

## 3. Safety: the mode must be enforced by the app, not only described to the agent

Spoken requests are untrusted input. Speech can come from another person or from a video nearby, and the
recognizer can mishear. Today the live-notes coordinator is the companion, the serving session, and it runs in
**bypass** mode. The "never from speech" rule exists only as prose in the skill. A mode called "safe" should be safe
by construction:

1. **Live notes get their own coordinator session,** in a tab like the workers. It is started by the app for the
   recording, with the **permission profile of the mode** (`dontAsk` plus an allow-list):
   - `requests=off` or `notes`: xournalai tools, the inbox helper and, when research is on, web search, fetch and
     the research tool. No Bash beyond the helpers, no files.
   - `requests=drawing`: the same tools. The difference is the write scope (point 2).

   This also leaves the companion free for the owner's own requests (markers, Ask) during a long recording. Today
   live notes keep it busy.
2. **The app checks every write against the session's scope.** Since 1.16.4 the app knows which session a call
   comes from (`X-Xournalai-Worker`). The scopes:
   - live coordinator and workers in `off` / `notes`: writes only on the live-notes pages, plus new pages placed
     right after them;
   - `drawing`: also the page being drawn on;
   - reads are allowed everywhere.

   A misheard or injected "delete my pages" fails with "outside this recording's scope", whatever the prose says.
3. **The prompt carries only what the mode allows.** The skill is split into modules (section 4). The coordinator
   of a `requests=off` recording never reads the module about carrying out requests. To it, requests are simply
   material.

## 4. The skills: one core and modules per axis

The options considered:

| Option | For | Against |
|---|---|---|
| one big skill with "if mode = …" branches | one place | grows with every mode; behaviour leaks between modes (a "safe" coordinator still reads the request rules) |
| a separate skill per mode | each one small | the coordinator loop, layout, workers and ending are copied 4–5 times and drift apart; a new preset needs a new skill |
| **core + modules** *(recommended)* | shared parts once; a mode loads exactly its parts; a new preset needs no new prose | the app must name the modules to read (it does: it starts the coordinator) |

The layout:

```
skills/live-annotation/
  SKILL.md                  core: the loop, the transcript, the layout, workers and zones, the end, don'ts
  modes/annotate-cards.md   topic cards and their flow (today's)
  modes/annotate-visual.md  plus schemes, mind maps, pictures, text blocks
  modes/requests-notes.md   recognising requests + today's safe list (live-notes pages)
  modes/requests-drawing.md requests on the page being drawn on (section 5)
  modes/research-web.md     web search inside workers: sources, licences
  modes/research-browser.md the research browser (section 6)
  modes/style-*.md          conspect, minutes, lecture, translate (sections 7, 10)
  lib/inbox.py, …
```

The app writes the coordinator's brief, its first message, from the preset, in the same way it writes a worker's
brief. The brief contains:
- the mode's settings;
- "read SKILL.md and modes/X.md, Y.md" (only those);
- the scope;
- the recording.

The wake lines name the mode too. Workers get their part of it in their own briefs.

## 5. Drawing mode: requests applied to what the owner is drawing

"Applied to the drawing I'm making" means that **"это / this" refers to what the owner just drew or pointed at.**
The app knows that context and can give it to the coordinator with each piece:
- **The page and region of the owner's latest strokes** at the time of the piece. The change log already records
  user-made strokes with their time.
- **The selection or the lasso,** if there is one. The pen's last position.

A request such as «раскрась это», «подпиши стрелки», «выпрями эту схему» or «нарисуй рядом график» then has a
target. The rules:
- **Additions** go straight onto the owner's page: labels, arrows, colour fills behind his strokes, a graph next
  to the drawing. They are undoable: they go into the shared undo history as agent changes.
- **Changes to his own ink** (restyle, straighten, move, delete) are **proposals.** The agent prepares them in a
  draft layer (transactions already work this way), and the app shows **Apply ✓ / Discard ✗** next to the zone.
  Only a tap applies them, never a spoken "yes", because the speech is the untrusted channel.
- No proactive cards in this mode. The transcript column stays on its live page, and the owner's page gets only
  what was asked for.

## 6. The research browser: brainstorming with a browser as the search assistant

The goal: the browser does the searching, not Claude. Claude only decides what to search, and reads a page only
when a quote or a picture is wanted. The owner sees the tabs, can open one, and quotes and pictures come onto the
page with their source.

### Recommendation: the app's own browser instance, driven through Chrome DevTools Protocol (CDP)
- **Its own profile in the app's folder** (`~/.local/share/xournalai/research-browser`). Chrome or Chromium starts
  with `--user-data-dir` there and a local `--remote-debugging-port`.
  - It is never the owner's own profile: no logins, cookies or accounts that a spoken or web-injected instruction
    could misuse.
  - Nothing outside the app's folders changes.
- **Where:** a setting.
  - With one screen, the window is moved to another workspace (`workspace: 2`) through the window manager's
    standard hints, so the owner switches to it with a gesture.
  - With several screens, it goes to a named monitor.
  - `behind` keeps it under the app.
- **The app tool `research`:**
  - `open(query, kind = web | images | wiki | scholar)` opens the search in a new tab, titled with the topic;
  - `list` shows the tabs with their topics and what each found;
  - `read(tab)` returns a text excerpt and the list of images;
  - `grab(tab, quote | image, …)` places a quote card or a picture on the page with its source line (licence
    rules as today).

  The tool is read-only: no forms, logins, downloads or purchases.
- **The owner's own grabs:** he selects text in a research tab and taps **📌 Вставить выделенное** on the
  recording bar. The app reads the selection through CDP and puts a quote card with the URL on the live page.
  Images already work through copy and paste.
- **What the coordinator does with it** (module `research-browser.md`):
  - every new named entity, unfamiliar term or open question gets a search tab (deduplicated, at most N a minute,
    the oldest closed beyond M tabs);
  - the topic's card gets a small "🔎 3 вкладки" mark;
  - only the best hits are read and placed.

**Alternative: the Claude in Chrome extension in the owner's own Chrome.** The owner mentioned this one. It needs
no second browser and uses the owner's familiar tabs. But the agent would then act in his logged-in profile, steered
by speech and by web pages. I would keep that for the owner's own requests typed in the terminal, not for speech.
The CDP instance costs about the same at run time, because the browser does the work either way.

### Subagents in the tabs
When the research browser is on, research workers get the `research` tool too. A worker:
- reads the tabs of its topic (`read`);
- picks the best links, illustrations and quotes;
- **proposes** each one as a question on the decision panel (section 9): a thumbnail or the quote, plus
  "вставить?". A tap inserts it with its source line.

Nothing a worker finds goes onto the page unasked, except text facts with their source, as today. This keeps
pictures under the owner's control. It is also cheap: a worker reads one tab's excerpt, not the web.

## 7. 🗒 Ведение конспекта: a conspect with a really good layout

What sets it apart is **layout quality** and **graphs**. The aim is a page that could be printed, not a board of
cards placed wherever there was room. What I would make different:
- **A page plan first.** As the first topics appear, a **layout worker** plans the page: a grid of sections,
  columns, where the main scheme goes, and room left for graphs. Topic workers draw *into the plan's slots*, not
  wherever there is space.
- **The layout library** of the konspekt-magazine skill. It provides wrapped text with the app's exact font
  metrics, LaTeX sizing, tables, timelines and card styles. With it, text never overflows and spacing is
  consistent.
- **Graphs from real numbers** (matplotlib to SVG to strokes), marked with their source or as estimates. Formulas
  in LaTeX. Diagrams as editable strokes.
- **Passes.** While the talk runs, the live draft is the cards. Every ~10 minutes, and at the end, the layout
  worker composes a **clean page** from them on a new page after the live ones; the draft stays. A light version
  is to re-compose only at the end.
- **Optional: the owner's handwriting style** (the xournal-conspect skill: his glyphs, code boxes, formula clouds)
  for the clean page.
- Requests are `notes` (a conspect is still steered by "добавь график…"). Research is `web`, or `browser` for a
  brainstorm-conspect.

This preset is mostly prose (a `style-conspect.md` module plus the layout library) and one more worker role. The
app only needs the presets. Questions about it: see section 12.

## 7a. 🎙 Голосовое управление: voice control instead of notes

Here the speech is not material for notes but **the way the owner works the app**. Nothing goes on the page:
- no live-notes page;
- no transcript column;
- no cards.

Each piece is sorted:
- **context** («вот здесь у нас формула Шрёдингера…»): kept by the session, so that a later «это», «там» or
  «её» has something to point to;
- **an instruction** («выдели это красным», «добавь страницу с сеткой», «нарисуй рядом график синуса»,
  «переведи этот абзац»): carried out on the open document.

What the owner sees:
- each piece pops up for a few seconds as a small indicator, «🎙 …the heard text», near the recording indicator,
  and fades;
- when a piece is taken as an instruction, a thinking zone opens over the area it acts on, with the instruction
  as its status; it ends with ✓ (or ✗ and the reason) and fades;
- no zone means the piece was taken as context.

How it differs from the other modes, and what it needs:
- **Scope.** Speech is untrusted in every other mode. Here it is the owner's command channel, so speech may do what
  he could do by hand **in the open document**: draw, edit, select, recolour, add pages, change tools and layers,
  navigate. It still cannot reach anything **outside** the document (files, other documents, mail, the shell or
  settings).
- **Destructive commands need a tap.** Deleting pages or content, replacing or clearing the owner's ink, and
  undoing a lot all go through the decision panel (**Да / Нет**, section 9). A misheard word, or someone else in
  the room, can then never destroy anything.
- **Who is talking to it.** Every piece may be an instruction, so other people's speech is the main risk. Two
  ways to deal with it:
  - a **wake word** («Клод, …»): only phrases that start with it are instructions; everything else is context;
  - **push-to-talk**: hold the Record tile, or a key, while speaking a command (the Ask popover already has "Hold
    to say").

  I would offer both: push-to-talk by default, the wake word as a setting for hands-free work.
- **Who runs it.** The instructions target the owner's own pages, so it is like Ask by voice. It should be a voice
  session of its own with the document scope (xournalai tools only, no shell), not the companion in bypass mode.
  For speed it starts with the recording and stays warm. One command at a time, in order; a new instruction
  while one runs is queued and shown as "next: …".
- **Cost and speed.** No drawing of notes, so only the commands cost tokens. Context pieces are only read, and
  short commands take the speed path of the skill (one call, no plan).

It sits on the same tile row as the other presets. The settings dialog offers voice control as one more preset
(`requests: commands`, `transcript: hidden`).

## 8. Where the decision panel fits

The decision panel (section 9) is the **trusted channel** this design was missing. Speech is untrusted; a tap is
the owner's own act. So the same panel serves:
- ✍ drawing mode: **Apply ✓ / Discard ✗** for changes to the owner's own ink;
- requests the coordinator is unsure about (today a `⚡?` tag on a card): **"Это просьба? Да / Нет"**;
- 🔎 brainstorm: workers' proposals, such as "вставить эту картинку?" with a thumbnail, or "which of these 3?";
- 🗒 conspect: occasional layout choices ("a mind map or a timeline for this part?");
- 🛡 safe mode: questions are allowed, but the answers can only choose **within** the mode's scope. A tap never
  widens what speech may do.

Outside live notes, it would also replace "confirm in the terminal" for markers and Ask.

## 9. The decision panel: questions answered with a tap

### Where it lives
The AI terminal docks at the bottom of the window. Its right strip holds the keys (Space, Enter, …) under the
**right thumb**. The panel takes its **left strip, under the left thumb.** That is where a hand rests when holding
the tablet, and while the right hand holds the pen the left thumb is free. So:
- **With the terminal open:** a strip on the left of the terminal area, about as wide as the key strip.
- **With the terminal closed** (focus mode, full canvas): **the same corner**, bottom left, as a small floating
  chip "❓ 2". It expands into the same buttons when a question arrives. Same place either way, so the thumb learns
  it.
- A question about an area of the page also **highlights it**, the way a thinking zone does. For "this or that",
  each option's area gets its letter on the canvas (A, B), and the buttons say A and B.

### What it shows
- **Two fixed buttons, Да / Нет,** always in the same place and large. They answer the current yes/no question.
  They are dimmed when nothing is asked.
- **Dynamic buttons** above them when a question has options: "этот / тот", A / B / C, thumbnails for pictures,
  and "пропустить".
- **The question:** one line of text, which agent asks (⚙ T4 or the coordinator), and a countdown to its
  default.
- A **queue counter** when several are waiting. The oldest is shown first; a swipe or a tap on the counter shows
  the next.

### How agents use it: a tool, never blocking
`ask_owner(op="ask", question, options=[…], default, timeout_s, zone|region)` returns an id at once. The agent goes
on with other work. The answer arrives:
- for the serving session or a live coordinator, as a wake line or in its inbox;
- for a worker, in its inbox (workers get no wake-ups).

When the timeout passes, the **default** applies. A live talk doesn't wait: "no answer in 60 s → skip". The
default is chosen so that silence is safe (skip, keep, don't insert).
- `op="cancel"` withdraws a question that is no longer relevant (the topic moved on).
- At most a few open questions per recording, and only one per worker at a time, so the owner isn't spammed.
- Questions expire when the recording ends.
- Answers by tap (or a key) only. A spoken "да" never answers a question, because speech is the untrusted
  channel.

### Which modes get it
- needed in ✍ drawing mode (Apply / Discard);
- useful in 🔎 brainstorm (proposals) and 🧠 / 🗒 (unsure requests, layout choices);
- 🛡 safe mode: on, with answers limited to the scope;
- 📝 transcript-only: off, nothing asks;
- a setting `questions: off | on`, on by default where an agent runs.

## 10. Other modes I would consider

- **Minutes (протокол)** for calls and meetings: decisions, action items with owners and dates, and open
  questions, collected as they come; a minutes card at the end. A *style*, combinable with any preset.
- **Lecture**: definitions, formulas in LaTeX, diagrams, a glossary and review questions at the end. A style.
- **Translate**: a translation under each transcript piece, for a talk in a foreign language. It builds on the
  language-from-audio work. A style, or a checkbox.
- **End summary only**: no drawing while recording; a summary page when it ends. One session at the end instead of
  one for the whole recording, so the cheapest mode after the transcript. It could be a checkbox on the
  transcript-only tile.
- **A trigger word for requests** (the skill already leaves room for one): in `notes` or `drawing`, only phrases
  that start with it count as requests. Fewer false positives, and another person cannot steer the notes by
  accident.

## 11. Plan, if the owner agrees

| Step | What | Size |
|---|---|---|
| 1 | Presets in the config, tiles in the Record menu, the mode in the wake lines; 📝 transcript-only (no agent); the skill split into a core plus modules (cards, notes requests, web) | medium |
| 2 | The live coordinator in its own tab with the mode's permission profile; the app's write scope per session; 🛡 safe mode on top of it | medium |
| 3 | ✍ drawing mode: the owner's latest strokes, selection and lasso as context; proposals with Apply / Discard | medium-large |
| 3a | The decision panel: bottom-left strip in the terminal, the chip when the terminal is closed, `ask_owner`, answers delivered to sessions and inboxes; then Apply / Discard in drawing mode | medium |
| 4 | 🔎 research browser: own profile, workspace or monitor placement, CDP, the `research` tool, "📌 insert selection", workers' proposals through the panel; the brainstorm preset | large |
| 4a | 🗒 Ведение конспекта: the `style-conspect` module, the layout worker, passes onto a clean page, graphs | medium |
| 4b | 🎙 Голосовое управление: no live page, pop-up pieces, a voice session with the document scope, Да/Нет for destructive commands, push-to-talk and the wake word | medium |
| 5 | The customisation dialog; switching the mode during a recording; styles (minutes, lecture, translate) | medium |

## 12. Questions for the owner

1. 📝 **Только текст:** the transcript only, with no agent at all? Or with an optional summary at the end?
2. ✍ **Drawing mode:** may additions (labels, arrows, colour behind your strokes) go on your page directly, with
   undo? Should changes to your own ink always wait for an Apply tap?
3. **A live coordinator of its own,** instead of the companion in bypass mode: agreed? This is what makes the safe
   mode really safe, and it keeps the companion free during recordings.
4. **Research browser:** a separate Chrome profile in the app's folder (recommended), or your Chrome with the
   extension? Where should it go: workspace 2, or one of your three screens?
5. **Brainstorm intensity:** how many tabs a minute at most, and how many kept open?
6. **Which extras:** minutes, lecture, translate, end summary only, a trigger word?
7. 🗒 **Ведение конспекта:** a clean page composed in passes (every ~10 min and at the end), or only at the end?
   Your handwriting style for it, or a typeset one?
8. **The decision panel:** the bottom-left corner (in the terminal when it is open, a chip when it is closed):
   does that match how you hold the tablet? What should silence mean by default: skip, or the first option?
9. 🎙 **Voice control:** push-to-talk, a wake word, or both? Which commands should always ask for a tap?

## 13. A principle for all of this: the app does the bookkeeping, the agents the judgement

The owner asked (about the busy zones of 1.16.4) whether the companion should keep such things up, or the
application itself should know what it delegated. The answer, and the rule I would apply to every mode:

| | The app itself (the binary) | The agent (prose in a skill) |
|---|---|---|
| **Stability** | deterministic: it happens every time; nothing is left behind when a worker crashes or the coordinator forgets | depends on the model following the prose every time; placeholders are ink, so they get saved, undone and left over |
| **Speed** | instant, no tokens, no waiting for an agent's turn | 2–4 tool calls per worker, inside turns that are busy with the talk |
| **Quality** | knows only events: started, ended, wrote here | knows meaning: what the worker does, which topic, which colour |

So: **the app owns the lifecycle; the agent supplies the meaning.** For busy zones (1.16.4):
- the app opens the zone when the worker is started, keeps it through the coordinator's turns, and ends it when the
  worker ends it, is closed or its process stops;
- the agent gives the region, the colour and the status text, and says when the job is really done.

What the app could take over next:
- an **elapsed time** on the zone's label (⚙ T4 · 3:12);
- **"drawing…"** as soon as the worker's first stroke lands in its region (the app sees the worker's calls);
- the worker's **busy or idle** state from its hooks. Today they are ignored, so that they are not taken for the
  serving session's. Then a zone whose worker has gone idle without ending it shows "waiting", not "thinking".

The same rule shapes the modes:
- the app does the transcript, chunking, pages, the scope checks, the decision panel and the research browser's
  tabs;
- the agents do what needs understanding: topics, requests, layout, what to search and what to propose.
