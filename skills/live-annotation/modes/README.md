# Live-notes modes (since 1.17.0; reworked in 1.20.0)

The owner holds the Record button and answers two separate questions:
- **what appears on paper**: the tile he taps (📝 Transcript, 🧠 Notes, 🗒 Typeset notes, 🎙 Voice only);
- **may his speech steer things**: the switch «Speech commands»:
  - 🛡 **off**: others talking, a video;
  - 🔤 **only «Hey Assistant»**: people around;
  - 💬 **conversation**: just him.

  He can change the switch during the recording too.

The app names both in every wake-up line, e.g.

    Live notes (recording 2026-11-10_10-00-00, page 3), mode «Notes» (conspect: annotate=visual, commands=off;
    options: intense, summary): new transcribed speech. … the live-annotation skill and its modes/safe.md …

`SKILL.md` describes 🧠 **Notes** with 💬. The modules change only what they say they change; everything else in
`SKILL.md` stays as written. The options of the recording are in [options.md](options.md).

| Tile | id | 🛡 commands off | 🔤 «Hey Assistant» | 💬 conversation |
|---|---|---|---|---|
| 📝 Transcript | `text` | the transcript only: you are woken only at the end, and only for «Summary at the end» | [drawing.md](drawing.md), woken only for a phrase with the trigger | [drawing.md](drawing.md): requests act on his own page; nothing drawn for the talk |
| 🧠 Notes | `conspect` | [safe.md](safe.md): cards, schemes, maps; **nothing said is ever carried out** | `SKILL.md`; only a phrase with the trigger is a request | `SKILL.md` as written |
| 🗒 Typeset notes | `konspekt` | [konspekt.md](konspekt.md) and [safe.md](safe.md) | [konspekt.md](konspekt.md); only a phrase with the trigger is a request | [konspekt.md](konspekt.md) |
| 🎙 Voice only | `voice` | (not offered) | the **voice-control** skill: only phrases with the trigger | the **voice-control** skill: a dialogue that may hold commands |

The old ids still work: `drawing` is 📝 with commands, `safe` is 🧠 with commands off.

«Hold to talk» is not part of this: holding it is always his own explicit command, in every mode.

The option 🔎 Browser assistant (any mode with an agent) is [research-browser.md](research-browser.md).
