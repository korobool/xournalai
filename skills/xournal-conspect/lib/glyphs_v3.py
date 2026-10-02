"""Font v3 — the owner's OWN captured glyphs (glyphs_user_raw.json), cleaned up but not redesigned.

Per captured variant:
  1. metric normalisation: baseline 0, x-height 1 (ascenders ~1.65, descenders ~0.55)
  2. tremor removal: smoothing spline (scipy splprep) on each stroke — keeps the curve shapes, drops the wobble
  3. straightening: strokes/pieces that are nearly straight become exactly straight (stems, bars)
  4. common slant: every glyph is sheared to the library's median slant
  5. width pulled halfway toward the median width of that letter's variants
Then the (up to) 3 cleanest variants per letter are kept — real variation from their own hand.
Characters never captured fall back to v2 (glyphs_v2) until the owner writes a specimen sheet (capture_glyphs.py).
"""
import json, math, os
import numpy as np
from scipy.interpolate import splprep, splev

HERE = os.path.dirname(os.path.abspath(__file__))
SMALL = set("acemnorsuvwxz"); ASC = set("bdfhklt"); DESC = set("gpqy")
BAD_SRC = {"How", "skills,", "accent,"}
BAD_IDX = {("a", 7), ("e", 0), ("n", 3), ("n", 4), ("n", 6), ("n", 11), ("n", 14), ("i", 5), ("i", 7), ("c", 3), ("o", 1)}
SKIP = set("gp")            # captured 'g' is really "ng", 'p' reads as 'n'
SKIP_UPPER = set("EHI")     # E,H fragments only; their I is J-shaped (reads as J in caps words)

def _resample(p, step):
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
    if d[-1] < step * 2: return p, d[-1]
    t = np.r_[np.arange(0, d[-1], step), d[-1]]
    return np.c_[np.interp(t, d, p[:, 0]), np.interp(t, d, p[:, 1])], d[-1]

def _smooth(p, sigma=0.03):
    """Smoothing spline; sigma = expected tremor (x-height units)."""
    p = np.asarray(p, float)
    keep = np.r_[True, np.hypot(*np.diff(p, axis=0).T) > 1e-6]; p = p[keep]
    if len(p) < 5: return p
    p, L = _resample(p, 0.02)
    if len(p) < 5: return p
    try:
        tck, _ = splprep([p[:, 0], p[:, 1]], s=len(p) * sigma ** 2, k=3)
        n = max(12, int(L / 0.03))
        x, y = splev(np.linspace(0, 1, n), tck)
        q = np.c_[x, y]; q[0], q[-1] = p[0], p[-1]
        return q
    except Exception:
        return p

