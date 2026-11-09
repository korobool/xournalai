#!/usr/bin/env python3
"""xournalai live-notes recognizer: faster-whisper with the model loaded once.

Speaks the "stream" interface of docs/mcp/TRANSCRIBERS.md (protocol 1): JSON lines on stdin and stdout. Prints a
hello when the model is loaded, then answers every "transcribe" request with a "result" (or an "error") carrying
the same id. Exits at the end of its input. Logs go to stderr.

Usually run on a GPU server through ssh, e.g. in mcp.json:
    "transcriber": {"kind": "stream", "command": "ssh -T mainframe transcript/bin/live-worker.sh"}
"""
import argparse
import base64
import io
import json
import os
import sys
import time
import wave
from pathlib import Path

PROTOCOL = 1
OUT = sys.stdout
sys.stdout = sys.stderr  # (nothing but protocol lines on the real stdout: libraries print)


def send(obj):
    OUT.write(json.dumps(obj, ensure_ascii=False) + "\n")
    OUT.flush()


def log(msg):
    print(f"[live ] {msg}", file=sys.stderr, flush=True)


# What Whisper says over silence or a quiet stretch it can't hear (learned from subtitled videos): never real speech
HALLUCINATIONS = ("субтитры сделал", "субтитры создавал", "редактор субтитров", "корректор", "продолжение следует",
                  "спасибо за просмотр", "thanks for watching", "thank you for watching", "subtitles by",
                  "amara.org", "дима торжок", "dimatorzok")


def hallucinated(text):
    t = text.lower().strip(" .!…")
    return bool(t) and len(t) < 80 and any(h in t for h in HALLUCINATIONS)


def load_audio(req, np):
    """The piece as float32 mono 16 kHz"""
    if req.get("audio_b64"):
        data = base64.b64decode(req["audio_b64"])
        with wave.open(io.BytesIO(data)) as w:
            if w.getsampwidth() != 2 or w.getnchannels() != 1 or w.getframerate() != 16000:
                raise ValueError("expected WAV PCM 16-bit mono 16 kHz")
            pcm = w.readframes(w.getnframes())
        return np.frombuffer(pcm, dtype="<i2").astype(np.float32) / 32768.0
    path = req.get("audio_path") or ""
    if not path or not os.path.isfile(path):
        raise FileNotFoundError(f"no audio: {path!r} (a remote recognizer needs \"audio\": \"inline\")")
    from faster_whisper import decode_audio
    return decode_audio(path, sampling_rate=16000)


def main():
    root = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("-m", "--model", default="large-v3")
    p.add_argument("-d", "--device", default="cuda", choices=["cuda", "cpu", "auto"])
    p.add_argument("-c", "--compute-type", default="float16")
    p.add_argument("--beam-size", type=int, default=5)
    p.add_argument("--models-dir", default=str(root / "models"))
    p.add_argument("--audio", default="inline", choices=["inline", "path"],
                   help="how it wants the sound: inline (base64, for a remote worker) or path (same machine)")
    p.add_argument("--no-vad", action="store_true", help="don't drop non-speech inside a piece")
    p.add_argument("--use-prompt", action="store_true",
                   help="pass the request's prompt (the previous text) to Whisper; off by default: with large-v3 it "
                        "made a quiet piece come back as 'Продолжение следует…' (2026-11-09)")
    p.add_argument("--hint-below", type=float, default=0.5,
                   help="detected language less sure than this: use the request's language_hint instead")
    args = p.parse_args()

    os.environ.setdefault("HF_HUB_CACHE", args.models_dir)
    os.environ.setdefault("HF_HOME", args.models_dir)
    import numpy as np
    from faster_whisper import WhisperModel

    t0 = time.time()
    log(f"loading {args.model} on {args.device} ({args.compute_type})")
    model = WhisperModel(args.model, device=args.device, compute_type=args.compute_type,
                         download_root=args.models_dir)
    gpu = ""
    try:
        import ctranslate2
        if args.device != "cpu" and ctranslate2.get_cuda_device_count() > 0:
            gpu = os.popen("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null").read().strip()
    except Exception:
        pass
    log(f"ready in {time.time() - t0:.1f}s")
    send({"type": "hello", "protocol": PROTOCOL, "audio": args.audio,
          "engine": f"faster-whisper {args.model}" + (f" ({gpu.splitlines()[0]})" if gpu else f" ({args.device})")})

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            log(f"not JSON: {line[:80]}")
            continue
        if req.get("type") != "transcribe":
            continue
        rid = str(req.get("id", ""))
        t = time.time()
        try:
            audio = load_audio(req, np)
            lang = req.get("language") or "auto"
            hint = req.get("language_hint") or ""
            prompt = (req.get("prompt") or None) if args.use_prompt else None
            language = None if lang == "auto" else lang

            def run(language, prompt):
                kw = dict(beam_size=args.beam_size, vad_filter=not args.no_vad, condition_on_previous_text=False,
                          initial_prompt=prompt)
                segs, inf = model.transcribe(audio, language=language, **kw)
                return list(segs), inf

            segments, info = run(language, prompt)
            if language is None and hint and info.language != hint and info.language_probability < args.hint_below:
                segments, info = run(hint, prompt)  # (a short piece, unsure: the language of the piece before)
            text = " ".join(s.text.strip() for s in segments).strip()
            if hallucinated(text) and prompt:
                segments, info = run(language, None)  # (the prompt can cause it: once more without)
                text = " ".join(s.text.strip() for s in segments).strip()
            if hallucinated(text):
                log(f"#{rid} dropped a hallucination: {text}")
                segments, text = [], ""
            send({"type": "result", "id": rid, "text": text, "language": info.language,
                  "language_probability": round(float(info.language_probability), 3),
                  "segments": [{"start": round(s.start, 2), "end": round(s.end, 2), "text": s.text.strip()}
                               for s in segments],
                  "elapsed": round(time.time() - t, 3)})
            log(f"#{rid} {len(audio) / 16000:.1f}s -> {info.language} in {time.time() - t:.2f}s: {text[:60]}")
        except Exception as e:  # (this piece failed; the next ones go on)
            send({"type": "error", "id": rid, "message": f"{type(e).__name__}: {e}"})
            log(f"#{rid} failed: {e}")
    log("input ended: bye")


if __name__ == "__main__":
    main()
