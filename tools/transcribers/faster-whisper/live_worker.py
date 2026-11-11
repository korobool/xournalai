#!/usr/bin/env python3
"""xournalai live-notes recognizer: faster-whisper with the model loaded once.

Speaks the "stream" interface of docs/mcp/TRANSCRIBERS.md (protocol 1): JSON lines on stdin and stdout. Prints a
hello when the model is loaded, then answers every "transcribe" request with a "result" (or an "error") carrying
the same id. Exits at the end of its input. Logs go to stderr.

Usually run on a GPU server through ssh, e.g. in mcp.json:
    "transcriber": {"kind": "stream", "command": "ssh -T mainframe transcript/bin/live-worker.sh"}

Voice fingerprints (1.23.0, experimental): when speakers.py, its own venv (<root>/.venv-speakers) and a speaker model
(<root>/models/speakers/*.onnx) are there, the hello offers them and a request with "voices": true gets one per
segment (see the README). Without them, or when the helper fails, the text comes as before.
"""
import argparse
import base64
import io
import json
import os
import select
import subprocess
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


class Voices:
    """The voice-fingerprint helper (speakers.py, in a venv of its own): started at once; a failed or stuck one is
    ended, and started again for a later piece (three times at most)"""

    def __init__(self, cmd, timeout=20.0):
        self.cmd = cmd
        self.timeout = timeout
        self.proc = None
        self.model = None
        self.dim = None
        self.starts = 0
        self.n = 0
        self.start()

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def start(self):
        self.starts += 1
        try:
            self.proc = subprocess.Popen(self.cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
            hello = self.read(120)
            if hello.get("type") != "hello":
                raise RuntimeError(f"no hello: {hello}")
            self.model, self.dim = hello.get("model"), hello.get("dim")
        except Exception as e:
            log(f"voices: the helper did not start: {e}")
            self.stop()

    def stop(self):
        if self.proc is not None:
            try:
                self.proc.kill()
                self.proc.wait(5)
            except Exception:
                pass
        self.proc = None

    def read(self, timeout):
        ready, _, _ = select.select([self.proc.stdout], [], [], timeout)
        if not ready:
            raise TimeoutError(f"no answer in {timeout:.0f} s")
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError("the helper ended")
        return json.loads(line)

    def fingerprints(self, np, audio, spans):
        """One unit vector (or None) per [start, end] span of `audio` (float32, 16 kHz)"""
        if not self.alive():
            if self.starts >= 3:
                raise RuntimeError("the voice helper is not running")
            self.start()
            if not self.alive():
                raise RuntimeError("the voice helper did not start")
        self.n += 1
        rid = str(self.n)
        pcm = (np.clip(audio, -1, 1) * 32767).astype("<i2").tobytes()
        try:
            self.proc.stdin.write(json.dumps({"id": rid, "pcm16_b64": base64.b64encode(pcm).decode(),
                                              "spans": spans}) + "\n")
            self.proc.stdin.flush()
            while True:
                answer = self.read(self.timeout)
                if answer.get("id") == rid:
                    break
        except Exception:
            self.stop()  # (stuck or gone: a fresh one next time)
            raise
        if answer.get("error"):
            raise RuntimeError(answer["error"])
        return answer.get("voices") or []


def voices_for(voices, np, audio, segments, result):
    """Adds the fingerprints to a result: one per segment (0.5 s or longer) and one for the whole speech (1 s or
    longer); or why there are none"""
    if voices is None:
        result["voices_error"] = "this recognizer has no voice fingerprints"
        return
    if not segments:
        return
    spans = [[round(s.start, 3), round(s.end, 3)] for s in segments]
    whole = [min(s[0] for s in spans), max(s[1] for s in spans)]
    try:
        vs = voices.fingerprints(np, audio, spans + [whole])
    except Exception as e:
        result["voices_error"] = f"{type(e).__name__}: {e}"
        log(f"voices failed: {e}")
        return
    for seg, v in zip(result["segments"], vs):
        if v is not None:
            seg["voice"] = v
    if len(vs) == len(spans) + 1 and vs[-1] is not None and whole[1] - whole[0] >= 1.0:
        result["voice"] = vs[-1]


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
    p.add_argument("--voices", default="auto", choices=["auto", "off"],
                   help="voice fingerprints: auto (when the helper's venv and a model are there) or off")
    p.add_argument("--voices-python", default=str(root / ".venv-speakers" / "bin" / "python"))
    p.add_argument("--voices-model", default="", help="default: the first <root>/models/speakers/*.onnx")
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
    voices = None
    helper = Path(__file__).with_name("speakers.py")
    models = sorted((Path(args.models_dir) / "speakers").glob("*.onnx"))
    model_file = args.voices_model or (str(models[0]) if models else "")
    if args.voices == "auto" and helper.is_file() and os.path.isfile(args.voices_python) and model_file:
        voices = Voices([args.voices_python, "-u", str(helper), "--model", model_file])
        if not voices.alive():
            voices = None
    log(f"ready in {time.time() - t0:.1f}s" + (f"; voices: {voices.model}" if voices else "; no voices"))
    hello = {"type": "hello", "protocol": PROTOCOL, "audio": args.audio,
             "engine": f"faster-whisper {args.model}" + (f" ({gpu.splitlines()[0]})" if gpu else f" ({args.device})")}
    if voices:
        hello["voices"] = {"model": voices.model, "dim": voices.dim}
    send(hello)

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
                segments, info = run(info.language if language is None else language, None)  # (once more, no prompt)
                text = " ".join(s.text.strip() for s in segments).strip()
            if hallucinated(text):
                log(f"#{rid} dropped a hallucination: {text}")
                segments, text = [], ""
            result = {"type": "result", "id": rid, "text": text, "language": info.language,
                      "language_probability": round(float(info.language_probability), 3),
                      "segments": [{"start": round(s.start, 2), "end": round(s.end, 2), "text": s.text.strip()}
                                   for s in segments]}
            if req.get("voices") is True:
                voices_for(voices, np, audio, segments, result)
            result["elapsed"] = round(time.time() - t, 3)
            send(result)
            log(f"#{rid} {len(audio) / 16000:.1f}s -> {info.language} in {time.time() - t:.2f}s: {text[:60]}")
        except Exception as e:  # (this piece failed; the next ones go on)
            send({"type": "error", "id": rid, "message": f"{type(e).__name__}: {e}"})
            log(f"#{rid} failed: {e}")
    log("input ended: bye")


if __name__ == "__main__":
    main()
