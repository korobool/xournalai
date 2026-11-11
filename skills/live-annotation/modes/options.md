# The recording's options (since 1.17.0)

The owner ticks them in the Record menu. The wake-up line lists those that are on (`options: …`). Each one
changes only what it says.

- **📍 `this_page`** (This page; off = 📄 New page): the recording starts on the page the owner was on, not on a new
  page after it. In `pages` the first entry has `own_page: true`: his own content is there. Draw only where nothing
  of his is (`find_free_space` on that page), never over or under his ink, a slide or a picture; when it is full,
  go on as `SKILL.md` says for a full page. Its `transcript_column` is `null` when the page had no room at its right edge:
  then the transcript is not on the paper at all (read it with `live_notes`), and `free_area` is the whole page
  (still only where it is empty).
- **⚡ `intense`** (an intensive brainstorm): think more and in parallel.
  - Start a worker for **every** topic at once, up to the cap: heavy topics at effort `xhigh`, light ones at
    `medium`.
  - Add background and connections on your own (marked «фон:»): what was said, and what it relates to.
  - Read again as soon as you are woken; don't batch.
  - It costs more of the owner's limits, and he chose it.
- **🧾 `summary`** (a summary at the end): when the recording ends, write a **summary page** after the live pages.
  It has:
  - the main points (5–9);
  - decisions (✓);
  - open questions (?);
  - action items (→ who, what);
  - the requests and their outcome.

  With the mode 📝 Transcript this is the only thing you do: read the whole transcript (`live_notes(op="read",
  since=0)`) and write that page.
- **Speech commands** (`commands=` in the line; not an option but a switch of its own since 1.20.0):
  - `off`: nothing said is ever carried out (`modes/safe.md`);
  - `trigger`: a piece is a request **only** when it starts with the trigger phrase the line gives («Hey Assistant,
    …», «Эй, ассистент, …»). Everything else is material, however much it sounds like a request. Drop the phrase
    itself when you quote the request;
  - `talk`: requests are recognised from what he says, as `SKILL.md` describes.

  He may change it during the recording: the next wake-up line says so.
- **❓ `questions`** (May ask me; «💡 Proposals» before 1.22.0; the question panel): you and your workers may ask
  the owner during the recording with `ask_owner`: a short question, a few big options («Yes, draw it», «No»), a
  safe default, `wait=false` so that nobody stops (you are woken with the answer; a worker looks with `op=get`). Ask only what matters: whether to
  carry out an unsure request (instead of the `⚡?` tag), which of two layouts, whether to insert a picture a
  worker found. Without this option, don't ask during the recording: tag and go on. A question about something on
  the page (a card, a slot, a picture's place) gets `about` ({page, region}) and `short`: it shows there too.
- **🔎 `browser`** (Browser assistant): the research browser runs with the recording; how to use it is in
  [research-browser.md](research-browser.md).
