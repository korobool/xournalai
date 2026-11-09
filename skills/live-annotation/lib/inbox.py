#!/usr/bin/env python3
"""Per-topic inboxes for live notes: the coordinator routes transcript pieces to topic agents.

Folder layout (DIR = ~/.local/share/xournalai/companion/live/<recording stem>/):
  inbox/T3.jsonl    one JSON object per line, appended by the coordinator
  cursor/T3         how many lines the T3 agent has taken (written only by that agent)
  topics/T3.json    the agent's card status (bbox, last_y, need_more, ...), written by the agent

usage:
  inbox.py add DIR T3 '{"n":12,"at":"2:05","kind":"material","text":"..."}'   (or the JSON on stdin)
  inbox.py take DIR T3          -> JSON list of the new items, and marks them taken
  inbox.py peek DIR T3          -> the new items, without taking them
  inbox.py pending DIR          -> {"T3": 2, ...}: items not taken yet, per topic
  inbox.py status DIR T3 '{"bbox":[24,36,186,120],"last_y":150}'   (merge into topics/T3.json)
  inbox.py wait DIR T3 [SECONDS]  -> blocks until T3 has new items (prints "NEW"), the recording ended and
                                     nothing is waiting ("ENDED"), or the time is up ("TIMEOUT"); default 100 s
  inbox.py end DIR                -> marks the recording as ended (the coordinator, after the last piece)
"""
import fcntl
import json
import pathlib
import sys
import time


def _lines(path):
    if not path.exists():
        return []
    return [json.loads(l) for l in path.read_text().splitlines() if l.strip()]


def _cursor(d, t):
    c = d / "cursor" / t
    return int(c.read_text().strip() or 0) if c.exists() else 0


def main(argv):
    if len(argv) < 3:
        sys.exit(__doc__)
    op, d = argv[1], pathlib.Path(argv[2]).expanduser()
    for sub in ("inbox", "cursor", "topics"):
        (d / sub).mkdir(parents=True, exist_ok=True)
    if op == "end":
        (d / "ended").write_text(time.strftime("%Y-%m-%d %H:%M:%S"))
        print("ENDED")
        return
    if op == "wait":
        t = argv[3]
        limit = float(argv[4]) if len(argv) > 4 else 100.0
        box, start = d / "inbox" / f"{t}.jsonl", time.time()
        while True:
            if len(_lines(box)) > _cursor(d, t):
                print("NEW")
                return
            if (d / "ended").exists():
                print("ENDED")
                return
            if time.time() - start > limit:
                print("TIMEOUT")
                return
            time.sleep(2)
    lock = open(d / ".lock", "w")
    fcntl.flock(lock, fcntl.LOCK_EX)
    try:
        if op == "pending":
            out = {}
            for f in sorted((d / "inbox").glob("*.jsonl")):
                t = f.stem
                n = len(_lines(f)) - _cursor(d, t)
                if n > 0:
                    out[t] = n
            print(json.dumps(out))
            return
        t = argv[3]
        box = d / "inbox" / f"{t}.jsonl"
        if op == "add":
            raw = argv[4] if len(argv) > 4 else sys.stdin.read()
            item = json.loads(raw)
            with open(box, "a") as f:
                f.write(json.dumps(item, ensure_ascii=False) + "\n")
            print(json.dumps({"topic": t, "queued": len(_lines(box)) - _cursor(d, t)}))
        elif op in ("take", "peek"):
            items, c = _lines(box), _cursor(d, t)
            new = items[c:]
            if op == "take":
                (d / "cursor" / t).write_text(str(len(items)))
            print(json.dumps(new, ensure_ascii=False))
        elif op == "status":
            p = d / "topics" / f"{t}.json"
            cur = json.loads(p.read_text()) if p.exists() else {}
            if len(argv) > 4:
                cur.update(json.loads(argv[4]))
                p.write_text(json.dumps(cur, ensure_ascii=False))
            print(json.dumps(cur, ensure_ascii=False))
        else:
            sys.exit(__doc__)
    finally:
        fcntl.flock(lock, fcntl.LOCK_UN)


if __name__ == "__main__":
    main(sys.argv)
