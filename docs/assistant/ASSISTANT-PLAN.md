# xournalai Assistant: an embedded AI, brainstorm and plan

Status: **proposal for discussion, rev 17: Milestone 1 planned (epoch E6 on the Kanban board)** (2026-09-29). Nothing here is implemented yet. The MCP server (1.x) stays as it
is; this document is about what to build *on top of it*.

---

## ★ What you actually do (read from your live canvas session)
Source: the canvas session `~/.claude/projects/-home-olek/085dba93…`, about 116 messages, 04:30 to now.

1. **Watching and assisting is the core loop** ("keep watching"):
   - formulas → LaTeX (including later edits);
   - text rewritten in your v3 handwriting with spelling fixed, plus small tips;
   - diagrams redrawn accurately;
   - a consistent colour palette; "?" placeholders greyed.
2. **Your marker language:**
   - `*!` improve my strokes (same pose and size);
   - `**!` a professional illustration;
   - `*w!` a web summary;
   - `*c!` a command;
   - `*r!` a real image (a PD/CC0 photo, or generated).

   Markers are removed after being acted on.
3. **Your sharpest feedback:** "I didn't ask you to redraw everything. I just needed you to improve my strokes… an
   assisted mode, not a complete redo without asking", and **"I need much quicker reaction."**
4. **Division of labour, already in use:** the peer (`andrew-proj-fd`) researches, verifies against code and gcloud,
   and updates and pushes the docs only on your confirmation; the canvas session renders and watches.
5. **What drawings became:** 11 architecture and data-flow schemes, pushed into the broker-fleet docs
   (0.9.49–0.9.52); a conspect redrawn from paper; the handwriting fonts v1 → v3; Einstein; a butterfly; a pigeon
   photo; a mind map.
6. **What hurt:**
   - slow reactions and over-eager redraws;
   - the context ran out twice;
   - connection drops and the invisible pause;
   - exports missing items;
   - SVG and result size limits;
   - stale facts (caught by the peer);
   - a second app window.
7. **How you talk to it:** long dictated prompts (voice is already in your workflow); "keep watching", "go";
   commands written on the canvas.

What this changes:
- **An assist contract:** minimal, in place, fast. Keep position, size, pose and layout; improve your strokes,
  don't replace the drawing; never add unasked. Bigger changes only with `**!` or a question.
- **Speed, your targets (decided):**
  - **small pieces** (a few words, a simple shape, a formula): a few seconds is fine;
  - **big tasks** (`*c!`, `**!`, diagrams from a peer): may take long;
  - **never silently.**

  Instant geometric touch-ups in the app are a bonus.
- **Auto-improve toggle (decided):** on = improve everything you write, unasked (formulas, text in your
  handwriting, diagrams, colours); off = only markers and commands. It's a toolbar button with a shortcut and a
  per-rule dropdown, and the mode shows in the status strip.
- **Visible thinking (first-class):** the zone is highlighted (an animated outline), with a thinking icon and a
  one-line status ("reading formula…", "asking andrew-proj-fd…", "drawing 3/5"). States: queued → thinking →
  drawing → done / failed. Tap to cancel or see details; a count in the status strip.
- **Markers are the command grammar,** built in and extendable. The app finds candidates cheaply; the fast model
  reads the tiny crop; the companion is woken; the marker is removed when done.
- **Context that lasts all day:** images out of the context by default, the canvas state in notes, an automatic
  rollover to a fresh session before the limit.
- **Delegation as a default:** the companion renders and watches, peers supply facts and docs; peer messages are
  data and never permissions.
- **Drawings into docs, correctly:** "publish this page to the project docs" exports from the app's renderer; the
  peer commits on your confirmation.

## 0. Scope: a Linux prototype on your subscriptions

