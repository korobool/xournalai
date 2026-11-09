# Speech recognizers for xournalai ("transcribers")

xournalai does not contain or know any particular speech recognizer. It talks to one through the small interface
below, so a recognizer can be replaced (another server, another model, a cloud service, a local one) by changing a
setting, without changing the app. This file is the specification of that interface: a recognizer that follows it
works with xournalai.

Used today by **live notes** (recording with transcription while you speak, `assistant.live_notes`). Version of this
specification: **1**.

## What xournalai sends

Pieces of the recording, cut at pauses (see `src/core/assistant/LiveChunker.h`):

- WAV files, PCM 16-bit, mono, 16 000 Hz, usually 2 to 25 s long (Whisper-style models hear 30 s at a time);
- in order, one piece per request, numbered within the recording;
- with a **language** (`"auto"` unless the owner set one) and a **prompt**: the end of the previous piece's text
  (up to 200 characters), to keep spelling, names and the thread across the cut. A recognizer may ignore it.

Whether to use the prompt is the recognizer's choice. **Beware:** with Whisper large-v3 (faster-whisper), passing it
as `initial_prompt` made a quiet piece come back as a stock phrase («Продолжение следует…», «Субтитры сделал …»)
instead of its words (2026-11-09, the owner's recording); without the prompt the piece was right. The faster-whisper
worker here ignores it unless started with `--use-prompt`, and drops such stock phrases.

Live notes are multilingual: `"auto"` means "detect the language of this piece". Short pieces are hard to detect,
so the request also carries `language_hint`: the language found in the previous piece. A recognizer should use the
hint when its own detection is unsure (e.g. probability below 0.5); it may ignore it.

## Two kinds of recognizers

The setting `assistant.live_notes.transcriber` in `mcp.json` names one:

```json
"live_notes": {
  "enabled": true,
  "transcriber": {"kind": "stream", "command": "ssh -T mainframe transcript/bin/live-worker.sh"},
  "pause_ms": 1500, "soft_s": 15, "hard_s": 25, "language": "auto"
}
```

`command` is run through the owner's login shell (`$SHELL -l -c`), like the AI terminal's tabs, so `PATH`, ssh keys
and agents are the owner's.

### `"kind": "file"`: one run per piece

For any existing command-line recognizer. `command` is a template; xournalai replaces

| placeholder  | with                                                        |
|--------------|-------------------------------------------------------------|
| `{audio}`    | the piece's WAV file (absolute path, shell-quoted)          |
| `{language}` | the language (`auto` or a code such as `ru`), shell-quoted   |
| `{prompt}`   | the prompt (may be empty), shell-quoted                     |

and runs it once per piece, one at a time. **Stdout is the transcript** (plain UTF-8 text; surrounding whitespace is
trimmed). Exit status 0 = success; anything else = an error for this piece (the last line of stderr is shown).
Simple, but a recognizer that loads a model on every run is slow (seconds per piece).

Example (the owner's existing remote script, which copies the file to the server and runs the large model there):

```json
"transcriber": {"kind": "file", "command": "~/.local/share/xournalai/companion/bin/transcribe-remote.sh {audio}"}
```

### `"kind": "stream"`: one long-running process (fast)

For a recognizer that keeps its model loaded. xournalai starts `command` once per recording and talks to it with
**JSON lines**: one JSON object per line, UTF-8, `\n`-terminated, on its stdin and stdout. Stderr is not part of the
protocol (xournalai logs it). The process may be remote: `ssh -T host worker` carries the lines unchanged.

**1. Hello** (recognizer → xournalai), once, when it is ready (after loading its model):

```json
{"type": "hello", "protocol": 1, "engine": "faster-whisper large-v3 (A40)", "audio": "inline"}
```

- `protocol`: 1 (this specification).
- `engine`: free text, shown to the owner.
- `audio`: how it wants the sound: `"path"` (it can read xournalai's files: a local recognizer) or `"inline"`
  (base64 in the request: a remote one). Default `"path"`.

xournalai waits up to 120 s for the hello; requests made meanwhile wait in order.

**2. Request** (xournalai → recognizer), one per piece:

```json
{"type": "transcribe", "id": "7", "audio_path": "/home/…/live/2026-11-09_14-30-00/piece-0007.wav",
 "audio_b64": "UklGR…", "language": "auto", "language_hint": "ru", "prompt": "…the end of the previous text"}
```

- `id`: a string, unique within the process; echoed in the answer.
- `audio_path` always; `audio_b64` (the whole WAV file, base64) only when the hello said `"inline"`.
- `language`: `"auto"` or a code. `language_hint`: a code or `""`. `prompt`: text or `""`.

**3. Answer** (recognizer → xournalai), one per request, in any order:

```json
{"type": "result", "id": "7", "text": "…", "language": "ru", "language_probability": 0.97,
 "segments": [{"start": 0.0, "end": 3.2, "text": "…"}], "elapsed": 0.9}
```

or, when this piece failed (the process goes on):

```json
{"type": "error", "id": "7", "message": "…"}
```

- `text` is required; the rest is optional. `segments` times are seconds within the piece. `elapsed`: seconds spent.
- An empty `text` is fine (nothing intelligible).

**4. End.** When the recording ends and every answer is in (or after 120 s), xournalai closes the recognizer's
stdin; the recognizer exits. If it exits early, the pieces without an answer are marked failed and the recording's
remaining pieces go to a fresh process.

Lines of any other `type` are ignored (room for progress messages later). A line that isn't JSON is logged and
ignored.

## Timeouts and failures

- A piece without an answer after 120 s is marked failed; later pieces go on.
- Failed pieces keep their WAV file (in the recording's `live/` folder); they can be transcribed later.
- The whole recording is still saved as usual and can be transcribed as a whole afterwards.

## Measured (2026-11-09)

The owner's 70 s recording (Russian, a quiet built-in microphone), fed in real time, the faster-whisper worker on an
A40 over ssh: 5 pieces of ~15 s (a monologue: cut at short pauses), every piece right, the last text in 0.6 s after
the sound ended; the worker's hello 5–6 s after start (the model loading).

## Recognizers in this repository

- `tools/transcribers/faster-whisper/live_worker.py`: a stream recognizer (faster-whisper; the model loaded once).
  Install it next to an existing faster-whisper setup and point `command` at it, e.g. through ssh.
- Tests use fake recognizers of both kinds (`test/mcp_integration/test_e15_live_notes.py`).

## Changing this specification

Bump the version above, keep version 1 working where possible (a `protocol` in the hello lets xournalai tell them
apart), and update both sides and the tests in the same change.
