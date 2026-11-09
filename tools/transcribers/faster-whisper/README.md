# faster-whisper live worker

A "stream" recognizer for xournalai's live notes (specification: [docs/mcp/TRANSCRIBERS.md](../../../docs/mcp/TRANSCRIBERS.md)).
It loads a Whisper model once and answers pieces as they come (about a second each for large-v3 on a GPU), in
any language (auto-detected; an unsure short piece takes the language of the piece before).

## Install (next to an existing faster-whisper setup)

The expected layout: `<root>/.venv` with `faster-whisper` installed, `<root>/models` (the Hugging Face cache).

```sh
scp live_worker.py live-worker.sh server:<root>/bin/
ssh server 'chmod +x <root>/bin/live-worker.sh'
```

Check it by hand (it prints a hello once the model is loaded; Ctrl+D ends it):

```sh
ssh -T server <root>/bin/live-worker.sh
```

## Use it in xournalai

In `~/.config/xournalpp/mcp.json` (the path may differ), under `assistant`:

```json
"live_notes": {"enabled": true, "transcriber": {"kind": "stream", "command": "ssh -T server <root>/bin/live-worker.sh"}}
```

Options (append to the command): `--model`, `--device`, `--compute-type`, `--beam-size`, `--no-vad`,
`--hint-below 0.5`, `--audio path` (when it runs on the same machine as xournalai).
