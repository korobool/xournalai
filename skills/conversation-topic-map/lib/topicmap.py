#!/usr/bin/env python3
"""Topic map of a conversation for the xournalai canvas (A4 landscape, 842 x 595 pt): a Gantt-style topic timeline,
a bubble map of the clusters (size = share of talk time, dashed lines = related) and one card per cluster.

    python3 topicmap.py data.json OUTDIR

writes OUTDIR/body.svg (lines, bars, bubbles: create_from_svg) and OUTDIR/texts.json (every label, typed in Sans:
create_text with texts=[...]), plus OUTDIR/title.svg in the owner's handwriting when the title is Latin text and
xournal-conspect is installed next to this skill (else the title is typed too).

data.json (see ../example/data.json):
{
  "title": "Team retro: mobile app launch",
  "lang": "en",                         # "en" or "ru": the section labels
  "duration_s": 2100,                   # optional: else the end of the last range
  "source": "notes.md · Whisper large-v3",
  "clusters": [
    {"key": "C1", "name": "...", "color": "#1f6fb2",
     "ranges": [[0, 180], [900, 1020]],          # seconds of the conversation about it
     "keywords": ["..."], "points": ["..."], "quotes": ["..."],
     "actions": ["✓ decided ...", "? open ..."],   # ✓ = decision/action (shown on the card), ? = open question
     "aside": false},                             # true: breaks, calls (timeline only, no bubble or card)
    ...
  ],
  "links": [["C1", "C2"], ...]          # optional: related clusters (default: consecutive in time)
}
"""
import json
import math
import os
import sys

W_PAGE, H_PAGE = 842, 595
HERE = os.path.dirname(os.path.abspath(__file__))
CONSPECT = os.path.join(HERE, "..", "..", "xournal-conspect", "lib")

LABELS = {
    "en": {"timeline": "TOPIC TIMELINE (0 → {m} min)", "map": "CLUSTER MAP (bubble size = share of talk time; lines = "
           "related)", "cards": "CLUSTER CARDS", "min": "{m} min", "source": "source: {s}"},
    "ru": {"timeline": "ХРОНОЛОГИЯ ТЕМ (0 → {m} мин)", "map": "КАРТА КЛАСТЕРОВ (размер пузыря = доля времени "
           "разговора; линии = связь)", "cards": "КАРТОЧКИ КЛАСТЕРОВ", "min": "{m} мин", "source": "источник: {s}"},
}

_font = None


def text_width(s, size):
    """Width of `s` in Sans at `size` pt (DejaVu metrics when available, else an estimate)"""
    global _font
    if _font is None:
        try:
            from PIL import ImageFont
            _font = ImageFont.truetype("DejaVuSans.ttf", 100)
        except Exception:
            try:
                from PIL import ImageFont
                _font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 100)
            except Exception:
                _font = False
    return _font.getlength(s) * size / 100 if _font else len(s) * size * 0.55


def fit(s, size, width, smallest=3.4):
    """The largest size <= `size` at which `s` fits `width`"""
    while text_width(s, size) > width and size > smallest:
        size -= 0.1
    return round(size, 1)


def wrap(s, size, width):
    out, cur = [], ""
    for word in s.split():
        t = (cur + " " + word).strip()
        if text_width(t, size) <= width or not cur:
            cur = t
        else:
            out.append(cur)
            cur = word
    if cur:
        out.append(cur)
    return out


