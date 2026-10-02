# Skills for xournalai

Reusable [Claude Code skills](https://docs.claude.com/en/docs/claude-code/skills) that draw on the xournalai canvas
through its MCP tools. Install them (links, so `git pull` keeps them current):

```sh
skills/install.sh
```

| Skill | What it does |
|---|---|
| [xournal-conspect](xournal-conspect/SKILL.md) | Study notes ("conspects") in a handwritten cheat-sheet style: your handwriting, code in boxes, LaTeX in wavy clouds, braces, connectors, mini plots. `lib/conspect.py` renders a page to SVG + LaTeX placements (+ PNG preview, or a native .xopp). Also: grow the handwriting library from what you write (`capture_glyphs.py`), beautify a handwritten line, turn a photo into a pencil sketch. |
| [conversation-topic-map](conversation-topic-map/SKILL.md) | A recording or conversation as a topic map: Gantt-style topic timeline, bubble cluster map, cluster cards, plus a notes file. `lib/topicmap.py data.json OUTDIR` draws any `data.json` (English or Russian labels). |
| [companion-bin/transcribe-remote.sh](companion-bin/transcribe-remote.sh) | The assistant's "remote transcriber first": sends a recording to your own GPU server over ssh. Configure it in `~/.config/xournalai/transcribe.conf`. |

**Your handwriting stays yours.** The captured glyphs are personal, so they are not in the repository:
`$XOURNALAI_GLYPHS`, else `~/.local/share/xournalai/glyphs_user_raw.json` (where `capture_glyphs.py` saves them).
Without them, conspects use a designed handwriting font.

Python 3 with numpy, scipy and Pillow; LaTeX (for formula previews) is optional.
