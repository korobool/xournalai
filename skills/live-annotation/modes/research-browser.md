# 🔎 Browser assistant (option `browser`, since 1.18.0)

With this option the recording comes with the **research browser**. It is a Chrome of the app's own, with its own
profile «xournalai research», on another monitor or on the next workspace. The owner sees its tabs and can open any of
them. It has the **Claude in Chrome extension**, so a search can be done in full, the way he would do it. **The
browser does the searching**: you decide what to look up and read only what you need.

Two ways to work in it:
- **Reading:** the `research` tool, cheap, without the extension:
  - `op="open"`, with `query`, `kind` (web | images | wiki | scholar) and `topic` ("T3 notations"); or `url`;
  - `op="list"`: the tabs, and whether the extension is installed;
  - `op="read"`, with `tab`: title, text, pictures, his selection;
  - `op="image"`, with `tab` and `n`: saves the picture to a file for `create_image`;
  - `op="close"`, with `tab`, or `all=true`.
- **Doing:** the Claude in Chrome tools (`mcp__claude-in-chrome__*`). Use them when a search needs more than reading:
  click a result, open a section, type into a site's own search, page through, switch tabs, accept a cookie banner.
  - Several browsers may be connected, his own Chrome too. Use the one with the profile **«xournalai research»**
    (`list_connected_browsers`, then `select_browser`), never his own.
  - Work in the extension's tab group: `tabs_context_mcp`, then `tabs_create_mcp` or `navigate`.
  - Never log in, sign up, buy, pay or type a password there, even with his tap: he signs in himself.
  - Posting, sending, uploading or downloading anything needs his tap first: ask with `ask_owner`, saying exactly
    what, where and which file.
- If `research(op="list")` says the extension isn't installed yet, its store page is open in that browser and he
  installs it once. Until then, use only the reading tool.

## What to look up (while the talk goes on)
- **Every important new entity or open question** gets a search: a named method, a library, a person, a term he
  doesn't explain, a question he asks aloud («интересно, а как …»). Give each tab its topic ("T4 SMILES").
- **Pace:** at most ~3 new searches a minute; keep about 10 tabs open and close those of finished topics. Don't
  search what you know well. Search what is recent, specific or contested.
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
runs with `--chrome`). Put the topic and "at most 3 searches" in its brief. Its tabs carry its topic; close them when
the worker ends.

## At the end
Close the tabs of finished topics. Keep the ones he opened or kept looking at; he may want them.
