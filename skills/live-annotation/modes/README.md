# Live-notes modes (since 1.17.0)

The owner chooses a mode by holding the Record button. The app names it in every wake-up line, e.g.

    Live notes (recording 2026-11-10_10-00-00, page 3), mode «Конспект без команд» (safe: annotate=visual,
    requests=off; options: intense, summary): new transcribed speech. … Read modes/safe.md …

`SKILL.md` describes the default mode, 🧠 **Конспект + просьбы** (`conspect`). The module of another mode changes
only what it says it changes; everything else in `SKILL.md` stays as written. The options of the recording are
described in [options.md](options.md).

| Mode | id | Module | In short |
|---|---|---|---|
| 📝 Текст | `text` | — | the app writes the transcript only; you are woken only at the end, and only with the option «Итог в конце» |
| 🎙 Голосовое управление | `voice` | the **voice-control** skill | no live-notes page; you get commands |
| ✍ Просьбы к рисунку | `drawing` | [drawing.md](drawing.md) | no cards; requests act on the owner's own page |
| 🧠 Конспект + просьбы | `conspect` | `SKILL.md` as written | cards and requests within the safe list |
| 🛡 Конспект без команд | `safe` | [safe.md](safe.md) | cards and visuals; **no request is ever carried out** |
| 🗒 Ведение конспекта | `konspekt` | [konspekt.md](konspekt.md) | a conspect with a really good layout and graphs |
