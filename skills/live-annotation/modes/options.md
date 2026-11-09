# The recording's options (since 1.17.0)

The owner ticks them in the Record menu. The wake-up line lists those that are on (`options: …`). Each one
changes only what it says.

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

  With the mode 📝 Текст this is the only thing you do: read the whole transcript (`live_notes(op="read",
  since=0)`) and write that page.
- **🔤 `trigger`** (requests by a word): a piece is a request **only** when it starts with the trigger word that
  the wake-up line gives (e.g. «Клод, …», "Claude, …"). Everything else is material, however much it sounds like a
  request. Drop the word itself when you quote the request.
- **❓ `questions`** (the question panel): you and your workers may ask the owner during the recording with
  `ask_owner`: a short question, a few big options («Да, рисовать», «Нет»), a safe default, `wait=false` so that
  nobody stops (you are woken with the answer; a worker looks with `op=get`). Ask only what matters: whether to
  carry out an unsure request (instead of the `⚡?` tag), which of two layouts, whether to insert a picture a
  worker found. Without this option, don't ask during the recording: tag and go on.
- **🔎 `browser`** (a browser assistant): not available yet.
