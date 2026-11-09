# Speech recognizers for xournalai ("transcribers")

Live notes (recording with transcription while you speak, `assistant.live_notes`) talk to a speech recognizer
through the small interface below and know no particular one, so a recognizer can be replaced (another server,
another model, a cloud service, a local one) by changing a setting, without changing the app. This file is the
specification of that interface: a recognizer that follows it works with xournalai. Version: **1**.

(Not yet behind this interface: Ask's dictation and the `audio_transcribe` tool use the app's built-in whisper.cpp
helper. Moving them here is planned, so that one setting chooses the engine everywhere.)

In the code: `src/core/assistant/Transcriber.h` (the interface and the factory), one class per kind
(`FileTranscriber.cpp`, `StreamTranscriber.cpp`). Adding a kind = a class, a line in `Transcriber::make`, a section
here, and tests.

## What xournalai sends

Pieces of the recording, cut at pauses (see `src/core/assistant/LiveChunker.h`):

- WAV files, PCM 16-bit, mono, 16 000 Hz, usually 2 to `hard_s` (25) s long;
- in order, one piece per request, numbered within the recording (`id` "1", "2", …);
- with a **language**: `"auto"` (detect it for this piece: live notes are multilingual) or the code the owner set;
- with a **language hint**: the language of the answers received before this piece was sent (with one piece at a
  time, the piece before; `""` at first). Short pieces are hard to detect: a recognizer should use the hint when its
  own detection is unsure (e.g. probability below 0.5); it may ignore it;
