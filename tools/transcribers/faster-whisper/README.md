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
`--hint-below 0.5`, `--audio path` (when it runs on the same machine as xournalai), `--use-prompt` (pass the previous
text as Whisper's initial prompt: off, as it made large-v3 answer a quiet piece with a stock phrase).

Stock phrases Whisper produces over silence («Субтитры сделал …», «Продолжение следует…», "Thanks for watching") are
dropped (an empty text), after one more try without the prompt.

## Voice fingerprints (optional; xournalai 1.23.0, experimental)

For «👥 Speakers» and «🎤 Only my voice» the worker can give a voice fingerprint for each segment (docs/mcp/TRANSCRIBERS.md,
«Voice fingerprints»). It does so through `speakers.py`, a helper in a **venv of its own**, so the faster-whisper
venv is not touched and a failure there never stops the text:

```sh
scp speakers.py server:<root>/bin/
ssh server 'cd <root> && "$(.venv/bin/python -c "import sys; print(sys._base_executable)")" -m venv .venv-speakers &&
  .venv-speakers/bin/pip install sherpa-onnx numpy &&
  mkdir -p models/speakers && curl -sSfL -o models/speakers/nemo_en_titanet_large.onnx \
  https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/nemo_en_titanet_large.onnx'
```

- The venv is made with the same Python as the faster-whisper one (on the owner's server the system `python3` has no
  `ensurepip`, and installing it needs sudo; pyenv's Python has it).
- About 125 MB for the venv and 100 MB for the model; it runs on the CPU (about 0.1 s per second of speech).
- The model: NVIDIA TitaNet-L (192 numbers per voice). Of five models tried on a public two-speaker recording
  (pyannote's sample, 11 Nov 2026) it kept the voices furthest apart: the same speaker 0.51–0.88, different ones
  at most 0.43 (WeSpeaker ResNet34: 0.49–0.89 against up to 0.73, overlapping).
- The worker finds it by itself (`<root>/.venv-speakers/bin/python`, the first `<root>/models/speakers/*.onnx`) and
  says so in its log (`ready …; voices: nemo_en_titanet_large`). `--voices off` turns it off; `--voices-python` and
  `--voices-model` point elsewhere.