> **What xournalai is for you: a shared whiteboard between you, your Claude sessions and your system.**
> ① **Assisted drawing (phase 1, tested first):** a Claude session helps you draw.
> ② **Sessions and your system draw for you:** a board starts already connected ("draw the architecture of the
> project in this folder", "learn my system and plot CPU and memory, capacity and current use, hand-drawn").
> ③ **The drawing as a launchpad:** the page starts or feeds work in other sessions.
> The flows mix freely: a diagram a session drew (②), refined with help (①), becomes the next task (③).

**Now:** a complete, stable xournalai on your Linux laptop, for daily use and for pitching. Installed with one
command, no terminal window. The assistant runs on your **Claude or Codex subscription** through the locally
installed Claude Code, Codex or OpenCode (ACP). No API keys, no servers of ours.

**Later, not in scope:** a public endpoint, the built-in API runner, Android, macOS, public packaging. The
architecture keeps those doors open, and nothing is built for them now.

What this changes:
- The runner is ACP only (local CLIs).
- Installing is part of the product: one script, plus adapters in a private folder the app manages, plus a setup
  check.
- Stability comes before features (5.9). Usage is yours to judge (Max plan); the app only shows what it spends.
- Android and other platforms: a separate, later iteration with its own redesign.
- Voice A uses local whisper.cpp only.
- No terminal drawer in the prototype.

**Settled:** this is your personal environment: your subscription, your laptop, your custom configuration.
Several Claude Code sessions run side by side over the local socket bus; xournalai joins it (5.10).

Architecture that keeps the doors open:
- The **assistant core** is a GTK-free C++ library: intents, action library, work units, runner interface, ACP
  client, prompts.
- The **runner protocol** is serialisable, so it can later run remotely.
- **MCP stays the tool surface.**

## 0.1 The scenario: how you'll use it
1. **Think with a pen:** open xournalai and draw to brainstorm.
2. **Assistant on:** it watches (focused: fresh pieces and zones) and helps: tidier strokes, clean shapes,
   colours, drawing what's tedious by hand, improving pictures.
3. **Ask as you go:** "help with this schema", "colorize this", "draw X here", "find a photo of X and put it here"
   (web search plus insert), "generate an illustration of Y" (image generation). You keep drawing while it works.
4. **It talks to your other sessions.** The project session (CLAUDE.md, skills, work in progress) gives context
   ("which modules talk to the broker?"), and the drawing session draws it. The project session can push to the
   canvas too. What you draw becomes a task for it, with the picture attached.
5. **Connected from the start:** a board linked to a project session or folder ("draw the system architecture of
   the project in ~/workspace/andrew_proj"), or to your system ("learn my laptop and draw CPU and memory as
   hand-drawn charts"). The result is a drawing you refine and talk about (0.5).
6. **From drawing to action (the launchpad):** a finished drawing seeds work ("start implementing this", "research
   this", "tinker", "continue with andrew-proj-fd"). The receiver gets the picture *and its meaning*, and can report
   back onto the page (0.4).
7. **Division of labour:** the drawing session knows the canvas; project sessions know their code. Each keeps the
   full power of Claude Code.

**Consequence:** the assistant is a **real, full Claude Code session**, embedded and hidden but complete (tools,
skills, web, MCP, subagents, peer messaging), specialised for drawing through its workspace.

## 0.2 Anatomy of the assistant session
- **Workspace:** `~/.local/share/xournalai/assistant/` (or per document), holding its CLAUDE.md, drawing skills,
  action library, and an exchange folder for images shared with other sessions.
- **Tools:** xournalai MCP (per-window socket) plus everything your Claude Code has (web, files, bash, subagents,
  peers, whatever image generation you've wired in: Nano Banana, an Anthropic tool, …). xournalai just brings the
  images onto the canvas well.
- **One session with helpers inside:** the main session (strong model) holds the conversation; a fast
  **canvas-watcher** helper agent processes the fresh pieces. This replaces "two sessions per document".
- **A peer on the bus** named `xournalai-<document>`; pictures travel as files in the exchange folder.
- **Hidden but yours:** "Open session in terminal" runs `claude --resume <id>` in your terminal. It replaces the
  VTE drawer.
- **Lifecycle:** starts with AI mode or the first request, lives while the document is open, resumes next time;
  a pause stops it at once.

Where I push back:
- **"Sees what I'm doing" must be a cheap, focused eye.** The fast helper watches the pieces and passes up only
  meaningful events. The main session isn't flooded with ink.
- **Talking to other sessions takes time and is asynchronous.** Never block: the panel shows "asked
  andrew-proj-fd, waiting", and answers arrive as notes or drafts.

## 0.3 Lessons from your canvas session (Einstein, conspects, fleet docs)
The session's record: 149 waits for your edits, 116 SVG imports, 85 renders, 45 LaTeX formulas, 64 deletions.

| What happened | What it means for the design |
|---|---|
| 104 tool results too big for the agent (an SVG import answering with ~200k chars) | **Compact answers by default** (count, operation, bbox; ids on request or to a file). Quick win. |
| An SVG over the 5 MB limit, split by hand | **Big drawings by path**, streamed and chunked, no local ceiling. Quick win. |
| Its scripts wanted direct canvas access, with no sanctioned way | **Canvas SDK** (Python plus CLI) over the per-window socket, owner-only, no token. |
| Procedural drawing couldn't reach a likeness of Einstein; tracing a photo could | **`trace_image`**: any picture → native pressure strokes (line-art / pencil / contour). |
| Your handwriting captured into a glyph library inside the skill | **"My handwriting" as a first-class feature** plus `write_handwritten` for every session. |
| Line grouping went wrong once (heading and next line mixed; tilted baseline) | Robust line and word grouping in the app's layout analysis. |
| Your markers `XC!` (revise) and `*r!` (generate an image) | **Markers are first-class** in the action library, found cheaply; `*r!` uses the session's own image tool. |
| Erasing left orphaned tips | Annotations bound to their targets. |
| The peer caught a factual mistake before publishing | Validates the bus; **"verify with project session"** as a step. |
| An export left out a separately drawn sketch | Exports and "send page" always come from the app's renderer. |
| Connection drops; the invisible pause | Fixed already. |

## 0.4 The launchpad: from a drawing to sessions (phase 2)
- **Start a new session from the page:** templates (implementation, research, tinker, custom), a folder (new or
  existing project); Claude Code starts there **in a normal terminal window**, joins the bus, linked to the page.
- **Continue with a running session:** page or region plus an instruction to a peer; replies as notes or drafts.
- **The handoff package** (built by the drawing session): the page image from the app's renderer, transcribed
  handwriting, **diagram structure as a graph**, LaTeX, notes memory, open questions, a link back.
- **The round trip:** linked sessions write progress marks and findings back; the drawing becomes a live map of
  the work.

Where I push back:
- **A picture alone is a weak spec.** The value is the structured meaning (graph, transcription, marked
  ambiguities), so `diagram_structure` deserves to be a real tool.
- **Guard rails for spawning:** a folder, your usual permissions, a clear first prompt, visible in a terminal,
  never a silent background session with write access.

## 0.5 Sessions and your system draw for you
Knowledge from sessions, projects and your machine comes onto the canvas, in your hand-drawn style (as with the
fleet pages built from `andrew-proj-fd`'s answers).
- **Linked boards:** "New board linked to…" a session or folder; the link is shown; requests go there; it may draw.
- **A shared diagram description** (JSON: nodes, groups, edges, labels, notes): sessions send structure, not
  prose. `draw_diagram(description, style)` lays it out and draws it; `diagram_structure` turns ink back into
  the same format.
- **`draw_chart(data, kind, style: hand | clean)`:** bars, lines, pies, gauges, tables, labels in your
  handwriting. The system inspection itself is Claude Code's own tools.
- **Refreshable charts:** the data source is kept in notes; Refresh redraws in place. Snapshots by default.

Where I push back:
- **Don't make the drawing session the project expert:** it asks the session that knows, and only draws.
- **Live dashboards are scope creep:** snapshots plus Refresh.
- **Layout is the hard part:** real graph layout goes into the app (`draw_diagram`). The model decides *what* to
  show, the app decides *where*.

## 0.6 The companion session: how the assistant runs (revised)
**Defaults:** starting xournalai starts its own Claude Code session (the companion). Other sessions are optional;
the companion connects to them when asked. With AI off, xournalai is a plain drawing app.

Three stages of hosting, cheapest first:
1. **Companion terminal (auto-launched):** a terminal window with Claude Code in the assistant workspace, MCP
   connected to this window, skills and CLAUDE.md loaded, a first prompt. A real interactive session: a bus peer,
   with voice mode and `/mcp`. Days of work; your proven setup.
2. **Hideable terminal dock:** the same companion in a VTE panel inside xournalai (show or hide; keeps running
   hidden), with tabs for Claude, plus Codex, OpenCode or a shell on demand.
3. **Structured runner (ACP):** only if stages 1–2 hit a real limit.

How in-app controls reach a terminal session:
- **Intents over MCP** (always): the companion takes intents; previews, undo and cancel are the app's.
- **A push over the socket bus** (if S5 allows): the app posts intents as bus messages that wake an idle session,
  with no polling and the terminal free.
- **Fallback:** a waiting loop (as today); Esc to talk.

Revision of the critique in section 2: the "terminal is wrong" argument assumed buttons would drive the TUI. With
intents they don't, so a terminal-hosted Claude Code is a fine runner, and the quickest.

Where I push back:
- **The companion must be supervised:** its state in the status strip, "Restart companion" (`--resume`), one per
  window.
- **Pre-allow xournalai's tools** in the workspace settings, so the companion doesn't stop to ask; dangerous
  things still ask.
- **Start policy:** with the app, when AI is switched on, or manually (about 300 MB and a few seconds).

## 0.7 After drawing: what happens with a board
- **Save and export:** .xopp, PDF, PNG or SVG, to a folder you name.
- **Build a presentation:** pages become slides (cleaned up, with speaker notes from the notes memory), as PDF,
  HTML or PPTX, through the companion's tools.
- **Share with a session:** a running peer or a new one (0.4).
- **Keep drawing:** AI on or off; switching it off loses nothing, and it resumes with its context.

## 0.8 Keeping the companion awake: the app watches, the session works
**The problem:** the session should watch all the time, but after a while with nothing to do it stops. That's
structural: watching is a loop of waits inside one turn, the turn ends (the skill even says "stop after ~2 empty
timeouts"), and nothing wakes it until you type.

**The fix: invert it.** xournalai is the watcher (it sees every stroke for free and already groups work units,
markers, AI-pen commands, watch zones). On a *meaningful* event it **wakes** the idle companion with a short message;
the session handles it and goes idle again. Idle is normal, and it's cheaper.

- **State via Claude Code hooks** (SessionStart, UserPromptSubmit, Stop, Notification) reporting busy / idle /
  waiting for permission to xournalai's socket. Events are queued while it's busy and delivered when it's idle.
- **Wake-up channels:**
  - **terminal input** (tmux in stage 1, the VTE dock in stage 2): a line typed only when idle and the input line is
    empty. Public behaviour, works today;
  - **a socket-bus message** (native, but an unpublished protocol: S5);
  - **the Stop-hook keep-alive** (blocks stopping in watch mode): last resort, burns turns.
- **Dependable:**
  - coalesced events (one wake-up with a count and a cursor);
  - a watchdog (resend once after ~20 s, then "companion not responding" plus Restart with `--resume`);
  - no wake-ups when AI is off or paused;
  - the skill drops its "stop after timeouts" rule.

Pushback: a session that's always inside a turn, looping on waits, is fragile and expensive (it holds the turn,
re-reads context every timeout, and dies silently). Waking on events gives the same experience with fewer tokens
and a visible state.

## 1. Where we started (the first version of the idea; sections 0–0.3 are the current state)

- Today you run a separate agent session (Claude Code) next to xournalai. It watches the document through MCP,
  recognises commands you write by hand (e.g. `XC! revise …`) and edits the page. It works well, but it is two
  windows, a manual `/mcp` dance when things drop, and it is only usable by someone who lives in a terminal.
- You want xournalai to **carry its own assistant**: you start xournalai, turn the assistant on, and it can do
  everything the external session does. It should be driven by **in-app controls**: an AI toolbox, "assistant
  mode", buttons that act on a selected region, the command-writing you already use. A hidden or collapsible
  terminal running Claude / Codex / OpenCode inside the app is acceptable to you.
- External agents over MCP must keep working exactly as now.
- Personal use first, public later.

## 2. Critique: where the idea is weak

I agree with the goal. I disagree with some of the means you suggested.

**2.1 An embedded terminal is the wrong primary interface.** It is the fastest thing to ship, and it is fine as a
power-user drawer. As *the* integration it fails on four counts:
- **Buttons can't drive a TUI.** A toolbar button that must type text into Claude Code's prompt, and then work
  out from terminal bytes when the agent is done or what it changed, is fragile and differs for every CLI.
- **No structured feedback.** The app can't show progress on the canvas, map a result to an undo step, or offer
  accept/reject, because it only sees ANSI output.
- **The wrong harness.** Coding agents carry large coding system prompts and shell, file and web tools, and you run
  them with `bypass permissions`. A note app would be launching a fully privileged software-engineering agent
  every time you want a formula typeset. That is slow, token-heavy and hard to justify for a public release.
- **Portability.** VTE is Linux-only in practice, which rules out Windows and macOS users.

**2.2 "Watching and recognising commands from handwriting" costs too much if the model does the watching.** In
the canvas session every idle pause means a full agent turn: a page image, element dumps, reasoning. That is 5 to
60 s and real money per pause, and most pauses contain no command. The model is also *guessing* whether ink is
a command (`XC!`). Detecting commands must be **cheap and deterministic**. Only executing them should involve
the model.

**2.3 "A session per document" is right for context but wrong for lifecycle** if it means a process per open file.
Agent processes are heavy (Node, 200 to 400 MB each). Better: at most one agent process per window, started on
first use, stopped after an idle timeout, and resumable.

**2.4 A risk to the public release: most people don't have Claude Code or Codex installed.** Building everything on
external CLIs serves you, not the public. At some point a built-in, API-key-based backend (or a local model) is
needed. The architecture has to make that a backend swap, not a rewrite.

**2.5 Reusing the terminal CLIs' voice mode is a trap.** Claude Code's (and other CLIs') voice input lives inside
their text interface. It works only in a visible terminal, can't be driven through ACP, and doesn't know what you
selected, where the pen is, or what you just drew. "Make *these* lines bolder" works only if speech is captured
*in the app* together with the selection and pointer at that moment. Voice must be an app-side intent source. The
CLI's voice mode stays available when you open the session in a terminal.

**2.6 An agent is often the wrong tool.** "Make this formula LaTeX", "beautify these lines" and "colour this diagram"
are single-shot transformations with a known input (the selection) and a known output shape. They need a model
call and a preview, not an autonomous multi-turn agent. Agents shine for open-ended work ("revise the whole page
for correctness and update the fleet docs"). Build for both, and don't force the first through the second.

## 3. Options considered

| # | Option | Good | Bad | Verdict |
|---|---|---|---|---|
| A | **Embedded terminal** (VTE) running `claude` / `codex` / `opencode` with xournalai MCP preconfigured | Days of work; uses your subscriptions; full power | No structured control, fragile buttons, Linux-only, privileged coding agent, poor public UX | **Keep, as an optional "Agent terminal" drawer** |
| B | **ACP client inside xournalai** driving any ACP agent (Gemini `--acp`, `opencode acp`, Claude Code via `claude-code-acp`, Codex via `codex-acp`) | Agent-agnostic by standard; structured stream (messages, tool calls, plans, permission requests, cancel); we pass our MCP server to the agent in `session/new`; uses your subscriptions | Needs Node plus adapters for Claude and Codex; adapters are young; coding-agent overhead remains | **Primary runner for v1.x** |
| C | **Built-in agent loop** calling model APIs directly (Anthropic / OpenAI / Ollama) with our tools in-process | No external installs; fastest and cheapest per action; full control of prompts, caching, previews; works on every OS | We own the loop (tool calling, context, caching, retries); API keys instead of subscriptions | **Second runner, for public 2.0** |
| D | **Companion app** (a separate Python or web process) as the assistant, with xournalai staying thin | Fast iteration in Python; crash isolation | Two windows again, which is what you want to escape; in-app buttons still need a channel | Rejected as the product; the *idea* of a thin app and a separate brain survives in section 4 |
| V1 | **Reuse the CLI's voice mode** (terminal drawer) | Zero work; multilingual | Terminal-only; no selection or pointer context; not drivable through ACP | Fallback only |
| V2 | **App-side voice layer** (push-to-talk or VAD → STT → intent with a context snapshot) | Knows selection, pointer, page, recent strokes; any runner; local STT possible (whisper.cpp) | We own capture, VAD and the STT integration | **Recommended** |
| V3 | **Realtime speech APIs** (OpenAI Realtime, Gemini Live) calling our tools | Sub-second, spoken replies, barge-in | Vendor-specific, costly while listening, cloud only | Later, as a "live" runner |
| E | **Headless CLI modes** (`claude -p --input-format stream-json`, `codex app-server`, `opencode serve`) | Structured, no adapters | Every CLI has a different protocol, so we'd maintain N integrations | Rejected: ACP gives the same thing through one protocol |

## 4. The proposal: intents first, then runners

The key design decision: **the in-app controls don't talk to an agent. They create *intents*.** An intent is a
small structured request: *"beautify this selection"*, *"execute the command written with the AI pen here"*,
*"summarize page 3"*. Intents are published through the MCP server we already have. Whoever is connected serves
them:

```
 AI pen / lasso / toolbar / prompt box / watch rules
                    │  creates
                    ▼
            ┌──────────────┐   MCP: intents_next / intent_update / intent_done
            │ Intent queue │◄──────────────────────────────────────────────┐
            └──────┬───────┘                                                │
                   │ progress, drafts, accept/reject, undo (in-app UX)      │
                   ▼                                                        │
     ┌───────────────────────────┐         ┌────────────────────────────────┴──┐
     │ Embedded runner (in-app)  │   or    │ Any external agent (today's Claude │
     │  B: ACP agent             │         │ session, Codex, your scripts)      │
     │  C: built-in API loop     │         └────────────────────────────────────┘
     └───────────────────────────┘
```

Why this is the right cut:
- **It pays off immediately, with no embedded agent at all.** Your current external canvas session gains a toolbox:
  lasso + "LaTeXify" puts an intent on the queue, the session picks it up, and the app shows progress and a preview.
  The fragile "watch everything and guess" loop becomes "wait for the next intent".
- **One UX for all backends.** Preview, accept/reject, undo and cancel live in the app, not in the agent.
- **The runner is replaceable.** ACP today, a built-in API loop for public users, a local model tomorrow.
- **It reuses what exists:** Drafts (hidden layers), EventHub, `wait_for_user`, pause, permission tiers, notes.

## 5. Feature set

### 5.1 AI toolbox (a toolbar group plus the "AI Agent" menu)
- **Assistant switch:** Off / On request / Co-pilot (watch), with a status light (idle, thinking, drafting,
  waiting for you).
- **AI pen:** a dedicated pen (its own colour, e.g. violet, dashed). Whatever you write with it is a *command*,
  never content: "make this a table", "check the math", an arrow to a region plus "explain". When you lift the
  pen and pause, it becomes an intent: the command ink plus the region it points at or encircles. After
  execution the command ink fades into a small, collapsible note: history, not clutter.
  It sits next to your markers (`XC!`, `*r!`, …), which stay first-class (0.3); the pen is simply the unambiguous, free-to-detect way.
- **AI lasso:** select a region, and a popover appears at the selection with actions (5.2). A keyboard or stylus
  button opens it on an existing selection.
- **Prompt box** (Ctrl+Space): type a request. The selection or current page is attached automatically.
- **Accept / Reject / Retry** for the pending draft; **Undo AI** (the whole intent is one undo step); **Stop**
  (the existing pause).
- **Panel toggle:** the Activity panel (5.4).

### 5.2 Action library (these become the toolbox buttons)
Declarative files, one per action, in `~/.config/xournalpp/ai-actions/`. Defaults ship with the app; users edit or
add their own; Claude-style `SKILL.md` packs (like your `xournal-conspect`) can be imported.

```yaml
name: LaTeXify
icon: sigma
scope: selection            # selection | page | document | pen-command
prompt: |
  Transcribe the handwritten formula in the selection and typeset it as LaTeX at the same place.
output: replace             # replace | annotate | beside | new-page | answer-only
preview: draft              # draft (accept/reject) | direct
model: fast                 # fast | strong | a specific model id
```

Defaults:
- **Handwriting and text:** Beautify handwriting (your own glyphs), Transcribe, Fix spelling/grammar, Translate.
- **Maths and diagrams:** LaTeXify, Check maths, Clean diagram, Colorize, Plot this.
- **Understanding:** Summarize, Explain, Quiz me.
- **Writing and export:** Continue, Make conspect, Export page as Markdown.

### 5.3 Co-pilot, focused: watch small pieces, revise everything only on request
The co-pilot watches **small, fresh pieces** of work, not whole pages, which means fewer tokens and faster
reactions. Full passes are an explicit command.
- **Work units (automatic):** new strokes are grouped locally (close in space and time, no model). A unit is
  *closed* when you move to another area, or after an idle period. Only closed units are sent.
- **Watch zones (pinned):** lasso a region and choose "Watch this zone", with a per-zone rule ("LaTeX here",
  "tidy diagram here"). Everything outside is left alone.
- **Revise (manual, full):** "Revise page / selection", a strong-model pass for consistency, numbering and style,
  run in the background, with drafts to review.

A unit call sends little:
- a crop of the unit plus its element data;
- a thin halo of surroundings;
- a page thumbnail, only if needed;
- a short text *style memory* of the page, kept in notes.

Rough, to be measured in S2: a whole-page turn is 2–5k image tokens plus context and 10–60 s; a unit call is
200–600 tokens and 1–4 s. That's about ten times cheaper and faster.

Rules that keep it pleasant:
- Trigger on "moved on" rather than only on idle.
- A hands-off radius around the pen.
- Local pre-sorting (text / formula / drawing) to choose the rule and prompt.
- Tiered models (fast by default, strong for redraws or when the fast one isn't confident).
- Each unit processed once; editing a unit re-queues only it.
- A background queue with drafts; a budget meter.

The weak spot is the global picture (numbering, notation, cross-references). The answer is the manual Revise pass
plus the style memory, which is updated from accepted units.

Modes (decided): AI off · AI on with **Auto-improve off** (markers, commands, buttons, voice) · AI on with
**Auto-improve on** (the focused co-pilot) · **Revise** (a manual full pass, any time).

### 5.4 Activity panel (native GTK, collapsible side or bottom)
- A conversation per document: your requests, the agent's replies, tool calls as compact rows ("drew 42 strokes
  on page 3"), plans, cost and time per intent.
- Permission requests appear inline (Allow once / Always for this document / Deny), mapped onto our tiers.
- **Open session in terminal:** resumes the hidden session (`claude --resume`) in your normal terminal. No terminal widget in the app.

### 5.5 Runners
- **ACP runner (1.x):** the agent is chosen in settings: Claude Code (adapter), Codex (adapter), Gemini, OpenCode.
  The app launches it on first use, one per window, stops it after about 10 idle minutes, and resumes the session
  on the next use. It passes xournalai's MCP server and an "xournalai assistant" system prompt with the action
  library, and routes intents as prompts (the selection is sent as an image plus element ids).
- **Built-in runner (2.0):** direct API calls (Anthropic first; OpenAI-compatible and Ollama next), our tools
  in-process with no MCP hop, prompt caching, and a key stored in the system keyring. Mainly for the public
  release and for fast single-shot actions.
- **External (always):** any MCP client can serve intents; nothing changes for today's workflow.

### 5.6 Per-window transport
Today one TCP port serves one window. An embedded runner must drive *its* window. Add a per-process Unix socket
(`$XDG_RUNTIME_DIR/xournalai/<pid>.sock`) and `--mcp-socket=` for the bridge, so every window's assistant talks to
its own window. This also removes the "second window has no server" class of problems.

### 5.7 Trust, cost, privacy
- **Previews by default:** drafts are shown, and you accept or reject them.
- **Undo:** one undo step per intent.
- **Tier-based permissions** are shared by every runner.
- **Budgets:** a per-hour/day spend cap, with the cost of each intent shown in the Activity panel.
- **Privacy:** a "never send" layer or page flag; a local-model option (Ollama through OpenCode, or the built-in
  runner).
- **Memory:** the notes memory holds per-document conversation summaries, so a resumed session has context
  without replaying everything.

### 5.9 Daily-use stability and setup (prototype must-haves)
- **The agent is an isolated child process:** crashes and hangs are recovered, and the UI never waits on it.
- **Timeouts and cancel** on every intent, and no half-applied edits.
- **Usage limits:** if a limit is hit, it's said plainly; switching the runner is manual.
- **Setup check:** which CLIs are installed and logged in, which adapters exist; an adapter installer; clear login
  hints.
- **`tools/install.sh`:** build, install to `~/.local`, launcher, adapters, updates.
- **Logs:** a runner log per session, and an intent history.
- **Dogfooding gate:** two weeks of daily use without a blocker before the pitch build.

### 5.10 Several Claude sessions and the socket bus
You run several Claude Code sessions that talk over the local socket bus (`/run/user/1000/cc-socks/<pid>.sock`;
today `xournal-ai-9a`, `olek-45`, `andrew-proj-fd`). xournalai's assistant should be a citizen of that bus.
- **xournalai sessions are named peers** (e.g. `xournalai-lecture3`). Other sessions can ask them to draw into the
  open page.
- **Route any intent to a peer:** "→ @andrew-proj-fd: update the fleet docs from this" with the AI pen, or lasso
  plus "Send to session". Replies land as notes or drafts. The Sessions panel lists peers and their state.
- **Several sessions per document, by role:** a fast co-pilot (work units) plus a strong worker (Revise, redraws).
  Usage is shared, so it's shown.
- **Two channels:** ACP for control (structured, needed for previews, undo and stop), the socket bus for
  session-to-session messages.
- **Open risk, spike S5:** the bus protocol isn't a published API and may change; and does an ACP- or SDK-started
  session join the bus? Fallbacks if not:
  - (a) start Claude Code interactively in a hidden pseudo-terminal, so it's a full peer;
  - (b) a small isolated bus adapter in xournalai.

  Decided after S5.

### 5.8 Voice: talk while you draw
Not the existing audio recording: a **conversational mode**. The pen stays in your hand, you talk, and the app acts
on what you're looking at ("select a region, then say *make the lines of this part bolder*").

1. **Listening:** push-to-talk (stylus button, key, the mic button) or conversation mode (continuous, with speech
   detection). A mic indicator is always visible.
2. **Context snapshot at speech start:** the selection, pointer, visible region, recent strokes and the active tool.
   Pen gestures while speaking count as **pointing** ("put it *here*" plus a tap).
3. **Speech-to-text:** multilingual, local (whisper.cpp) or cloud, with live subtitles near the pointer.
4. **An intent:** the same queue as the pen, lasso and buttons.
5. **Two lanes:**
   - a **fast lane** for simple edits (bolder, colour, delete, undo, navigation): a fast model maps the utterance
     to a tool call in about 1–2 s;
   - an **agent lane** for complex requests, with a draft.
6. **Replies:** a toast, or optional short spoken confirmations and questions ("which box: left or right?").

Staging:
- **Voice A:** push-to-talk dictation into the prompt box, with the selection.
- **Voice B:** snapshot and gesture pointing, the fast lane, subtitles, clarifying questions.
- **Voice C (distant):** hands-free conversation, barge-in, spoken replies, a realtime "live" runner, a wake word.

## E7: think in parallel, edit one transaction at a time (built, 1.3.0)
- **The serving session coordinates:** each request goes to a background subagent (up to 5; `canvas-quick` on a
  fast model or `canvas-artist` on the strong one), and the coordinator ends its turn, staying responsive.
- **Thinking is concurrent, editing is not:**
  - `transaction_begin` claims an area and opens a private draft;
  - `transaction_commit` plays an ordered list (draw instantly or like a stylus, delete, restyle, move);
  - commits are serialized, each one undo step "AI: …"; completion order may differ from request order.
- **Conflicts are checked at commit:** strokes it depends on that changed meanwhile, or that the user drew over,
  refuse the commit with a reason; the draft is kept.
- **Hard limits:** N open at most, no overlapping claims, a 10-minute lease; Stop aborts all.
- **Visible:** a zone per transaction; the request's zone travels in the wake-up; the subagent count is shown.

## 6. Plan: the Linux prototype

**★ Milestone 1 = epoch E6: serving session in the app → 1.2.0** (the short-term plan; tasks in
`docs/mcp/tasks.json`, shown on the Kanban board)
- **S6.1 Embedded AI terminal (1.1.1):**
  - T6.1.1 terminal dock (VTE, collapsible, tabs, keeps running hidden);
  - T6.1.2 companion folder (CLAUDE.md: role, markers, assist contract, delegation, wake-up protocol; `.mcp.json`;
    hooks);
  - T6.1.3 serving session autostart: `claude --dangerously-skip-permissions --continue` by default (your decision;
    "normal" mode as a setting); "+" for Codex (`--dangerously-bypass-approvals-and-sandbox`), OpenCode or a shell.
- **S6.2 The app watches, the session works (1.1.2):**
  - T6.2.1 session state via hooks;
  - T6.2.2 event pump and wake-ups typed into the idle terminal (coalesced, watchdog, Restart).
- **S6.3 AI toolbar and Auto-improve (1.1.3):**
  - T6.3.1 the AI toolbar row built on your markers;
  - T6.3.2 the Auto-improve toggle;
  - T6.3.3 handwritten marker detection.
- **S6.4 Visible thinking (1.1.4):**
  - T6.4.1 the canvas overlay (grey veil, animated outline, icon, one-line status);
  - T6.4.2 status and cancel.
- **S6.5 (1.2.0):** T6.5.1 tests with a fake companion, docs, tag.

The AI toolbar:
- Improve strokes (`*!`), Illustrate (`**!`), Web summary (`*w!`), Real image (`*r!`), Command… (`*c!`);
- Revise page; the Auto-improve toggle (with a per-rule dropdown); Pause; Terminal.

Buttons act on the selection, or else the last piece drawn; written markers produce the same intent.

Visible thinking:
- a **translucent grey veil** plus an animated outline over the zone, a spinner, and a one-line status;
- states queued → thinking → drawing → done / failed;
- the status strip shows the state, the mode and the task count;
- tap the icon or Stop to interrupt.

**E5b: quick wins from real use → 1.2.0 (now, MCP only)**
- Compact tool results.
- Big drawings by path, streamed and chunked.
- `trace_image` (picture → native strokes).
- Annotations bound to their targets.
- Robust line and word grouping.
- Instant assist tier (no model): smooth, straighten, snap, spacing, palette recolour.
- Marker detection (candidate `*` / `!`, events with crops, `markers_list` / `marker_done`).

**E5c: sessions draw for you (MCP) → 1.2.x (early)**
- The diagram description format; `draw_diagram` with real graph layout; `diagram_structure`.
- `draw_chart` (hand or clean; handwriting labels once "My handwriting" exists); refreshable charts.
- Usable right away from external sessions.

**Spikes**
- **S1′** Companion launch: `claude` started by the app with MCP config, skills, pre-allowed tools and a first
  prompt; terminal choice; bus naming. (The ACP spike S1 is deferred with stage 3.)
- **S3** (back) VTE dock: build dependency, embedding, hiding while running.
- **S2** Latency and usage per action: page versus unit, fast versus strong model.
- **S4** Per-window Unix socket. (The VTE spike is dropped.)
- **S6** (dropped: image generation is whatever your Claude Code session has.)
- **S7** Does an ACP-started assistant session load its workspace's CLAUDE.md, skills and subagents? Can
  `claude --resume` take it over?
- **S8** Waking the companion: hooks reporting busy/idle; typing a wake-up line into tmux or VTE safely; the Stop-hook
  keep-alive; latency from a finished formula to the session acting.
- **S5** (now the key spike) Can xournalai *push* intents to the companion over the socket bus? Also: do ACP- or SDK-started sessions join it? Its protocol and stability; listing
  peers and sending and receiving from xournalai.

**E6: assistant core, intents and AI toolbox → 1.2.0**
- GTK-free assistant core (intent queue, action library, SKILL.md import).
- MCP intents surface.
- AI lasso, AI pen, prompt box.
- **Task a session from the canvas (the headline feature, first):** a region or page plus an instruction goes as a
  task, with image, elements and context, to a chosen session (external via MCP intents, or a bus peer if S5
  allows); the reply comes back as a note. Also "→ @session" with the AI pen.
- Acceptance: canvas → task → project session → result, from in-app controls.

**E7: result UX → 1.3.0**
- Visible thinking: zone highlight, thinking icon plus one-line status (`intent_update`), the states, tap to cancel,
  a count in the status strip. Work without an intent is still shown (regions its tool calls touch; the hooks' busy
  state).
- Drafts with Accept / Reject / Retry.
- One undo per intent.
- Progress and cancel.
- Timing per intent.

**E8: companion hardening after Milestone 1 → 1.4.0** (stage 1, the external terminal, is skipped)
- Assistant workspace (CLAUDE.md, skills, actions, exchange folder, pre-allowed tools); launch with the app, on AI
  on, or manually; MCP bound to this window; first prompt.
- **The app watches, the session works:** hooks report state; coalesced events; wake-ups by terminal input (tmux),
  bus push if S5 allows; watchdog, resend, Restart (`--resume`); none when AI is off or paused; the skill's
  timeout-stop rule removed.
- State in the status strip; one companion per window.
- All-day context (images out, state in notes, automatic rollover).
- The assist contract and the delegation default in the companion's instructions.
- Acceptance: start from the icon, and the canvas-session workflow works with the companion that comes with it.

**E8b: companion session, stage 2: hideable terminal dock → 1.4.x**
- A VTE dock (show or hide, keeps running), tabs for Claude plus Codex, OpenCode or a shell.
- Then the former E8 items below:
- Per-window socket; ACP client and lifecycle; Activity panel (no terminal); prompt and routing.
- Setup check and adapter installer; limit detection and failover; crash and hang recovery.
- Socket bus, complete: named peers, Sessions panel, replies as notes or drafts, asynchronous questions to project
  sessions.
- The assistant workspace (CLAUDE.md, skills, actions, exchange folder, fast canvas-watcher helper); "Open session
  in terminal".
- Capabilities: web search and insert image; images from the session's own generators (`*r!`); "improve this
  picture"; "sketch it" (reference → `trace_image`).
- Canvas SDK (Python plus CLI) for the assistant's scripts.
- "My handwriting" (capture, refine, `write_handwritten`).
- Your markers (`XC!`, `*r!`, …) in the action library.
- Acceptance: xournalai alone, launched from the icon, does the canvas-session workflow.

**E9: focused co-pilot and Revise → 1.5.0**
- The Auto-improve toggle (button, shortcut, menu, per-rule dropdown, status strip) drives the co-pilot; off =
  markers and commands only.
- Work units, watch zones, small payloads, pre-sorting, tiered models, caching, background drafts.
- Manual Revise; style memory.

**E10: linked boards and the launchpad → 1.6.0**
- Linked boards ("New board linked to…" a session or folder).
- The handoff package (image, transcription, diagram description, LaTeX, notes, open questions).
- "Start session from page" (templates, folder, terminal, bus, link).
- "Send to session".
- The round trip (progress marks and findings written back).

**E10b: Voice A, local push-to-talk → 1.7.0** (optional for the pitch)
- PortAudio capture, whisper.cpp, push-to-talk, transcript plus selection as an intent.

**E11: prototype release → 2.0.0 "pitch build"**
- `tools/install.sh`, logs and diagnostics, polish.
- Two weeks of dogfooding; three scripted demos; a demo video and a one-pager.

**Later horizon (designed for, not built):** remote runner (laptop serves phone, then a hosted endpoint) ·
built-in API runner · Android (new touch app, remote brain) · macOS (packaging) · Voice B and C · public release.

## 7. Open questions for you

1. **Commands in ink:** your markers only, markers plus an optional AI pen (recommended), or an AI pen replacing
   them? (Was: replace or keep `XC!`?) Should the AI pen replace your written `XC!` convention? It's faster and cheaper, but a
   habit change. Keeping both is possible: `XC!` would become a co-pilot rule.
2. **Which runner first?** ACP via claude-code-acp uses your Claude subscription. The built-in API runner is
   leaner but needs an API key. My recommendation: ACP first for you, built-in for the public.
3. **Previews by default,** or direct edits with undo? I recommend drafts for co-pilot and direct edits for
   explicit buttons.
4. **How far should the public release go:** Linux only (Flatpak) or cross-platform? This decides whether the
   terminal drawer is worth anything beyond you.
5. **Budget** (answered: seconds for small pieces, longer for big tasks, never silent): acceptable cost and latency per action in co-pilot mode. It sets the default model split
   (fast versus strong).
6. **Speech recognition:** local first (whisper.cpp, cloud optional), cloud first, or only the CLI's voice mode?
   Recommended: local first.
7. **Listening:** push-to-talk first and conversation mode later, or always listening from the start? Recommended:
   push-to-talk first.
8. **Spoken replies:** visual only, short optional spoken confirmations, or full spoken conversation? Recommended:
   short and optional.
9. **Focused co-pilot:** when is a piece of writing "done": when I move to another area (recommended), after a
   short idle, or only inside pinned zones?
10. **Default runner in the prototype:** Claude Code, Codex, or both with failover? Recommended: Claude by default,
    Codex selectable per document.
11. **Voice A in the pitch build:** yes, only if time allows (recommended), or later?
12. **Sessions per document:** one full session with a fast helper inside (recommended, rev 7), two sessions
    (co-pilot + worker), or one shared for the app?
13. **Peer names on the bus:** `xournalai-<document>` (recommended), chosen per document, or Claude's defaults?
14. **Launchpad:** where should a session started from a page run: a normal terminal window joining the bus
    (recommended), hidden in the Sessions panel, or ask each time?
15. **Charts from live data:** snapshots plus a Refresh button (recommended), auto-refresh while watching, or
    snapshots only?
16. **Companion start:** answered: with the app, connected to nothing else by default.
17. **Companion home first:** answered: straight to the in-app dock.
18. **Waking the companion:** terminal input first, switching to bus push once S5 proves it (recommended); bus push
    only; or a keep-alive loop via the Stop hook?
19. **Companion permissions:** answered: `--dangerously-skip-permissions` by default (your setup); "normal" mode
    as a setting.
