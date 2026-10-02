#!/usr/bin/env python3
"""
Grow the owner's glyph library from strokes they wrote on the xournalai canvas.

usage: capture_glyphs.py ELEMENTS_JSON WORDS_JSON [--out FILE (default: $XOURNALAI_GLYPHS or ~/.local/share/xournalai/glyphs_user_raw.json)] [--merge]
  ELEMENTS_JSON : saved output of xournalai page_elements(detail="full", types=["stroke"])
  WORDS_JSON    : [["word", "e2156", "e2160"], ...]  — the text of each written word and the first/last
                  stroke id of that word (read ids off a page_render(highlight=...) to be sure).
Each word's strokes are clustered into letters by x-overlap (i-dots merged); a word is used only if the
cluster count equals its letter count. Glyphs are normalized to x-height 1 with baseline at y=0.
Check the result with:  python3 -c "from conspect import Sheet; s=Sheet(420,80); s.text(10,30,'abc...'); s.save('spec')"
"""
import json, os, sys, argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glyphstore
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("elements"); ap.add_argument("words")
ap.add_argument("--out", default=glyphstore.write_target())
ap.add_argument("--merge", action="store_true", help="append to the existing library instead of replacing it")
a = ap.parse_args()

E = {e["id"]: e for e in json.load(open(a.elements))["elements"]}
WORDS = json.load(open(a.words))
DESC = set("gpyqj,")

def ids_between(a_, b_):
    lo, hi = int(a_[1:]), int(b_[1:])
    return [f"e{i}" for i in range(lo, hi + 1) if f"e{i}" in E]

def clusters(ids):
    items = sorted(((i, np.array([q[:2] for q in E[i]["points"]])) for i in ids), key=lambda t: t[1][:, 0].min())
    cl = []
    for i, p in items:
        lo, hi = p[:, 0].min(), p[:, 0].max()
        if cl and lo < cl[-1]["hi"] - 0.35 * (hi - lo):
            cl[-1]["s"].append(p); cl[-1]["hi"] = max(cl[-1]["hi"], hi)
        else:
            cl.append({"lo": lo, "hi": hi, "s": [p]})
    return cl

lib = json.load(open(a.out)) if a.merge else {}
for w, first, last in WORDS:
    cl = clusters(ids_between(first, last)); letters = list(w)
    dim = lambda c: max(np.ptp(np.vstack(c["s"])[:, 0]), np.ptp(np.vstack(c["s"])[:, 1]))
    while len(cl) > len(letters):                       # merge dots / split strokes into a neighbour
        cand = [k for k in range(len(cl)) if not (k == len(cl) - 1 and letters[-1] in ",:;.")]
        k = min(cand, key=lambda k: dim(cl[k])); cx = np.vstack(cl[k]["s"])[:, 0].mean()
        j = min([j for j in (k - 1, k + 1) if 0 <= j < len(cl)], key=lambda j: abs((cl[j]["lo"] + cl[j]["hi"]) / 2 - cx))
        cl[j]["s"] += cl[k]["s"]; cl[j]["lo"] = min(cl[j]["lo"], cl[k]["lo"]); cl[j]["hi"] = max(cl[j]["hi"], cl[k]["hi"]); cl.pop(k)
    if len(cl) != len(letters):
        print(f"SKIP {w!r}: {len(cl)} clusters vs {len(letters)} letters"); continue
    bots = [max(s[:, 1].max() for s in c["s"]) for c in cl]
    base = np.median([b for b, ch in zip(bots, letters) if ch not in DESC and ch.isalpha()] or bots)
    xh = np.median([max(s[:, 1].max() for s in c["s"]) - min(s[:, 1].min() for s in c["s"])
                    for c, ch in zip(cl, letters) if ch in "aceimnorsuvwxz"] or [9])
    for c, ch in zip(cl, letters):
        g = [((s - [c["lo"], base]) / xh).round(4).tolist() for s in c["s"]]
        lib.setdefault(ch, []).append({"strokes": g, "w": (c["hi"] - c["lo"]) / xh, "src": w})
    print(f"ok {w!r}")
json.dump(lib, open(a.out, "w"))
print("letters:", "".join(sorted(lib)))