def _straighten(p, tol=0.045):
    """If the stroke is nearly straight, make it exactly straight."""
    if len(p) < 3: return p
    a, b = p[0], p[-1]; L = np.hypot(*(b - a))
    if L < 0.25: return p
    n = np.array([-(b - a)[1], (b - a)[0]]) / L
    dev = np.abs((p - a) @ n).max()
    if dev < tol * max(1.0, L):
        t = np.linspace(0, 1, max(8, len(p) // 2))[:, None]
        return a + (b - a) * t
    return p

def _slant(strokes):
    """Dominant slant angle (rad, + = leaning right) from the long near-vertical strokes."""
    angs, wts = [], []
    for s in strokes:
        d = s[-1] - s[0]; L = np.hypot(*d)
        if L > 0.5 and abs(d[1]) > 2 * abs(d[0]):
            angs.append(math.atan2(d[0], -d[1]) if d[1] < 0 else math.atan2(-d[0], d[1])); wts.append(L)
    return float(np.average(angs, weights=wts)) if angs else None

def _norm(ch, S):
    """Fit the letter's body exactly onto the guide lines (baseline 0, x-height -1, asc -1.65, cap -1.5, desc +0.55)."""
    allp = np.vstack(S); top, bot = allp[:, 1].min(), allp[:, 1].max()
    if ch in "ij":                                   # use the stem, keep the dot proportional
        main = max(S, key=lambda s: np.ptp(s[:, 1])); top, bot = main[:, 1].min(), main[:, 1].max()
        t_top, t_bot = -1.0, (0.0 if ch == "i" else 0.55)
    elif ch in SMALL: t_top, t_bot = -1.0, 0.0
    elif ch == "t": t_top, t_bot = -1.45, 0.0
    elif ch in ASC: t_top, t_bot = -1.65, 0.0
    elif ch in DESC: t_top, t_bot = -1.0, 0.55
    elif ch.isupper() or ch.isdigit(): t_top, t_bot = -1.5, 0.0
    else: return S
    k = (t_bot - t_top) / max(bot - top, 1e-3)
    k = float(np.clip(k, 0.5, 2.0))
    return [np.c_[s[:, 0] * k, (s[:, 1] - top) * k + t_top] for s in S]

# hand-picked good variants: raw indices into glyphs_user_raw.json[char] (review a variant sheet after recapturing)
PICK = {"a": [0, 1, 3, 8], "d": [1, 3], "e": [1, 2, 3, 4], "f": [0], "h": [1], "i": [2, 3], "l": [1, 2, 4],
        "n": [8, 13, 1], "o": [0, 2, 11], "r": [1, 2, 4, 6], "s": [0, 3, 7], "t": [0, 4, 6], "L": [0, 2]}
MAXW = 0.85   # round lowercase letters wider than this (x-height units) are narrowed — except m, w

def build(user_json=None, variants=3, target_slant=None, picks=PICK):
    import glyphstore
    raw = json.load(open(user_json or glyphstore.user_glyphs()))
    cleaned = {}
    for ch, vs in raw.items():
        if ch in SKIP or ch in SKIP_UPPER or not ch.isalnum(): continue
        for i, v in enumerate(vs):
            if v["src"] in BAD_SRC or (ch, i) in BAD_IDX or (v["src"] == "pronounciation" and ch != "p"): continue
            S = [np.asarray(s, float)[:, :2] for s in v["strokes"] if len(s) >= 2]
            if not S: continue
            S = _norm(ch, S)
            jitter = 0.0
            out = []
            for s in S:
                q = _smooth(s)
                if len(s) >= 5:   # tremor energy = distance between raw and smoothed (quality score)
                    r, _ = _resample(s, 0.02)
                    jitter += float(np.mean(np.min(np.hypot(r[:, None, 0] - q[None, :, 0], r[:, None, 1] - q[None, :, 1]), axis=1)))
                out.append(_straighten(q))
            cleaned.setdefault(ch, []).append({"S": out, "jit": jitter / len(S), "sl": _slant(out), "raw": i, "src": v["src"]})
    sl = [g["sl"] for vs in cleaned.values() for g in vs if g["sl"] is not None]
    target = target_slant if target_slant is not None else (float(np.median(sl)) if sl else 0.0)
    lib = {}
    for ch, vs in cleaned.items():
        for g in vs:   # shear to the common slant
            if g["sl"] is not None:
                k = math.tan(target) - math.tan(g["sl"])
                g["S"] = [np.c_[s[:, 0] - s[:, 1] * k, s[:, 1]] for s in g["S"]]
            x0 = min(s[:, 0].min() for s in g["S"]); g["S"] = [s - [x0, 0] for s in g["S"]]
            g["w"] = max(s[:, 0].max() for s in g["S"])
        med = float(np.median([g["w"] for g in vs]))
        for g in vs:   # pull width halfway toward the median of this letter
            if g["w"] > 0.15:
                fx = (0.5 * g["w"] + 0.5 * med) / g["w"]
                g["S"] = [np.c_[s[:, 0] * fx, s[:, 1]] for s in g["S"]]; g["w"] *= fx
        order = sorted(vs, key=lambda g: g["jit"] + 0.3 * abs(g["w"] - med))
        # picks refer to the STABLE raw index (position in glyphs_user_raw.json), not to the sorted order
        best = [g for g in vs if g["raw"] in picks[ch]] if picks and ch in picks else order[:variants]
        for g in best:
            if ch.islower() and ch not in "mw" and g["w"] > MAXW:
                fx = MAXW / g["w"]; g["S"] = [np.c_[s[:, 0] * fx, s[:, 1]] for s in g["S"]]; g["w"] = MAXW
        lib[ch] = [{"strokes": [s.tolist() for s in g["S"]], "w": float(g["w"])} for g in best]
    return lib, target
