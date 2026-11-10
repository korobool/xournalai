# 🔎 Browser assistant (option `browser`, since 1.18.0)

With this option the recording comes with the **research browser**. It is a Chrome of the app's own, with its own
profile «xournalai research»: on one monitor it stays behind his notes, with several it is on another one. It never
takes the focus by itself (the app brings him back if it does). He goes there with «🔎 Browser» and comes back with
«↩ Back», so he sees its tabs and can open any of them. It has the **Claude in Chrome extension**, so a search can be done in full, the way he would do it. **The
browser does the searching**: you decide what to look up and read only what you need.

Two ways to work in it:
- **Reading:** the `research` tool, cheap, without the extension:
  - `op="open"`, with `query`, `kind` (web | images | wiki | scholar) and `topic` ("T3 notations"); or `url`;
  - `op="list"`: the tabs, and whether the extension is installed;
  - `op="read"`, with `tab`: title, text, pictures, his selection;
  - `op="image"`, with `tab` and `n`: saves the picture to a file for `create_image`;
  - `op="keep"`, with `tab` (or `url`, for a page you reached through the extension), `topic` and `note`: a target
    page (see below), marked ★;
  - `op="close"`, with `tab`, or `all=true`;
  - `op="sources"`: the kept pages by topic.
- **Doing:** the Claude in Chrome tools (`mcp__claude-in-chrome__*`). Use them when a search needs more than reading:
  click a result, open a section, type into a site's own search, page through, switch tabs, accept a cookie banner.
  - Several browsers may be connected: his own Chrome, his other profiles (DataAnnotation…), and their default
    names are all alike. `research(op="list")` gives the research browser's `extension.name` («xournalai
    research») and `extension.device_id`: `select_browser` with **exactly that id**, never another browser. If the
    id is still empty (the extension hasn't connected yet), use only the reading tool. Another browser only when he
    names it.
  - **Closed?** If he closed it (it is missing among the connected browsers), any `research` call opens it again:
    call `research(op="list")`, wait a few seconds for the extension to connect (its `device_id` appears), then
    select it.
  - Work in the extension's tab group: `tabs_context_mcp`, then `tabs_create_mcp` or `navigate`.
  - Never log in, sign up, buy, pay or type a password there, even with his tap: he signs in himself.
  - Posting, sending, uploading or downloading anything needs his tap first: ask with `ask_owner`, saying exactly
    what, where and which file.
- If `research(op="list")` says the extension isn't installed yet, its store page is open in that browser and he
  installs it once. Until then, use only the reading tool.
- **Signing in** (Google, claude.ai, the extension) is his, in a window the app lets go of: Google refuses to sign in
  while the app is connected to the browser («This browser or app may not be secure»). When he asks to sign in there,
  or a sign-in is refused, call `research(op="signin")`. The browser restarts without the app's connection, with a page
  that says what to do. Until he closes that window, `research` answers that it is open for signing in, and you wait.
  Then it is back, connected, with his sign-ins. He can also tap «🔑 Browser sign-in» in the Record menu.

## What to look up (while the talk goes on)
- **Every important new entity or open question** gets a search: a named method, a library, a person, a term he
  doesn't explain, a question he asks aloud («интересно, а как …»). Give each tab its topic ("T4 SMILES").
- **Pace:** at most ~3 new searches a minute. Don't search what you know well. Search what is recent, specific or
  contested.
- **Target pages and intermediate ones** (his rule, 10 Nov):
  - A **target page** holds what was looked for: the page he asked for, or the one that shows the entity or the
    answer (the article, the paper, the person's page, the product, the picture's page). Mark it at once with
    `op="keep"` and a short note. It stays open until **he** closes it.
  - An **intermediate page** is the way there: search results once you have opened what you needed, steps along a
    site, a page that turned out off topic. Close it as soon as you are past it. Keep at most about 10 of them.
  - Never close a kept page (★) or one he looked at: the app refuses. Only when he asks you to close it, pass
    `owner_asked=true`.
- **Pictures:** `kind="images"` for objects that are better shown than said: a molecule, a device, a diagram, a
  person.

## What to do with what is found
- **Facts:** put the gist on the topic's card with a small source line ("— en.wikipedia.org"). Mark it as background
  ("фон:"), never as something that was said.
- **Pictures and quotes are the owner's choice.** With the ❓ Question panel option, propose them with `ask_owner`:
  `wait=false`, in English, a default of not inserting, e.g. «Insert this picture of the SMILES notation?» with
  «Insert» and «Not now». On «Insert», call `research(op="image")`, then `create_image(path, …)` inside the topic's
  slot, with a source line. Without that option, insert no pictures; put a small «🔎 2 tabs» mark on the card, and
  the tabs are there for him.
- **His own picks:** he may select text in the browser and tap «📌 Insert selection». The app then puts the quote,
  with its source, on the page he looks at. Leave it there; use it as material if it belongs to a topic.

## Workers
A worker with a heavy topic may search too: it has the `research` tool and the Claude in Chrome tools (its command
runs with `--chrome`). Put the topic and "at most 3 searches" in its brief. Its tabs carry its topic. It keeps its
target pages (`op="keep"`) and closes its intermediate ones before it ends.

## At the end
Close the intermediate pages left (`op="close", all=true` leaves the kept ones and those he looked at). Then draw a
**«🔎 Sources»** card on the live notes from `research(op="sources")`: by topic, each kept page's title, its
address and the note. The kept pages stay open until he closes them.
