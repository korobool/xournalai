#!/usr/bin/env python3
"""Beautify handwritten text lines while keeping the writer's own glyph shapes.

usage: beautify.py elements.json spec.json out.svg
elements.json: output of xournalai page_elements(detail=full)
spec.json: {"lines": [ {"ids": [...stroke ids in writing order...],
                          "remove": [...ids to drop, e.g. wrong letters...],
                          "insert": [ {"after": id_or_null, "glyph": [ids copied from elsewhere], "gap": pts} ],
                          "color": "#rrggbb" (optional) } ]}
Writes an SVG (1 unit = 1 pt, absolute page coords) with one smoothed path per stroke.
"""
import json, sys, math
import numpy as np
from scipy.ndimage import gaussian_filter1d

els = json.load(open(sys.argv[1]))
els = els["elements"] if isinstance(els, dict) else els
byid = {e["id"]: e for e in els}
spec = json.load(open(sys.argv[2]))

def pts(e):
    p = np.array([[q[0], q[1]] for q in e["points"]], float)
    w = np.full(len(p), float(e.get("width", 1.4)))
    return p, w

def resample(p, w, step=0.6):
    if len(p) < 2:
        return p, w
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
    if d[-1] < step:
        return p, w
    t = np.arange(0, d[-1], step)
    t = np.r_[t, d[-1]]
    return np.c_[np.interp(t, d, p[:, 0]), np.interp(t, d, p[:, 1])], np.interp(t, d, w)

def smooth(p, w, sigma=1.6):
    if len(p) < 5:
        return p, w
    q = p.copy()
    q[:, 0] = gaussian_filter1d(p[:, 0], sigma, mode="nearest")
    q[:, 1] = gaussian_filter1d(p[:, 1], sigma, mode="nearest")
    q[0], q[-1] = p[0], p[-1]
    return q, gaussian_filter1d(w, sigma, mode="nearest")

def bbox(strokes):
    a = np.vstack([s[0] for s in strokes])
    return a[:, 0].min(), a[:, 1].min(), a[:, 0].max(), a[:, 1].max()

out = []
for L in spec["lines"]:
    remove = set(L.get("remove", []))
    ids = [i for i in L["ids"] if i not in remove and i in byid]
    color = L.get("color") or byid[ids[0]].get("color", "#008000")[:7]
    # clusters = glyphs (strokes overlapping in x)
    items = []
    for i in ids:
        p, w = pts(byid[i]); items.append({"id": i, "p": p, "w": w})
    # inserted glyphs (copied from elsewhere), positioned after anchor
    for ins in L.get("insert", []):
        if "strokes" in ins:  # synthesized glyph, absolute page coords
            for k, s in enumerate(ins["strokes"]):
                p = np.array(s, float); items.append({"id": "syn%d" % k, "p": p, "w": np.full(len(p), ins.get("width", 0.85))})
            continue
        g = [pts(byid[i]) for i in ins["glyph"]]
        gx0, gy0, gx1, gy1 = bbox(g)
        # target scale: median glyph height of the line
        hs = [s["p"][:, 1].max() - s["p"][:, 1].min() for s in items]
        scale = ins.get("scale", 1.0)
        if ins.get("after") in [s["id"] for s in items]:
            a = next(s for s in items if s["id"] == ins["after"])
            ax = a["p"][:, 0].max() + ins.get("gap", 2.0)
            ay = a["p"][:, 1].max()
        else:
            ax, ay = ins["x"], ins["y"]
        for (p, w), gid in zip(g, ins["glyph"]):
            q = (p - [gx0, gy1]) * scale + [ax, ay + ins.get("dy", 0)]
            items.append({"id": "ins_" + gid, "p": q, "w": w, "shift_after": (gx1 - gx0) * scale + ins.get("gap", 2.0)})
    # deskew: fit baseline through stroke bottoms (robust, drop dots/descenders)
    bots = np.array([[s["p"][:, 0].mean(), s["p"][:, 1].max()] for s in items])
    hts = np.array([np.ptp(s["p"][:, 1]) for s in items])
    keep = hts > np.median(hts) * 0.4
    if keep.sum() >= 3:
        k, b = np.polyfit(bots[keep, 0], bots[keep, 1], 1)
        k = float(np.clip(k, -0.25, 0.25))
    else:
        k, b = 0.0, bots[:, 1].mean()
    x0 = bots[:, 0].min()
    ang = -math.atan(k)
    c, s_ = math.cos(ang), math.sin(ang)
    piv = np.array([x0, k * x0 + b])
    for it in items:
        d = it["p"] - piv
        it["p"] = np.c_[d[:, 0] * c - d[:, 1] * s_, d[:, 0] * s_ + d[:, 1] * c] + piv
    # glyph clustering by x overlap, then even out gaps (pull 60% toward median gap)
    items.sort(key=lambda s: s["p"][:, 0].min())
    clusters = []
    for it in items:
        lo, hi = it["p"][:, 0].min(), it["p"][:, 0].max()
        if clusters and lo < clusters[-1]["hi"] - 0.35 * (hi - lo):
            clusters[-1]["m"].append(it); clusters[-1]["hi"] = max(clusters[-1]["hi"], hi)
        else:
            clusters.append({"lo": lo, "hi": hi, "m": [it]})
    gaps = [clusters[i + 1]["lo"] - clusters[i]["hi"] for i in range(len(clusters) - 1)]
    if gaps:
        small = [g for g in gaps if g < np.median(gaps) * 2.5]  # word gaps excluded
        med = float(np.median(small)) if small else 0
        shift = 0.0
        for i, cl in enumerate(clusters):
            for it in cl["m"]:
                it["p"] = it["p"] + [shift, 0]
            if i < len(gaps):
                g = gaps[i]
                tgt = g if g >= med * 2.5 else g + 0.6 * (med - g)
                shift += tgt - g
    for it in items:
        p, w = resample(it["p"], it["w"])
        p, w = smooth(p, w)
        width = float(np.median(w)) if len(w) else 1.4
        d = "M" + " L".join("%.2f,%.2f" % tuple(q) for q in p)
        out.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{width:.2f}" stroke-linecap="round" stroke-linejoin="round"/>')

open(sys.argv[3], "w").write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 595.28 841.89" width="595.28" height="841.89">' + "".join(out) + "</svg>")
print(len(out), "paths")