- with a **prompt**: the last 200 characters of the text received before this piece was sent, to keep spelling,
  names and the thread across the cut. Using it is the recognizer's choice. **Beware:** with Whisper large-v3
  (faster-whisper), passing it as `initial_prompt` made a quiet piece come back as a stock phrase («Продолжение
  следует…», «Субтитры сделал …») instead of its words (2026-11-09, the owner's recording); without the prompt the
  piece was right. The faster-whisper worker here ignores it unless started with `--use-prompt`.

## Choosing a recognizer: `mcp.json`

```json
"live_notes": {
  "enabled": true,
  "transcriber": {"kind": "stream", "command": "ssh -T mainframe transcript/bin/live-worker.sh", "timeout_s": 120},
  "pause_ms": 1500, "soft_s": 15, "hard_s": 25, "language": "auto"
}
```

- `pause_ms` 500–5000 (a pause this long ends a piece), `soft_s` 5–28 (from this length a piece ends at a short
  pause), `hard_s` `soft_s`–29 (it ends at this length anyway; Whisper-style models hear 30 s at a time), `language`
  `"auto"` or a code. Values out of range are brought into it.
- `transcriber` is kept **exactly as written**: the app never rewrites or drops fields in it (a recognizer's own
  settings can live there).

Settings every kind understands:

| field             | meaning                                                                              | default |
|-------------------|--------------------------------------------------------------------------------------|---------|
| `kind`            | `"file"` or `"stream"`                                                               | —       |
| `command`         | run through the owner's login shell (`$SHELL -l -c`), like the AI terminal's tabs: their `PATH`, ssh keys and agents | — |
| `timeout_s`       | a piece without an answer this long **after it was sent** fails (1–3600)             | 120     |
| `ready_timeout_s` | stream: the hello must come within this (1–3600)                                     | 120     |

Every command runs in a **process group of its own**: when xournalai ends it, what it started (ssh, scp, a model
server) ends too. Keep the login shell quiet: what a profile prints on stdout would be read as the recognizer's.

## `"kind": "file"`: one run per piece

For any existing command-line recognizer. `command` is a template; xournalai replaces

| placeholder       | with (each shell-quoted)                               |
|-------------------|--------------------------------------------------------|
| `{audio}`         | the piece's WAV file (absolute path)                   |
| `{language}`      | the language (`auto` or a code such as `ru`)           |
| `{language_hint}` | the language hint (may be empty)                       |
| `{prompt}`        | the prompt (may be empty)                              |
| `{id}`            | the piece's id                                         |

and runs it once per piece, one at a time. **Stdout is the transcript** (plain UTF-8 text; surrounding whitespace is
trimmed). Exit status 0 = success; anything else = an error for this piece (the last line of stderr is shown). A run
that takes longer than `timeout_s` is ended with its process group, the piece fails ("no answer in N s"), and the
next piece starts at once. Simple, but a recognizer that loads a model on every run is slow (seconds per piece).

Example (the owner's existing remote script, which copies the file to the server and runs the large model there):

```json
"transcriber": {"kind": "file", "command": "~/.local/share/xournalai/companion/bin/transcribe-remote.sh {audio}"}
```

## `"kind": "stream"`: one long-running process (fast)

For a recognizer that keeps its model loaded. xournalai starts `command` once per recording and talks to it with
**JSON lines**: one JSON object per line, UTF-8, `\n`-terminated, on its stdin and stdout. Stderr is not part of the
protocol (xournalai logs it; its last line explains a failure). The process may be remote: `ssh -T host worker`
carries the lines unchanged.

xournalai reads every line defensively: a field that is missing, `null` or of another type is treated as missing
(an `id` may be a number); a line that is not a JSON object is logged and ignored; lines of any other `type` are
ignored (room for progress messages later).

**1. Hello** (recognizer → xournalai), once, when it is ready (after loading its model):

```json
{"type": "hello", "protocol": 1, "engine": "faster-whisper large-v3 (A40)", "audio": "inline", "max_in_flight": 1}
```

- `protocol`: 1 (this specification). Another number is logged and xournalai goes on as with 1.
- `engine`: free text, shown to the owner.
- `audio`: how it wants the sound: `"path"` (it can read xournalai's files: a local recognizer) or `"inline"`
  (base64 in the request: a remote one). Default `"path"`.
- `max_in_flight`: how many pieces it may be sent before answering (1–16; default 1). Pieces beyond that wait in
  xournalai, in order, so a piece's `timeout_s` counts only its own recognition.

Requests made before the hello wait. No hello within `ready_timeout_s`, or an exit before the hello: the waiting
pieces fail (with the last line of stderr), and the process is not restarted for them.

**2. Request** (xournalai → recognizer), one per piece:

```json
{"type": "transcribe", "id": "7", "audio_path": "/home/…/live/2026-11-09_14-30-00/piece-0007.wav",
 "audio_b64": "UklGR…", "language": "auto", "language_hint": "ru", "prompt": "…the end of the text so far"}
```

- `id`: a string, unique within the recording; echoed in the answer.
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

- `text` is the transcript (missing = empty: nothing intelligible); the rest is optional. `segments` times are
  seconds within the piece. `elapsed`: seconds spent.
- An answer to a piece that already timed out is ignored.

**4. Failures.**
- A piece without an answer `timeout_s` after it was sent fails ("no answer in N s"), and the recognizer is
  considered stuck: it is ended (its process group) and a fresh one takes the remaining pieces.
- If the process exits after its hello, the pieces sent to it fail ("the recognizer ended: <last stderr line>") and
  a fresh process takes the remaining ones.

**5. End.** When the recording has ended and every piece is answered, xournalai closes the recognizer's stdin: it
should exit. If it hasn't after 10 s, its process group gets SIGTERM, and SIGKILL 5 s later (a hung worker must not
keep its GPU).

## What is kept

- Every piece stays as a WAV file in the recording's `live/<recording>/` folder (failed ones can be transcribed
  later).
- The text, piece by piece as it is written, in `transcripts/<recording>.live.txt` (`[m:ss] text`; a failed piece as
  `(not recognized: …)`), next to the recording; kept if the app exits.
- The whole recording is saved as usual and can be transcribed as a whole afterwards.

## Measured (2026-11-09)

The owner's 70 s recording (Russian, a quiet built-in microphone), fed in real time, the faster-whisper worker on an
A40 over ssh: 5 pieces of ~15 s (a monologue: cut at short pauses), every piece right, the last text in 0.6 s after
the sound ended; the worker's hello 5–6 s after start (the model loading).

## Recognizers in this repository

- `tools/transcribers/faster-whisper/live_worker.py`: a stream recognizer (faster-whisper; the model loaded once;
  stock phrases Whisper says over silence are dropped). Install it next to an existing faster-whisper setup and point
  `command` at it, e.g. through ssh.
- Tests use fake recognizers of both kinds, including misbehaving ones (`test/mcp_integration/test_e15_live_notes*.py`).

## Changing this specification

Bump the version above, keep version 1 working where possible (the `protocol` in the hello lets xournalai tell them
apart), and update both sides and the tests in the same change.