def build(data):
    """(svg elements, texts, title) for `data`"""
    L = LABELS.get(data.get("lang", "en"), LABELS["en"])
    C = data["clusters"]
    col = {c["key"]: c.get("color", "#555555") for c in C}
    total = data.get("duration_s") or max(b for c in C for _, b in c["ranges"])
    talk = {c["key"]: sum(b - a for a, b in c["ranges"]) for c in C}
    whole = sum(talk.values()) or 1
    pct = {k: round(100 * v / whole) for k, v in talk.items()}
    S, T = [], []

    def tx(x, y, s, c="#222222", sz=5):
        T.append({"x": round(x, 1), "y": round(y, 1), "text": s, "color": c, "font": {"name": "Sans", "size": sz}})

    # Timeline: one row per cluster, a bar per range
    X0, X1, Y, LH = 150, 818, 46, 6.4
    sc = (X1 - X0) / total
    minutes = max(1, math.ceil(total / 60))
    tx(24, Y - 9, L["timeline"].format(m=minutes), "#555555", 5.5)
    for i, c in enumerate(C):
        y = Y + i * LH
        label = "%s %s" % (c["key"], c["name"])
        tx(24, y - 1, label, "#888888" if c.get("aside") else col[c["key"]], fit(label, 4.6, 122))
        S.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#e3e3e3" stroke-width="0.4"/>' % (X0, y + 2.2, X1,
                                                                                                     y + 2.2))
        for a, b in c["ranges"]:
            S.append('<rect x="%.1f" y="%.1f" width="%.1f" height="4.4" fill="%s" stroke="none"/>' %
                     (X0 + a * sc, y, max(1.2, (b - a) * sc), col[c["key"]]))
    yb = Y + len(C) * LH + 1
    S.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke="#555555" stroke-width="0.5"/>' % (X0, yb, X1, yb))
    step = next(s for s in (1, 2, 5, 10, 15, 20, 30, 60, 120) if minutes / s <= 10)
    for m in range(0, minutes + 1, step):
        x = X0 + m * 60 * sc
        S.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#555555" stroke-width="0.5"/>' %
                 (x, yb, x, yb + 3))
        tx(x - 4, yb + 4, L["min"].format(m=m), "#555555", 4.4)

    # Cluster map: bubbles in two staggered rows, in time order
    main = [c for c in C if not c.get("aside")]
    ty = yb + 16
    tx(24, ty, L["map"], "#555555", 5.5)
    n = max(1, len(main))
    gap = (W_PAGE - 190) / max(1, n - 1) if n > 1 else 0
    rows = (ty + 52, ty + 112)
    first = sorted(main, key=lambda c: min(a for a, _ in c["ranges"]))
    pos = {c["key"]: (95 + i * gap, rows[i % 2]) for i, c in enumerate(first)}
    R = {k: 7 + math.sqrt(max(pct[k], 1)) * 4.2 for k in pos}
    links = data.get("links") or [[first[i]["key"], first[i + 1]["key"]] for i in range(len(first) - 1)]
    for a, b in links:
        if a not in pos or b not in pos:
            continue
        (x1, y1), (x2, y2) = pos[a], pos[b]
        d = math.hypot(x2 - x1, y2 - y1)
        sx, sy, ex, ey = x1, y1 - R[a], x2, y2 - R[b]
        mx, my = (sx + ex) / 2, min(sy, ey) - min(40, 0.3 * d)
        S.append('<path d="M%.1f %.1f Q%.1f %.1f %.1f %.1f" fill="none" stroke="#9a9a9a" stroke-width="0.5" '
                 'stroke-dasharray="2,1.5"/>' % (sx, sy, mx, my, ex, ey))
    for c in first:
        k = c["key"]
        x, y = pos[k]
        r = R[k]
        S.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s" fill-opacity="0.22" stroke="%s" stroke-width="1"/>' %
                 (x, y, r, col[k], col[k]))
        tx(x - 9, y - 4, k, col[k], 6)
        tx(x - 9, y + 2, "%d%%" % pct[k], "#222222", 5)
        room = 2 * gap - r - 14 if gap else 300  # (up to the next bubble in the same row)
        tx(x + r + 3, y - 11, c["name"], col[k], fit(c["name"], 5, room))
        for j, w in enumerate(c.get("keywords", [])[:3]):
            ww = text_width(w, 4.2) + 5
            yy = y - 3 + j * 7
            S.append('<rect x="%.1f" y="%.1f" width="%.1f" height="6" rx="2" fill="#ffffff" stroke="%s" '
                     'stroke-width="0.5"/>' % (x + r + 3, yy, ww, col[k]))
            tx(x + r + 5.5, yy + 0.6, w, "#333333", 4.2)

    # Cards: five per row, the second row centred
    cy0 = rows[1] + 40
    cw, gx = 160, 21
    per = 5
    nrows = max(1, math.ceil(len(main) / per))
    chh = min(84, (H_PAGE - 24 - cy0 - 4 * nrows) / nrows)
    tx(24, cy0 - 10, L["cards"], "#555555", 5.5)
    for i, c in enumerate(main):
        k = c["key"]
        row, colno = divmod(i, per)
        in_row = min(per, len(main) - row * per)
        x = gx + colno * cw + (per - in_row) * cw / 2
        y = cy0 + row * (chh + 4)
        head = "%s %s · %d%%" % (k, c["name"], pct[k])
        S.append('<rect x="%.1f" y="%.1f" width="%d" height="%.1f" rx="3" fill="none" stroke="%s" stroke-width="0.8"/>'
                 % (x + 2, y, cw - 6, chh, col[k]))
        S.append('<rect x="%.1f" y="%.1f" width="%d" height="9" rx="3" fill="%s" stroke="none"/>' % (x + 2, y, cw - 6,
                                                                                                     col[k]))
        tx(x + 5, y + 1, head, "#ffffff", fit(head, 5, cw - 14))
        acts = [a for a in c.get("actions", []) if a.startswith("✓")]
        for fs in (4.3, 4.1, 3.9, 3.7, 3.5):  # the largest text that fits the card
            lh = fs * 1.25
            lines = []
            for p in c.get("points", [])[:4]:
                lines += [(("• " if n == 0 else "   ") + ln, "#222222") for n, ln in enumerate(wrap(p, fs, cw - 20))]
            for a in acts:
                lines += [(("" if n == 0 else "   ") + ln, "#1e8449") for n, ln in enumerate(wrap(a, fs, cw - 20))]
            if 12 + len(lines) * lh <= chh - 2:
                break
        yy = y + 12
        for s, cc in lines:
            if yy + lh > y + chh:
                break
            tx(x + 5, yy, s, cc, fs)
            yy += lh
    if data.get("source"):
        tx(24, H_PAGE - 11, L["source"].format(s=data["source"]), "#777777", 4.5)
    return S, T


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = json.load(open(sys.argv[1], encoding="utf-8"))
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    S, T = build(data)
    title = data.get("title", "")
    handwritten = False
    if title and title.isascii() and os.path.isdir(CONSPECT):  # the owner's handwriting has Latin letters only
        try:
            sys.path.insert(0, CONSPECT)
            from conspect import Sheet
            sh = Sheet()
            sh.text(24, 26, title, size=6.5, color="#141414")
            sh.save(os.path.join(out, "title"), preview=True, dpi=80)
            handwritten = True
        except Exception as e:  # (no numpy/scipy, …): typed instead
            print("title typed (conspect unavailable: %s)" % e, file=sys.stderr)
    if title and not handwritten:
        T.insert(0, {"x": 24, "y": 14, "text": title, "color": "#141414", "font": {"name": "Sans", "size": 13}})
    with open(os.path.join(out, "body.svg"), "w", encoding="utf-8") as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">%s</svg>' %
                (W_PAGE, H_PAGE, W_PAGE, H_PAGE, "".join(S)))
    with open(os.path.join(out, "texts.json"), "w", encoding="utf-8") as f:
        json.dump(T, f, ensure_ascii=False)
    print("%s: body.svg (%d shapes), texts.json (%d labels)%s" % (out, len(S), len(T),
                                                                  ", title.svg" if handwritten else ""))


if __name__ == "__main__":
    main()
