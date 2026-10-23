# xournalai changelog

The fork has its own version (`XOURNALAI_VERSION` in `CMakeLists.txt`). The application reports it as
`<upstream version>+ai.<fork version>`, e.g. `1.3.7+ai.0.1.0`. The embedded MCP server reports the fork version.
See [ROADMAP.md](ROADMAP.md) for the versioning rules.

## Unreleased
- **Skills in the repository** (`skills/`, `skills/install.sh`): xournal-conspect (handwritten conspects; the
  handwriting itself stays private in `~/.local/share/xournalai`), conversation-topic-map with a reusable
  `topicmap.py` (any conversation's data.json, English or Russian labels; a made-up example), and the remote
  transcriber helper as a template configured outside the repo.

## 1.15.3 — The recording chooser always comes back
- **Every recording asks again what it is.** Once the "what is it?" chooser had been closed without a choice
  (Escape with the keyboard in it, or the canvas hidden), it popped up only when no recording was waiting. The one
  it had been closed on stayed first, so every later recording only waited, unseen. Now a recording that stops
  while the chooser is closed shows it, with that recording first, and a chooser hidden with the canvas comes back
  when the canvas does. Recordings that stop while it is open still wait their turn.

## 1.15.2 — The pale 2 mm grid by default
- **"Pale graph (2 mm)" paper, the new default:** 2 mm squares with very pale (#f3f3f3), thin (0.2 pt) lines. It
  is in the paper list next to the others. New documents start on it, and new pages take the paper of the page
  before them, as before. If your settings still had the former default (the 2.5 mm fine graph) for new documents,
  they move to the pale grid; a paper you chose yourself is kept.
- **Agents' pages get it too.** An agent that asked `page_manage` for a "graph" page got the coarse 5 mm Graph,
  and could not set the pale grid at all (on 2026-10-21 it patched the saved file instead). A paper type now means
  your default style of it, so "graph" is the pale grid, and `background_config` sets a paper's parameters exactly
  (e.g. `r1=14.17` for 5 mm squares), within safe bounds.

## 1.15.1 — AI zones do not flood the X server
- **The freezes while agents work: found and much reduced.** On 2026-10-20 at 15:23 the window froze for 15 s
  (rescued and restarted) while it asked the X server for a new range of resource ids. Every pixmap and picture
  takes one of the app's ~2 million ids, and the app had used them all: it had sent 148 million X requests in 68
  minutes. While an agent works, its AI zone is animated. The overlay it is drawn on lies over the whole window,
  so every frame redrew and relaid out the entire window, and the zone's spinner ran at 60 frames a second. At
  each of those frames cairo uploaded every icon and every piece of the page shadows again, as new pixmaps. Now
  the overlay redraws only what moves (the zone's outline and status pill) and relays out its buttons only when
  they move, the spinner is a still stop icon, and the page shadows are kept on the X server. An animated zone
  went from ~41 700 X requests and 59 frames a second to 3 500–9 000 and ~20 (measured on Xvfb).
  `app_status.ui.x_requests_by_kind` counts the requests by kind, for the next time. Still heavy: zooming
  (~60 000 a second while it lasts) and an idle window (~1 500, GTK's own widgets).

## 1.15.0 — The on-screen keyboard only when called
- **GNOME's on-screen keyboard no longer pops up and down by itself in xournalai.** In tablet mode GNOME Shell 42 opens
  its keyboard whenever a text field of any app tells the input method (IBus) that it has the keyboard, closes it
  when it lets go, and slides the window up and down with it. In xournalai the keyboard moves between the AI
  terminal, the AI input fields and the canvas all the time, so the screen jumped. Now xournalai's text fields use
  GTK's own input method, and GNOME never hears of them: swipe up from the bottom edge of the screen to call the
  keyboard, and it stays until you close it. Typing, keyboard layouts (us, ru, ua) and compose keep working; only
  IBus input engines would not, and the session uses none. AI Agent → "Screen keyboard only when called" turns it
  back to automatic (remembered, takes effect at once). Programs in the AI terminal keep the session's input method.

## 1.14.0 — The agent works without moving your view
- **Agents' page and layer work no longer moves your view.** `page_manage` drove the window: before and after every
  operation it scrolled to the page and made it the current one, so on 2026-10-20 an insert at the end took the
  owner from page 1 to page 7. insert, delete, duplicate, move, background and size now change the given page in
  the document, with the same undo steps (an insert with a background is one step now); `layer_manage` works on
  any page likewise. Only `goto` moves the view, or `show=true` when you asked to be taken there. An inserted
  page no longer becomes the current one, so agents pass its number to their next calls. A size change on another
  page no longer re-fits your zoom, and "Page n of N" follows inserts and deletes.
- **Pages added or removed above keep what you see in place.** The scroll position follows the content, and the
  page you are on stays current under its new number. Before, the content under you moved by a page, and a
  deletion took you to the top of your page.
- **Your selection and text editing survive agents' work on other pages.** Every agent draw, edit and commit used
  to drop your selection and end your text editing, on any page. Now only work on that same page does: there the
  agent goes first.
- **A chip shows where an agent works.** When an agent changes a page you are not looking at, a small "AI ↓ p. 7"
  (↑ when it is above) appears at the top right of the canvas; a tap takes you there. It goes when you get there
  or 12 s after the agent's last change out of view.
- **Agents move the view only when asked, and not in the middle of what you do.** `view` (scroll, zoom, full
  screen…), `page_manage goto`/`show` and `elements_select` wait until you are between strokes, drags and typing
  (at most 8 s). `show_message` on a page out of view no longer scrolls there: it says "(p. N)" at the top of the
  visible area. Copying or cutting elements on another page no longer goes there either.
- **Undo, redo, the highlighter and the object pointer among the big buttons.** In Focus, the row of big buttons
  now also has the ordinary highlighter, the pointer (pick an object with a tap), undo and redo. In a narrow window
  some give way, in this order: Auto-improve first (it is in Tools too), then the command, the lasso, the pointer;
  the terminal stays, since it opens only from there. On a 1920-pixel screen all of them fit. The row now fills the room it has: every tile used
  to be counted as wide as the Touch switch, so with the laptop's screen upright (1200 wide) two or three tiles were
  missing before an empty stretch.

## 1.13.0 — The keyboard stays where you type
- **Saves and page changes no longer take the keyboard from the AI terminal or the AI input fields.** On 2026-10-20 the
  owner held space to dictate in the AI terminal, an agent saved (as agents do after every change), and the canvas
  took the keyboard: the held space then scrolled the document a screen per repeat, and the dictation ended. Every
  save and export, and every page that became current (also from the owner's own scrolling, or an agent's page work),
  moved the keyboard to the canvas. Now these moves leave it in the terminal, an AI Lasso or AI Highlighter input
  field, or any other text field that has it. Starting to draw on the canvas still takes it, as before. The canvas
  being made insensitive during your own save used to drop an AI input field's keyboard too: it is given back after
  the save, with its text intact.
- **Agents' saves no longer block the window.** `file_save` and `file_save_as` used to grey out the menus and
  toolbars, set a busy cursor, show the progress bar (the window's layout jumped) and make the canvas insensitive, so
  the pen did not draw while an agent saved. They now save in the background; the AI strip shows the running save.
  Your selection stays selected (its elements are saved at their places from copies), and text you are editing stays
  open (it is saved as it was before the edit). Changes made while a save runs stay marked unsaved: before, the end
  of any save marked them saved. Your own Ctrl+S works as before.
- **Drawing while a key is held in the terminal keeps the keyboard there.** A stroke started while you hold a key in
  the AI terminal or an input field (push-to-talk dictation) draws, and the keyboard stays where the key is held. A
  key counts as held only while it keeps repeating, so a lost release cannot leave it stuck.

## 1.12.7 — PDF exports do not freeze the pen
- **Exporting PDF and xopp no longer freezes the window either.** After 1.12.6 the remaining freezes on 2026-10-19
  were the companion's PDF exports of the magazine, 2–4 s each. `export` now writes pdf and xopp on the render
  thread as well. The PDF exporter reads the document itself, so during a PDF export an edit waits for it to finish;
  before, everything waited. Opening a file (`file_open`, 1–2 s for the magazine) still happens on the UI thread: it
  replaces the document anyway.

## 1.12.6 — Exports do not freeze the pen
- **Exporting png and svg no longer freezes the window.** On 2026-10-18 at 04:27 and 04:32 the window froze for about
  2.7 s each time the companion exported a page: `export` drew the page on the UI thread (the stall trace holds 32
  such freezes, the longest 9.8 s). It now draws on the render thread, as `render` does; the output paths and the
  overwrite permission are still checked first. An svg export copies the page and lets go of the document at once,
  so the pen does not wait for it either.
- **A freeze report says at once whether the X server is to blame.** The 2-second HANG line in the stall trace now
  starts with "the UI thread waits on the X server" or "the UI thread is not on X", and gives a second connection
  up to 250 ms to be answered. Before, it always said "a second connection has no answer either (0 ms)", which made a
  UI busy drawing an export look like a stalled server.

## 1.12.5 — Closing and X errors do not crash the app
- **SIGTERM no longer crashes xournalai.** A logout or `kill` saved the document for recovery and then crashed at exit
  ("terminate called without an active exception"), because the freeze watchdog's thread was still running when the
  app's static objects were destroyed. The watchdog now stops itself at exit.
- **An X error during drawing no longer ends the app.** On 2026-10-18 at 00:00 GDK ended the app on an X error that
  came back while GTK drew a widget (`cairo_mask` → `XRenderCreatePicture`). RENDER's errors and BadAlloc are now
  logged in the journal with the request they answer, and the app goes on (at worst one frame is drawn incompletely);
  GDK still handles every other error. `app_status` counts them (`ui.x_errors_survived`).

## 1.12.4 — A freeze restarts one app, and its report names the X request
- **After a freeze, one app comes back, not two.** On 2026-10-17 at 22:44 the X server stopped answering the app; the
  watchdog rescued the document and restarted xournalai, as designed. At the same moment the MCP bridge of the agent
  in the dying app's AI terminal started a second app for a tool call. That one took port 7474 and the rescued
  document, and the restarted one had no MCP server, so its AI terminal could not work. Once a bridge has seen the app
  running, it no longer starts one: a tool call while the app is closed or restarting is answered with "not open, try
  again in a few seconds", and the tools come back when the app does.
- **Freeze reports can name the X request again.** The log of recent X requests started only if another library had
  already loaded libX11-xcb, which on the laptop it had not, so none of its 29 freeze reports named a request. The
  app now loads it itself; HANG lines say how many requests were logged, and `app_status` shows it
  (`ui.x_requests_logged`).
- **The restart after a freeze works after an update.** At 23:04 and 23:17 the frozen app was rescued and closed,
  but its restart never came: an install had replaced the binary while the app ran, so the path it restarts from
  (`/proc/self/exe`) ended in " (deleted)". The app now uses the updated binary's path for the restart, and also for
  the agent's MCP command and hooks; it warns if the file is gone.
- **A freeze report says whether the X server answers others.** The first report from 23:17 shows the server stopped
  after the end of a frame (`CopyArea`) and never answered what followed (`FreePicture`, `FreePixmap`, `SYNC
  SetCounter`, GDK's `GetProperty`). Now the report also opens a second connection to the server and times one round
  trip: answered means the server ignores only this app, unanswered means it is busy or held by another client. The
  answer is in the HANG line and in the journal's "Frozen" message.

## 1.12.3 — A crash never leaves a dead app behind
- **A crash whose handler hangs now ends the app.** On 2026-10-17 heap corruption aborted the app, and the crash
  handler then waited forever for the allocator's lock (its own allocation). The dead process kept its window, MCP port
  7474 and helpers, so the next start's AI terminal could not connect. Now the hang watchdog rescues the document
  (offered on the next start) and ends the app within seconds; a 25 s deadline ends it in any case.
- **A crash writes its evidence first**: the crashing thread's stack goes to the journal and to
  `~/.cache/xournalai/crash.log` before anything allocates, and the version text is prepared at start.
- The corruption itself was looked for with an AddressSanitizer build (the whole suite and a 7-minute soak of that
  session's mix, `test/mcp_integration/soak_session.py`) and not reproduced; a repeat now names its culprit.
- Tests can run against another build (`XOAI_BINARY`), and `run.py --file` runs a scenario outside the suite.

## 1.12.2 — The AI highlighter hears a quiet microphone
- **Talking keeps the AI highlighter on**, also through a quiet built-in microphone. It used to count only levels
  above 0.03 RMS as voice; the laptop microphone's speech is ~0.01, so it finished after 10 s while you were still
  talking. Voice is now judged against the microphone's own noise (3x its quiet level, between 0.005 and 0.02).
- **A visible countdown**: the last 5 s before it finishes by itself, Done turns orange, says "Done in N…" and a
  bar on it drains. Any mark or word starts the 10 s again.
- The level meter in the mode bar is scaled for quiet microphones (it barely moved before).

## 1.12.1 — Readable over white paper
- **Automatic colours for the AI terminal** (a sun button next to the opacity slider, on by default,
  remembered): over a light page the terminal turns light (dark text on a see-through white background, with a
  palette made for light backgrounds), over a dark page it stays dark. Off: always dark, as in 1.12.0. For Claude
  Code, `/theme light` matches the light look.

## 1.12.0 — A see-through AI terminal
- **The AI terminal floats over the page** instead of pushing it up, with a see-through background so your notes
  show through; the text stays solid.
- **One row for everything**: the title, the tabs and the controls share the tab row (no extra bar).
- **An opacity slider in that row** (20–100%, 70% by default, remembered): lower shows more of the
  page, higher makes the text easier to read.
- **A dock button** next to it puts the terminal back under the page (pressed) or lets it float again.
- **Resize by dragging** the title or any empty part of that row up or down, floating or docked (remembered).
- `ui_inspect` lists up to 3000 widgets (was 800, which a window with terminal tabs reached) and says `truncated`
  when it stops early.

## 1.11.4 — The AI lasso looks like a lasso
- **The AI lasso has its own icon**: a violet lasso (like the violet AI highlighter) instead of a microphone, in the
  big buttons, Tools → AI and the AI toolbar, so it isn't mistaken for the plain selection lasso. Its tile now
  reads "AI lasso".

## 1.11.3 — Auto-improve under the thumb
- **Auto-improve switch** next to Touch, at the start of the big buttons ("Auto-improve off" / "on"): the same
  setting as the AI Agent menu and Ctrl+Alt+I, so it can be flipped in Focus where the menu is hidden. Also in
  Tools → AI.

## 1.11.2 — A finger switch
- **Touch drawing among the big buttons.** The first tablet button (with a gap after it, so it isn't hit by
  accident; also in Tools → View) switches touch drawing: off, as before, only the stylus draws and fingers scroll;
  on, fingers draw too. It is the app's own setting, so the menu and the settings agree.
- **A finger can mark for the AI.** With touch drawing on, a finger in the AI highlighter or lasso marks instead of
  leaving ink. The switch leaves the AI highlighter on, so it can be flipped in the middle of highlighting.
- The switch says what it is: "Touch off" / "Touch on" under its icon, highlighted when on.
- Fixed: starting in Focus (remembered from last time) left the side toolbars visible.

## 1.11.1 — The AI highlighter ends when you say so
- **No more cut-offs.** A pause between strokes no longer ends the request (and your next stroke no longer lands as
  a pen line). Highlight as many places as you like while you keep talking.
- **The strip becomes a mode bar** while the AI highlighter is on: the microphone level, the time, how many marks,
  Discard and a big violet Done — at the strip's height, so the page doesn't move.
- **Done by itself after 10 s** with no marks and no words (talking keeps it open); the last 3 seconds are counted
  down on the Done button.
- Escape discards; tapping the AI highlighter again (or Ctrl+Alt+H), or picking a tool, counts as Done.

## 1.11.0 — A smarter AI highlighter
- **Marks that go away.** The AI highlighter's marks are no longer ink: they are drawn over the page, stay while the
  AI works on the request and vanish when it is done (or failed, or its zone is dismissed). Cancel the popover and
  they go at once. Nothing to undo, nothing saved. For highlighting that stays, use the regular highlighter. (The
  "keeps its marks" setting is gone.)
- **The shape counts, not just a box.** The request carries each mark's path and width; an element counts only if a
  mark touches it, and one only partly covered is flagged as "a detail of it is meant" — so you can point at two
  letters of a word or one symbol of a formula.
- **The detail as a picture.** The AI also gets an image of the area with everything outside your marks faded.
- The AI highlighter no longer switches tools: you stay on yours, and the marks are drawn on top.

## 1.10.0 — The AI highlighter
- **Highlight while you speak.** The AI highlighter (Highlight on the AI row, the AI group in Tools, or
  Ctrl+Alt+H) draws like the highlighter, in violet, and starts listening on the first touch. Highlight one or
  several places while you explain; after a short pause the same Ask popover as the lasso opens with what you said,
  ready to edit, add to (microphone) or send with a command.
- The request names the places in the order you highlighted them and the elements under them. The marks stay on
  the page as a record of the question; AI Agent → "AI highlighter keeps its marks" off removes them when you send.
- Afterwards you're back on the tool you had, with the highlighter's own colour unchanged. The AI lasso stays as it
  was: mark first, then speak.
- **Tablet buttons in Focus.** With the menus hidden, the strip's empty left side holds big icon buttons, the same
  kind as in Tools: AI highlighter, AI lasso, pen, eraser, select, then command and terminal. While the pen is in
  use a palette button opens your colours as big swatches. In a narrow window the last ones give way first; Focus,
  Full screen, Record and Tools always stay.
- Fixed: switching full screen on or off in Focus brought the menu and toolbars back.
- **Consistent tool states:** picking any tool (tablet buttons, Tools, toolbar, keys) ends the AI highlighter and
  the AI lasso, so after the AI highlighter you can always go straight back to the pen; a second tap on an armed AI
  button switches it off.
- The AI highlighter has its own violet icon, so it can't be mistaken for the yellow highlighter.

## 1.9.2 — Full screen and Focus under the thumb
- **Focus** (big green button, or F9): hides the menu bar, every toolbar (top, left, right, bottom), the AI row and
  the sidebar; only the page and the big buttons stay. The button then reads "Menus" and brings everything back as
  it was. Your toolbar settings are not changed, and Focus is remembered. AI Agent → Big Focus button hides it.
- **The Tools menu covers what Focus hides**, now in two columns: Colour (the palette's first ten; a colour picks
  the pen when the tool has none), Shapes (line, rectangle, ellipse, arrow, shape recognizer; drawn with the pen,
  a second tap goes back to freehand) and View (zoom out, fit, zoom in), beside Select, Draw, Size, Page and AI.
- **Big Full screen button** next to Record, as large: one tap for full screen, and it then reads "Window" to go
  back (it follows F11 and the menu too). AI Agent → Big Full screen button hides it; the choice is remembered.

## 1.9.1 — One big Tools button
- **For tablet mode (pen, fingers, voice):** one big, colourful Tools button to the right of the toolbars, as tall
  as all of them, so it takes no row of its own. It (or F8) opens large buttons in fixed groups: Select (lasso,
  rectangle, pointer), Draw (pen, highlighter, eraser, text), Size (all five: very fine to very thick), Page (undo, redo, new page) and AI (Ask,
  Command, terminal). The tool in use is marked. Hide it from AI Agent → Big Tools button; the choice is remembered.
- **Big Record button (experimental)** next to Tools, as large: starts and stops audio recording. While recording
  it turns red and says "Stop 0:42"; rays around it are the last moments of microphone level and its core breathes
  with your voice, so a silent microphone shows at a glance. AI Agent → Big Record button hides it.
- In a narrow window the toolbars give the button its width: what doesn't fit goes into their overflow arrows.

## 1.9.0 — Your terminals, your tools under the thumb
- **Any command in a terminal tab.** The AI terminal's + menu (and AI Agent → "New tab with a custom command…")
  has "Custom command…": paste or type a command line (e.g. one of several OpenCode configurations), optionally a
  tab name and a folder. Commands are remembered (`ai-terminals.json`, most recent first, up to 12), offered again
  in the + menu, and can be forgotten from the popover.
- **Close terminal tabs.** Every tab has a ×; closing ends its program. Restart now acts on the tab shown even
  after tabs were reordered.
- **Quick tools.** A big grid button starts the AI toolbar (or press F8): a grid of large buttons for a thumb,
  finger or the pen — pen, eraser, Ask, select, Command, terminal, size (fine → medium → thick), new page,
  highlighter, undo, and more. The ten shown are the ones you use most: every way of choosing a tool (toolbar,
  menu, shortcut, the grid) is counted in `quick-tools.json`.

## 1.8.9 — Which request did the X server ignore?
- **The freezes now name their trigger.** In every frozen display so far the X server left one or two of this app's
  requests unanswered while serving everyone else. The app now remembers the last 64 requests it sent (their type
  and number; always on, negligible cost), and HANG and rescue lines in the stall trace list the unanswered ones by
  name, e.g. "unanswered: #168027882 RANDR:8, #168027883 GetInputFocus". No tracer or special launcher needed.

## 1.8.8 — Back to work faster
- **After a frozen display the app restarts itself.** When the X server stops answering (see 1.8.6), xournalai
  still rescues the document and closes, and now starts again 2 s later and offers the rescued document (at most
  twice in a row).
- **A crash in GTK's popover handling (2026-10-08):** GtkWindow tried to position a popover that no longer existed.
  The likeliest source: the "drawn here" hint that every agent drawing flashed, which was destroyed while still
  shown. It is now one popover, reused and closed, never destroyed while shown.
- **The next such crash names its cause:** with the stall trace on, every destroyed popover is logged (name, what it
  points at, whether it was still shown), and the first GTK "not a widget" error gets a stack.

## 1.8.7 — An Ask never stays stuck listening
- **A pen-button Ask ends when you let go, even if the release is lost.** On 2026-10-05 an Ask kept listening: three
  seconds before it the pen driver force-ejected the pen ("stuck in range"), which sends no button release. Now the
  pen leaving the tablet's range releases the button, a release anywhere in the window counts (not only over the
  page), and an Ask stops by itself after 2 minutes.
- **No deadlock behind the Print dialog.** Ctrl+P during agent work froze the app for good: the Print dialog holds
  the document lock while it is open and keeps serving events, so an agent's commit inside it waited for the UI
  thread itself. While a dialog runs, tools are now refused at once ("showing a dialog … try again"), and work
  resumes when it closes.

## 1.8.6 — A silent X server
- **Frozen by the X server? The app saves and closes.** Three more freezes on 2026-10-05 had the same cause as the
  tooltip one: the UI thread waited for an X server reply that never came (in a window-property read, in a repaint),
  while the server kept answering every other program. Nothing inside the app can unblock that wait. Now, 15 s into
  such a freeze, the app rescues the document (as in 1.8.5) and, when the UI thread is stuck in the X library, closes
  its X connection: GTK sees the display gone and the app exits. The next start offers the rescued document.
- **Evidence for the cause:** HANG lines carry the X request counters ("last request sent #N, last answered #M"),
  and `tools/xournalai-xtrace.sh` runs the app with its X11 traffic recorded (needs `xtrace`), keeping the last
  20000 lines.

## 1.8.5 — No tooltip freezes
- **The pen no longer freezes the app.** On 2026-10-05 the whole window froze for good: a GTK tooltip, started by the
  pen hovering over a button, asked the X server where the pointer was and waited forever for the answer (the
  kernel was force-ejecting a Wacom pen stuck "in proximity" at the time; the X server kept answering everyone else).
  Pen, eraser and touch hover outside the canvas no longer starts tooltips; taps, drags and the canvas are unchanged,
  and the mouse keeps its tooltips.
- **A frozen app rescues your work.** If the UI is stuck for 15 s, the watchdog saves the document to the emergency
  file from its own thread; the next start offers to restore it ("crashed or stopped responding"). A clean quit
  removes it.
- **Hang reports keep coming** after 60 s (they stopped there, mistaken for a suspended laptop).

## 1.8.4 — Hangs leave evidence
- **A freeze is on record even if you kill the app.** When the UI thread is stuck for more than 2 s, the stall trace
  (`~/.cache/xournalai/trace-stalls` on) gets a `HANG` line right away: what the UI thread is doing, repeated every
  10 s, and once the stack of every thread (a deadlock shows both sides). Before, a stall was written only when it
  ended, so a hang that was killed left nothing. Each run's first line gives its pid and start time.
- **A long status no longer stops the window from painting.** Zone statuses were shortened byte by byte, which cut
  Cyrillic letters (and "…") in half; the invalid text put the window's drawing into an error state until the zone
  ended (2026-10-02, about two minutes of a frozen-looking window). They are now cut by whole characters, and every
  text is checked before it is drawn.

## 1.8.3 — What should AI do?
- **The recording chooser asks "What should AI do?"** Type it, hold the mic (or the pen button) and say it, or tap
  chips: Summarize, Key points, Action items, Flashcards, Translate…. Enter or Send means Notes with that request.
- **Your request is yours, the recording is material:** the request reaches the assistant as your instruction; with
  Notes, what is said in the recording is still never followed as one. The page changes only if the request says so.
- **Keep audio with words** labels the recording (`transcripts/<name>.label.txt`).
- When the chooser and an Ask popover are both open, the pen button dictates into the chooser.

## 1.8.2 — Connect any agent
- **AI Agent → Connect an Agent…** (it replaces *Copy agent connect command*): pick how, copy, run or paste once.
  - Universal: an `mcpServers` JSON block for any MCP client, stdio (recommended: the bridge reads the token itself)
    or HTTP; the bare URL and header.
  - Per client: Claude Code (stdio or HTTP), Codex, Gemini CLI and OpenCode, with your paths and token filled in.
    The commands were checked against the clients themselves.
  - The dialog shows the token, so it is for you only: agents can neither open nor read it.
- `mcp.json` has the same under `_help.connect`.

## 1.8.1 — The mouse cursor comes back
- **After the stylus, the mouse finds its cursor again.** Moving the mouse, touchpad or TrackPoint after the pen or
  a touch applies the cursor again, even when the app believed it was already shown (something else may have hidden
  it meanwhile). Pressing Ctrl does the same.
- **Diagnosable:** with the touch trace on (`~/.cache/xournalai/trace-touch`), `touch-trace.log` also records each
  device switch and each cursor applied.

## 1.8.0 — Recordings: you decide (epoch E12)
- **Nothing is sent before you choose.** When a recording stops, a chooser with big buttons for a finger or a
  stylus asks what it is:
  - **Instructions:** Claude transcribes it and does what you said, like an Ask about the page.
  - **Notes:** Claude transcribes it into `transcripts/<name>.notes.md` next to the recordings: a title, a summary,
    the transcript with times and where your ink fits. What you said is treated as material, never as instructions.
  - **Keep audio:** nothing else happens, and Claude is not told.
  An optional title goes along. Recordings that end while the chooser is open wait their turn.
- **Processing is never silent.** A zone over the ink written during the recording and the status-line pill (teal,
  a travelling wave) show Claude's current step ("transcribing…", "writing notes…") until it is done.
- **Strokes line up with the audio.** A stroke's moment in a recording now counts from the recording's first captured
  sample, not from the click on Record; opening the microphone takes a moment, which put every stroke that much
  late in the audio. Highlighter strokes get their moment too. The log says how long the microphone took to start.

## 1.7.2 — Ask in the status line
- **The status line's recording pill animates for Ask too:** while Ask listens it shows "Ask: listening" with the
  time and teal bars moving with your voice, like the pill next to the pen. There is no Stop button: releasing the
  pen button ends it. When the recorder is on as well, the pill shows the recorder.

## 1.7.1 — Paper and recording comfort
- **Fine graph paper, the new default:** squares half the size of Graph (2.5 mm) with light, thin lines, so the grid
  stays in the background. It is in the paper list as "Fine graph (2.5 mm, light)"; documents keep their own paper.
- **You can't forget the recorder:** while the Xournal++ recorder records, the status line shows a pulsing red dot,
  the live microphone level, the time so far and a Stop button. It sits in the AI status line, or at the bottom of
  the window when the AI part is not there.

## 1.7.0 — Recordings for the AI (epoch E11)
- **The serving session hears about your recordings.** When the Xournal++ recorder stops, it gets a
  for-your-information line, and agents an `audio_recorded` event: the file, its length, the page, and the strokes
  written meanwhile. It transcribes the recording with your remote transcriber (its memory knows the server:
  faster-whisper large-v3, multilingual), falling back to the local one, and changes the page only when asked or
  with Auto-improve. Setting: `assistant.share_recordings` (on).
- **Ink and speech line up.** Strokes and texts written during a recording show their moment in it in
  `page_elements` (`audio: {file, t}`), so a transcript's times match what was written when.
- **`audio_transcribe`,** the local fallback: whisper.cpp, English only, with timestamped segments. It reads the
  recorder's Ogg files, runs in its own process (Ask keeps working), and keeps `<name>.local.txt` / `.local.json`
  in the recordings' `transcripts/` folder. An existing transcript is returned instead of redone, the remote
  `<name>.txt` first.

## 1.6.3 — Reliable speech
- **"Nothing heard" while the level bars moved:** the speech check wanted sound three times louder than the
  quietest part of the clip, and at least 0.012 RMS. Speaking from start to end made even the quietest part voice,
  and the owner's microphone is quiet (voice peaks about 0.015). The threshold is now 3× the quietest 10%, capped
  to 0.005–0.02, and clearly loud clips always go to whisper; silence and hiss are still ignored.
- **The microphone opens at once** after a previous request: transcriptions run on a queue in the helper (the start
  of a quick next phrase was lost).
- **A late transcript no longer hides the Listening indicator** of a new recording.
- **"Didn't catch that"** appears in the pill next to the pen when nothing was heard; errors show there too.
- **Diagnostics:** every attempt is logged in `~/.cache/xournalai/speech.log`, and the last clip judged silent is
  kept as `last-silent.wav`.

## 1.6.2 — Visible thinking, a popover for the stylus
- **An ask stays visible while it is worked on.** The serving session hands an ask to a background subagent and
  ends its turn at once, and that closed the zone: you saw nothing while the subagent worked. Now a turn that
  delegated keeps its zones until their transaction ends, the agent reports done or failed, you press Stop, or
  10 minutes pass. Subagents claim their zone before any research, report progress in it, and always end it.
- **Every zone has an animated status pill** above it, in the style of the recording indicator: waiting (a
  breathing dot), thinking (orbiting teal dots), done (a green tick), failed (a red cross), each with its status
  text. This covers markers and the toolbar actions too.
- **The Ask popover is sized for a stylus:** labelled command buttons (64×56), a larger text field (16 px), and big
  microphone and Send buttons.
- Fix: the layer selector showed an agent's hidden draft layer as the current one. It was display only: your
  strokes went into your layer.

## 1.6.1 — Ask: the button and the recording indicator
- **Ask button** on the AI toolbar (the first one; Ctrl+Alt+A). The next stroke, with the pen or the mouse, draws a
  dashed lasso instead of ink; the popover opens there. Type, tap a command, or hold the pen button (or the
  popover's microphone) to dictate into it.
- **The pen button decides on release:** a lasso drawn while holding it is a new ask; only speaking while an ask is
  open adds to that ask.
- **Recording indicator:** a dark pill next to the pen appears the instant you press. It has a pulsing red dot,
  level bars that move with your voice, and "Listening"; after release it shows bouncing dots and "Transcribing".
  The helper reports the microphone level about 20 times a second.
- Fixes: finishing a lasso could crash (the lasso hook removed itself while running). `ui_interact` now clicks a
  toolbar button's inner button, so toggles such as Pause, Auto-improve and Ask work for agents too.

## 1.6.0 — Ask: lasso + voice (epoch E10, experimental)
Point and say. **Hold the pen's first barrel button, speak, and circle the area with the tip; release.**
- **Speech is transcribed locally** by `xournalai-stt`, a small helper built with whisper.cpp v1.9.4 (English,
  `base.en`, ~1 s for a short request on an i7-1355U). Nothing leaves the laptop. The model (148 MB) is downloaded
  once into `~/.local/share/xournalai/models`. Settings: `assistant.speech` and `assistant.speech_model` in
  `mcp.json`.
- **Silence changes nothing:** holding the button without speaking is still the button's usual tool (e.g. the lasso
  selection).
- **The Ask popover** opens next to the circled area. It holds the transcript (editable), one-tap command icons
  (improve, illustrate, write, revise, style, explain, summarize), a hold-to-talk microphone button, Send (Enter)
  and close (Esc). It does not grab the pen. With speech on, the toolbar's **Command…** opens it for the selection.
- **The request** becomes a thinking zone on the area and wakes the serving session:
  `Ask [command]: "…" — about N element(s) on page P at [x,y,w,h] (circled) (ids …) [zone N]`. The companion
  instructions explain how to read it.
- **Tests** (synthetic pen events, fake microphone): `test_e10_stt.py`, `test_e10_speech_client.py`,
  `test_e10_ask.py`.

## 1.5.2 — Review fixes
- **Zoom keys no longer stay blocked after a palm touch:** the pinch guard (1.3.1) could stay on after a third touch
  or a lost touch end, ignoring zoom keys, Ctrl+wheel, the slider and agents' zoom. It now ends with the touches,
  and expires after 3 s without pinch movement.
- **The UI stall watchdog wakes up rarely:** it slept only 5 ms at a time, about 200 wake-ups per second on a
  laptop. It now sleeps until a stall could have started.
- **`wait_for_user` and `draft(op="render")` render off the UI thread**, like the other reading tools.
- The deep zoom toggle is disabled in presentation mode, like the other zoom controls.

## 1.5.1 — Fixes
- **"PDF background missing" on every page (1.5.0 regression):** the copy of a page used for rendering became a PDF
  page. Its background is now copied as it is; `test_e9_backgrounds.py` and `PageSnapshotTest` check it.
- **Deleting a page no longer waits for other pages to render:** removing a page view waited for whatever
  background job was running (3–4 s on a heavy page); now only for that page's own job (0.02 s).
- **Touch trace** (`~/.cache/xournalai/trace-touch`) also logs the touch handler's decisions: touches ignored as
  invalid, resets, blocking.

## 1.5.0 — No freezes (epoch E9)
Agents thinking never freeze the pen, and agents drawing freeze it no longer than the drawing itself. Measured on a
page with 2,500 strokes (1M points), before → after:
- `page_render` (an agent looking at the page): the UI froze for 1.4 s → no freeze.
- `page_elements` with full detail: 0.25 s → none. `layout_analyze` and `blocks_render`: up to 0.1 s → none.
- The pen, at the end of each stroke, waited up to 0.38 s for the sidebar preview render → none.
- An edit right after a zoom waited ~3 s for the page to re-render → 0.03 s.

How:
- **Agents' reading tools run off the UI thread** (on the render thread). They take a snapshot of the page under a
  short lock and work on the copy; their results are serialized there too.
- **Page and preview rendering hold the document lock only to copy what they draw**, not for the whole render.
- **UI stall watchdog:** every time the UI thread does not respond for over 50 ms, it is recorded with its cause, in
  `app_status.ui`. A detailed log is written to `~/.cache/xournalai/stall-trace.log` while
  `~/.cache/xournalai/trace-stalls` exists.
- **Test:** `test_e9_responsiveness.py` covers agents reading, and the user drawing meanwhile, with no stall of
  100 ms or more. The `test_block_ui` hook (test builds' hooks only) checks the watchdog itself.

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
