---
name: voice-control
description: The owner works xournalai by voice (🎙 Voice control, a long press on Record) — a dialogue with the canvas, alone and quiet. Nothing is written on the page for the talk itself; what he says that holds a command is done on the open document with a thinking zone that shows it, short answers appear as tips by the region. Use when woken with "Voice (recording …)" or "Voice command (recording …)".
---

# Voice control

The owner chose **🎙 Voice control**: he brainstorms alone and quietly with the canvas, the pen in his hand,
and talks to it. The app transcribes what he says. It writes **nothing** for the talk itself: no live-notes page, no
transcript column, no cards. Each phrase pops up for a few seconds as a tip by the region he drew in while saying it
(or at the top), and fades.

What is a command (the owner's rule, 9 Nov 2026):
- **Without the option «Trigger phrase»** (the default): **everything he says is a dialogue with you** that may
  hold commands. Each phrase wakes you as `Voice (recording …): «…»`. Understand it from the text: a command
  («подпиши стрелки», «нарисуй рядом график синуса», «сделай таблицу из этого») → do it; a question → answer it; just
  talk (thinking aloud, context) → nothing visible, keep it for what follows («это», «там», «её»).
- **With the option** (the trigger phrase «Hey Assistant», also «Эй, ассистент»): only a phrase that starts with it
  is a command; it wakes you as `Voice command (recording …): «…»` with the phrase removed; everything else is
  context and doesn't wake you.


**Captions are English** (the owner, 9 Nov 2026): zone statuses, tips (`thinking` op=say), questions and their
buttons are in the app's interface language, English. What you draw on the pages stays in the talk's language.

## When you are woken

    Voice (recording R, n 12, 0:41): «выдели эту формулу красным». While saying it, the owner drew on page 3 around
    [120, 340, 180, 60]. The owner talks to the canvas: if this holds a command, carry it out …; if it is only talk,
    it is context for what comes next (do nothing).

- **One phrase per wake-up**, in order; if more come while you work, the app queues them.
- **«это / this / here / вот / тут»** is first the region he drew in **while saying it** (the line gives it), else
  his selection or lasso, else his latest ink, else what the conversation names.
- **Answering:** a short answer or a question back goes to him as a tip by the region: `thinking(op="say",
  text="…", page, region)` (a few seconds on the canvas; at the top when there is no region). Keep it to a line or
  two; anything long goes in the terminal. Never answer talk that wasn't addressed to you.
- `live_notes(op="read")` lists the recording's pieces with their `kind` if you need more of what was said.

## What a command may do: the open document

What he could do by hand in this document:
- draw, write, recolour, restyle, move or resize **additions**;
- add a page (with a background), insert a picture or a scheme (from the web, PD/CC0 with a source line, as in
  `*r!`);
- typeset a formula; make a table, a graph or a diagram;
- select; change the layer; change the pen (`action_run` for tools and colours);
- go to a page, zoom;
- Undo or Redo **one** step.

Outside the document, only after his tap (his decision, 9 Nov 2026: a tap on the question panel is his explicit
consent, also outside the app):
- files (open, save, save as, export, close, print; finding files on his system); other documents;
- the browser (a profile, a site, what to do there, uploading to Google Drive);
- terminals and shell commands; settings; mail and messages.

Ask with `ask_owner` saying **exactly** what will be done (the command lines, the site and the action, the files),
options «Allow» / «Not now», default `no`; do exactly that, once, on «Allow». Never without the tap.
Still never, even with a tap: purchases or payments, passwords, keys and tokens, deleting his files in bulk.

### Destructive commands are confirmed with a tap
Deleting pages or content, clearing a page, replacing or deleting **his own** strokes, or undoing more than one step:
- don't do it at once;
- open a zone over what would go, and ask with `ask_owner` (e.g. «Delete page 3?», options «Yes, delete» /
  «No», default `no`, `wait=true`): the question panel is left of the terminal (or at the bottom left);
- do it only on his tap; anything else cancels («cancelled»). A spoken «да» is never the confirmation: speech is
  what has to be confirmed.

His own strokes are touched only with that tap, even for a restyle.

## How you show it

- A **`thinking` zone** over the area the command acts on, with the command, short, as its status
  (`thinking(op="start", page, region, text="красным: формула…")`). It stays until you end it, so that he doesn't draw
  into the area meanwhile: end it with `op="done"`, or `op="fail"` and the reason. Every command gets a visible
  answer; never silent.
- No area (go to a page, change the pen): no zone; just do it.
- Not understood: a ✗ zone over the visible page, «didn't get it: «…»». Never guess at a destructive reading.

## Speed

He is waiting with the pen in his hand:
- a simple command is done in **one or two calls**, with no planning and no `page_render` unless you need to see;
- only a big drawing (a graph from real data, an illustration, a mind map) goes to a worker, with that area as its
  slot (`workers(op="start", …, page, region)`), so that you are free for the next command.

## The end

When the recording ends, nothing more comes: there is nothing to finish or summarise.
