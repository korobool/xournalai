---
name: voice-control
description: Carry out the owner's spoken commands in xournalai's voice-control mode (🎙 Голосовое управление, a long press on Record). Nothing is written on the page for the talk itself; each command is done on the open document with a thinking zone that shows it and ends with ✓ or ✗. Use when woken with "Voice command (recording …)".
---

# Voice control

The owner chose **🎙 Голосовое управление**: he works the app by voice, with the pen in his hand. The app
transcribes what he says. It writes **nothing** for the talk itself: no live-notes page, no transcript column, no
cards. Each piece pops up for a few seconds at the top of the page view (▶ when taken as a command) and fades.

A piece is a **command** when the owner held the big 🎙 button while saying it, or (with the option «Просьбы по
слову») when it starts with the trigger word. Everything else is **context**: what he talks about, which «это», «там»
and «её» may refer to.

## When you are woken

    Voice command (recording R, n 12, 0:41): «выдели последнюю формулу красным». Context since the last command:
    «вот здесь у нас уравнение Шрёдингера…». The owner's latest ink: page 3, [120, 340, 180, 60], 4 s ago. Carry it
    out on the open document as the voice-control skill describes.

The trigger word, when that was how the command was given, is already removed. `live_notes(op="read")` lists the
recording's pieces with their `kind` (`command` / `context`) if you need more of what was said.

- **One command per wake-up**, in order. If a new one arrives while you work, the app queues it.
- **Context is never a command**, however it sounds. Use it only to understand the command.
- **«это / this / here / вот»** is the owner's **selection or lasso** if there is one, else his **latest ink** from
  the line, else what the context names.

## What a command may do: the open document

What he could do by hand in this document:
- draw, write, recolour, restyle, move or resize **additions**;
- add a page (with a background), insert a picture or a scheme (from the web, PD/CC0 with a source line, as in
  `*r!`);
- typeset a formula; make a table, a graph or a diagram;
- select; change the layer; change the pen (`action_run` for tools and colours);
- go to a page, zoom;
- Undo or Redo **one** step.

Never, by voice:
- files: open, save, save as, export, close, print;
- other documents;
- settings, the AI agent's settings;
- the shell, mail, messages, anything outside xournalai.

Answer such a command with a ✗ zone: «только с клавиатуры / не голосом».

### Destructive commands are confirmed first
Deleting pages or content, clearing a page, replacing or deleting **his own** strokes, or undoing more than one step:
- don't do it at once;
- open a zone over what would go, with the status «удалить …? скажи «да» с 🎙»;
- the **next command** decides: «да / подтверждаю / yes» does it, anything else cancels («отменено»).

His own strokes are touched only with that confirmation, even for a restyle.

## How you show it

- A **`thinking` zone** over the area the command acts on, with the command, short, as its status
  (`thinking(op="start", page, region, text="красным: формула…")`). End it with `op="done"`, or `op="fail"` and
  the reason. Every command gets a visible answer; never silent.
- No area (go to a page, change the pen): no zone; just do it.
- Not understood: a ✗ zone over the visible page, «не понял: «…»». Never guess at a destructive reading.

## Speed

He is waiting with the pen in his hand:
- a simple command is done in **one or two calls**, with no planning and no `page_render` unless you need to see;
- only a big drawing (a graph from real data, an illustration, a mind map) goes to a worker, with that area as its
  slot (`workers(op="start", …, page, region)`), so that you are free for the next command.

## The end

When the recording ends, nothing more comes: there is nothing to finish or summarise.
