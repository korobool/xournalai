#!/usr/bin/env python3
"""xournalai: voice fingerprints for the live worker (1.23.0, experimental; docs/mcp/TRANSCRIBERS.md, «Voice
fingerprints»).

A small helper that live_worker.py starts in a virtual environment of its own (sherpa-onnx and numpy only, no torch),
so that nothing of it touches the faster-whisper environment and a failure here never stops the text. It turns
stretches of speech into speaker embeddings ("voice fingerprints"): vectors that are close for the same voice and far
apart for different ones. They are not audio: the speech can't be got back from them.

JSON lines on stdin and stdout:
    → {"type": "hello", "model": "nemo_en_titanet_large", "dim": 192}            (once, when the model is loaded)
    ← {"id": "7", "pcm16_b64": "<16 kHz mono 16-bit PCM>", "spans": [[0.0, 3.2], [3.4, 5.0]]}
    → {"id": "7", "voices": [[0.0123, …], null]}   (unit length, one per span; null: a span too short or empty)
    → {"id": "7", "error": "…"}                     (this request failed; the next ones go on)
Logs go to stderr.
"""
import argparse
import base64
import json
import sys
from pathlib import Path

OUT = sys.stdout
sys.stdout = sys.stderr  # (nothing but protocol lines on the real stdout)
RATE = 16000


def send(obj):
    OUT.write(json.dumps(obj) + "\n")
    OUT.flush()


def log(msg):
    print(f"[voice] {msg}", file=sys.stderr, flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--model", required=True, help="a sherpa-onnx speaker-embedding model (.onnx)")
    p.add_argument("--threads", type=int, default=4)
    p.add_argument("--min-span", type=float, default=0.5, help="shorter spans get no fingerprint (seconds)")
    args = p.parse_args()

    import numpy as np
    import sherpa_onnx

    extractor = sherpa_onnx.SpeakerEmbeddingExtractor(
        sherpa_onnx.SpeakerEmbeddingExtractorConfig(model=args.model, num_threads=args.threads, provider="cpu"))
    name = Path(args.model).stem
    send({"type": "hello", "model": name, "dim": extractor.dim})
    log(f"{name} ({extractor.dim}) ready")

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rid = None
        try:
            req = json.loads(line)
            rid = req.get("id")
            pcm = np.frombuffer(base64.b64decode(req["pcm16_b64"]), dtype="<i2").astype(np.float32) / 32768.0
            voices = []
            for start, end in req.get("spans", []):
                a = pcm[max(0, int(start * RATE)):max(0, int(end * RATE))]
                if len(a) < args.min_span * RATE:
                    voices.append(None)
                    continue
                stream = extractor.create_stream()
                stream.accept_waveform(RATE, a)
                stream.input_finished()
                v = np.asarray(extractor.compute(stream), dtype=np.float64)
                n = float(np.linalg.norm(v))
                voices.append([round(float(x), 4) for x in v / n] if n > 0 else None)
            send({"id": rid, "voices": voices})
        except Exception as e:  # (this request failed; the next ones go on)
            send({"id": rid, "error": f"{type(e).__name__}: {e}"})
            log(f"#{rid} failed: {e}")


if __name__ == "__main__":
    main()
