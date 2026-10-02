"""Conspect renderer: the owner's handwriting + hand-drawn primitives -> SVG (+ LaTeX manifest, + PNG preview).

Coordinates are page points (1/72 in), origin top-left, y down — the same as xournalai.
Typical use (see ../examples/logistic_regression.py):

    from conspect import Sheet
    s = Sheet(842, 595)                         # A4 landscape
    s.heading(20, 24, "Logistic Regression", size=6)
    s.code(24, 60, "def f(x):\n    return x", size=4.2)
    s.cloud(20, 80, 150, 40); s.latex(26, 86, r"J=\\frac1m\\sum_i L", height=18)
    s.save("out")          # out.svg (strokes), out.latex.json (formula placements), out.png (preview)

Then in xournalai: create_from_svg(path=out.svg, x=0, y=0, fit="none", layer="current", profile="ink")
and one create_latex call per manifest entry (x, y, latex, height, color, layer="current").
"""
import json, math, os, random
import numpy as np
from scipy.ndimage import gaussian_filter1d

HERE = os.path.dirname(os.path.abspath(__file__))
import sys; sys.path.insert(0, HERE)
import glyphs_synth as GS
import glyphstore

# ---------------------------------------------------------------- font
SMALL = set("acemnorsuvwxz"); ASC = set("bdfhklt"); DESC = set("gpqy")
BAD_SRC = {"How", "skills,", "accent,"}
USER_SKIP = set("gp")
NARROW = 0.9                                              # v3 horizontal factor                                      # captures unusable ("ng", n-like p)                     # mis-segmented captures
BAD_IDX = {("a", 7), ("e", 0), ("n", 3), ("n", 4), ("n", 6), ("n", 11), ("n", 14), ("i", 5), ("i", 7), ("c", 3), ("o", 1)}

def _resample(p, step=0.03):
    p = np.asarray(p, float)
    if len(p) < 2: return p
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
    if d[-1] < step: return p
    t = np.r_[np.arange(0, d[-1], step), d[-1]]
    return np.c_[np.interp(t, d, p[:, 0]), np.interp(t, d, p[:, 1])]

def _neaten(ch, strokes, target=None):
    S = [np.asarray(s, float) for s in strokes]
    allp = np.vstack(S); top, bot = allp[:, 1].min(), allp[:, 1].max()
    if ch in SMALL: f = 1.0 / max(-top, 0.3)
    elif ch in ASC: f = 1.65 / max(-top, 0.5)
    elif ch in DESC: f = 1.55 / max(bot - top, 0.5)
    else: f = 1.0
    f = float(np.clip(f, 0.7, 1.4))
    out = []
    for s in S:
        r = _resample(s * f)
        if len(r) > 6:
            r[:, 0] = gaussian_filter1d(r[:, 0], 1.8, mode="nearest")
            r[:, 1] = gaussian_filter1d(r[:, 1], 1.8, mode="nearest")
        out.append(r)
    x0 = np.vstack(out)[:, 0].min()
    out = [s - [x0, 0] for s in out]
    return {"strokes": [s.tolist() for s in out], "w": float(np.vstack(out)[:, 0].max())}

class Font:
    """Owner's captured lowercase glyphs (neatened, 2 most regular variants each) + synthetic fallback for the rest."""
    def __init__(self, user_json=None, use_user=True, style="v3"):
        self.g = {}
        user_json = user_json or glyphstore.user_glyphs()
        if style == "v3" and not user_json:
            style = "v2"                        # no captured handwriting here: the designed font
        self.style = style
        self.gap, self.space, self.noise = (0.24, 0.6, 0.012) if style == "v2" else (0.3, 0.62, 0.025)
        if style == "v2":                       # redesigned font modelled on the owner's paper handwriting
            import glyphs_v2
            self.g = glyphs_v2.build()
            return
        if style == "v3":                       # DEFAULT: the owner's own captured glyphs, cleaned (not redesigned)
            import glyphs_v2, glyphs_v3
            own, self.slant = glyphs_v3.build(user_json)
            self.g = glyphs_v2.build()          # fallback only for characters never captured
            self.g.update(own)
            for ch, vs in self.g.items():       # a bit narrower overall (owner's request)
                for v in vs:
                    v["strokes"] = [[[p[0] * NARROW, p[1]] for p in st] for st in v["strokes"]]; v["w"] *= NARROW
            self.gap, self.space, self.noise = 0.3, 0.62, 0.006
            self.own = set(own)
            return
        for ch, strokes in GS.G.items():
            st = [[list(p) for p in s] for s in strokes]
            self.g[ch] = [{"strokes": st, "w": GS.width(strokes)}]
        if use_user and user_json and os.path.exists(user_json):
            raw = json.load(open(user_json))
            for ch, vs in raw.items():
                if not (ch.islower() and ch.isalpha()) or ch in USER_SKIP: continue  # caps/digits/symbols: synthetic set
                keep = [_neaten(ch, v["strokes"]) for i, v in enumerate(vs)
                        if v["src"] not in BAD_SRC and (ch, i) not in BAD_IDX and not (v["src"] == "pronounciation" and ch != "p")]
                if keep:
                    med = np.median([k["w"] for k in keep])
                    self.g[ch] = sorted(keep, key=lambda k: abs(k["w"] - med))[:2]
    def glyph(self, ch, rnd):
        vs = self.g.get(ch) or self.g.get(ch.lower()) or self.g["?"]
        return rnd.choice(vs)


# ---------------------------------------------------------------- real LaTeX (for measuring + preview)
CACHE = os.path.expanduser("~/.cache/xournal-conspect")
def tex_png(src):
    """Compile a formula in display math -> (cropped png path, aspect w/h) or None. Cached in ~/.cache/xournal-conspect.
    Uses the plain article class (standalone.cls is not installed here) and trims whitespace with PIL."""
    import hashlib, subprocess, shutil
    from PIL import Image, ImageOps
    os.makedirs(CACHE, exist_ok=True)
    h = hashlib.sha1(src.encode()).hexdigest()[:16]; png = os.path.join(CACHE, h + ".png")
    if not os.path.exists(png):
        if not shutil.which("pdflatex") or not shutil.which("pdftoppm"): return None
        d = os.path.join(CACHE, h); os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "f.tex"), "w").write(
            "\\documentclass{article}\\usepackage{amsmath,amssymb}\\pagestyle{empty}\n"
            "\\begin{document}\n\\[ " + src + " \\]\n\\end{document}\n")
        r = subprocess.run(["pdflatex", "-interaction=nonstopmode", "f.tex"], cwd=d, capture_output=True, timeout=60)
        if r.returncode != 0 or not os.path.exists(os.path.join(d, "f.pdf")):
            shutil.rmtree(d, ignore_errors=True); return None
        subprocess.run(["pdftoppm", "-png", "-r", "600", "-singlefile", "f.pdf", "full"], cwd=d, capture_output=True)
        im = Image.open(os.path.join(d, "full.png")).convert("L")
        box = ImageOps.invert(im).getbbox()
        if not box: shutil.rmtree(d, ignore_errors=True); return None
        pad = 6; box = (max(0, box[0] - pad), max(0, box[1] - pad), box[2] + pad, box[3] + pad)
        im.crop(box).save(png)
        shutil.rmtree(d, ignore_errors=True)
    w, hh = Image.open(png).size
    return png, w / hh

# ---------------------------------------------------------------- sheet
class Sheet:
    def __init__(self, width=842, height=595, ink="#141414", seed=7, font=None, jitter=0.35):
        self.W, self.H, self.ink = width, height, ink
        self.rnd = random.Random(seed); self.np = np.random.default_rng(seed)
        self.font = font or Font()
        self.paths = []      # (points ndarray, color, width, fill)
        self.latexes = []    # dicts for create_latex
        self.jit = jitter

    # -- low level
    def _wobble(self, pts, amp=None):
        pts = np.asarray(pts, float)
        amp = self.jit if amp is None else amp
        if amp <= 0 or len(pts) < 3: return pts
        n = np.column_stack([gaussian_filter1d(self.np.normal(0, 1, len(pts)), 6, mode="nearest") for _ in range(2)])
        n *= amp / (np.abs(n).max() + 1e-9)
        return pts + n
    def _dense(self, pts, step=1.2):
        pts = np.asarray(pts, float); out = [pts[0]]
        for a, b in zip(pts[:-1], pts[1:]):
            k = max(1, int(np.hypot(*(b - a)) / step))
            out += [a + (b - a) * t / k for t in range(1, k + 1)]
        return np.array(out)
    def stroke(self, pts, color=None, width=0.8, wobble=True, fill=None):
        p = self._dense(pts)
        if wobble: p = self._wobble(p)
        self.paths.append((p, color or self.ink, width, fill))
        return p

    # -- text
    def measure(self, s, size):
        w = 0.0
        for ch in s:
            if ch == " ": w += self.font.space * size; continue
            vs = self.font.g.get(ch) or self.font.g.get(ch.lower()) or self.font.g["?"]
            w += (np.mean([v["w"] for v in vs]) + self.font.gap) * size
        return w
    def text(self, x, y, s, size=4.5, color=None, width=0.7, line=None, angle=0.0):
        """Handwritten text. y = baseline of the first line; '\\n' = new line. Returns bbox (x0, y0, x1, y1)."""
        line = line or 3.0 * size
        ca, sa = math.cos(math.radians(angle)), math.sin(math.radians(angle))
        xmax = x
        for li, row in enumerate(s.split("\n")):
            cx, by = 0.0, li * line
            for ch in row:
                if ch == " ": cx += self.font.space * size; continue
                g = self.font.glyph(ch, self.rnd)
                gs = self.rnd.uniform(0.96, 1.04); gdy = self.rnd.uniform(-0.03, 0.03) * size
                for st in g["strokes"]:
                    p = np.asarray(st, float)
                    if len(p) >= 3:   # tiny organic wobble so synthetic glyphs don't look typeset
                        p = p + np.column_stack([gaussian_filter1d(self.np.normal(0, 1, len(p)), 2, mode="nearest") for _ in range(2)]) * self.font.noise
                    p = p * size * gs + [cx, by + gdy]
                    p = np.c_[p[:, 0] * ca - p[:, 1] * sa, p[:, 0] * sa + p[:, 1] * ca] + [x, y]
                    if len(p) < 8: p = self._dense(p, step=0.4)
                    self.paths.append((p, color or self.ink, width, None))
                cx += (g["w"] + self.font.gap) * size
            xmax = max(xmax, x + cx)
        return (x, y - 1.7 * size, xmax, y + (len(s.split("\n")) - 1) * line + 0.6 * size)
    def heading(self, x, y, s, size=6, color=None, underline=True, width=0.9):
        b = self.text(x, y, s, size=size, color=color, width=width)
        if underline: self.stroke([(x - 1, y + 0.9 * size), (b[2] + 1, y + 0.8 * size)], color, 0.8)
        return b
    def code(self, x, y, src, size=4.2, color=None, line=None, bold_first=False):
        """Code block, leading spaces preserved (indent unit = measured space)."""
        line = line or 3.1 * size
        rows = src.split("\n"); b = None
        for i, r in enumerate(rows):
            ind = len(r) - len(r.lstrip(" "))
            bb = self.text(x + ind * self.font.space * size, y + i * line, r.lstrip(" "), size=size, color=color, width=0.7 if i or not bold_first else 1.0)
            b = bb if b is None else (b[0], b[1], max(b[2], bb[2]), bb[3])
        return b

    # -- shapes
    def line(self, p0, p1, color=None, width=0.8): return self.stroke([p0, p1], color, width)
    def rect(self, x, y, w, h, color=None, width=0.8, fill=None):
        return self.stroke([(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y + 0.01)], color, width, fill=fill)
    def cloud(self, x, y, w, h, color=None, width=0.8, amp=1.6, period=9.0, radius=6):
        """Wavy rounded frame — the owner's 'formula cloud' box."""
        r = min(radius, w / 3, h / 3); pts = []
        for (cx, cy, a0) in [(x + w - r, y + r, -90), (x + w - r, y + h - r, 0), (x + r, y + h - r, 90), (x + r, y + r, 180)]:
            pts += [(cx + r * math.cos(math.radians(a0 + t)), cy + r * math.sin(math.radians(a0 + t))) for t in range(0, 91, 10)]
        pts.append(pts[0]); p = self._dense(pts, 0.6)
        d = np.r_[0, np.cumsum(np.hypot(*np.diff(p, axis=0).T))]
        tang = np.gradient(p, axis=0); nrm = np.c_[tang[:, 1], -tang[:, 0]]; nrm /= np.linalg.norm(nrm, axis=1)[:, None] + 1e-9
        ph = self.rnd.uniform(0, 6.28)
        p = p + nrm * (amp * np.sin(2 * np.pi * d / period + ph) * (0.6 + 0.4 * np.sin(d / 23.0)))[:, None]
        self.paths.append((self._wobble(p, 0.2), color or self.ink, width, None)); return p
    def brace(self, x, y0, y1, side="right", depth=5, color=None, width=0.8):
        """Vertical curly brace; side='right' means it opens to the left (points right)."""
        s = 1 if side == "right" else -1; ym = (y0 + y1) / 2; t = np.linspace(0, 1, 40)
        top = [(x + s * depth * 0.5 * (1 - np.cos(np.pi * v)) * 0.5, y0 + (ym - y0) * v) for v in t]
        top = [(x + s * (depth * 0.45) * math.sin(math.pi * v), y0 + (ym - y0) * v) for v in t]
        pts = [(x, y0)] + [(x + s * depth * 0.45, y0 + (ym - y0) * 0.15), (x + s * depth * 0.45, ym - (ym - y0) * 0.2), (x + s * depth, ym),
               (x + s * depth * 0.45, ym + (y1 - ym) * 0.2), (x + s * depth * 0.45, y1 - (y1 - ym) * 0.15), (x, y1)]
        from scipy.interpolate import make_interp_spline
        P = np.array(pts); tt = np.linspace(0, 1, len(P)); spl = make_interp_spline(tt, P, k=2)
        return self.stroke(spl(np.linspace(0, 1, 60)), color, width)
    def bracket(self, x, y0, y1, label=None, size=3.6, color=None, width=0.8):
        """Left square bracket with a vertical label (like 'FP[' / 'BP[' in the owner's notes)."""
        self.stroke([(x + 3, y0), (x, y0), (x, y1), (x + 3, y1)], color, width)
        if label: self.text(x - 2, (y0 + y1) / 2 + self.measure(label, size) / 2, label, size=size, color=color, angle=-90)
    def arrow(self, p0, p1, color=None, width=0.8, head=5, dot_start=False, dot_end=False, head_end=True):
        p0 = np.array(p0, float); p1 = np.array(p1, float)
        self.stroke([p0, p1], color, width)
        if head_end:
            d = (p1 - p0) / (np.linalg.norm(p1 - p0) + 1e-9); n = np.array([-d[1], d[0]])
            self.stroke([p1 - head * d + 0.45 * head * n, p1, p1 - head * d - 0.45 * head * n], color, width, wobble=False)
        if dot_start: self.dot(*p0, color=color)
        if dot_end: self.dot(*p1, color=color)
    def connector(self, pts, color=None, width=0.8, dot_end=True):
        """Polyline link (e.g. code box -> explanation box) ending with a dot anchor, like the owner's notes."""
        self.stroke(pts, color, width)
        if dot_end: self.dot(*pts[-1], color=color)
    def dot(self, x, y, r=1.3, color=None):
        a = np.linspace(0, 2 * np.pi, 14)
        self.paths.append((np.c_[x + r * np.cos(a), y + r * np.sin(a)], color or self.ink, 0.8, color or self.ink))
    def polyline(self, pts, color=None, width=0.8, wobble=False): return self.stroke(pts, color, width, wobble=wobble)
    def axes(self, x, y, w, h, color=None, width=0.8):
        """Plot axes with arrows; origin bottom-left at (x, y+h)."""
        self.arrow((x, y + h), (x + w, y + h), color, width, head=4)
        self.arrow((x, y + h), (x, y), color, width, head=4)
    def latex(self, x, y, src, height, color=None, maxw=None):
        """Formula placement for xournalai create_latex. maxw: shrink height so the width fits. Returns (x0,y0,x1,y1)."""
        r = tex_png(src); aspect = r[1] if r else None
        if aspect and maxw and height * aspect > maxw:
            height = maxw / aspect
        self.latexes.append({"x": round(x, 1), "y": round(y, 1), "latex": src, "height": round(height, 1),
                             "color": color or self.ink, "width": round(height * aspect, 1) if aspect else None})
        return (x, y, x + (height * aspect if aspect else 8 * height), y + height)

    # -- output
    def save_xopp(self, path, bg="plain"):
        """Write a native Xournal++ document (gzipped XML): editable pen strokes + LaTeX as teximage (PNG).
        Use when the xournalai MCP is unavailable; open it in Xournal++ (File > Open)."""
        import gzip, base64, html
        from PIL import Image
        def col(c):
            c = c.lstrip("#"); return "#" + (c + "ff" if len(c) == 6 else c)
        out = ['<?xml version="1.0" standalone="no"?>',
               '<xournal creator="xournalpp 1.3.0" fileversion="4">', '<title>Xournal++ document</title>',
               f'<page width="{self.W}" height="{self.H}">',
               f'<background type="solid" color="#ffffffff" style="{bg}"/>', '<layer>']
        for p, c, w, fill in self.paths:
            if len(p) < 2: continue
            pts = " ".join("%.2f %.2f" % (a, b) for a, b in p)
            f = ' fill="255"' if fill else ""
            out.append(f'<stroke tool="pen" color="{col(c)}" width="{w:.2f}"{f} capStyle="round">{pts}</stroke>')
        for L in self.latexes:
            r = tex_png(L["latex"])
            if not r: continue
            png, aspect = r
            w = L["height"] * aspect
            data = base64.b64encode(open(png, "rb").read()).decode()
            out.append(f'<teximage text="{html.escape(L["latex"], quote=True)}" left="{L["x"]:.2f}" top="{L["y"]:.2f}" '
                       f'right="{L["x"] + w:.2f}" bottom="{L["y"] + L["height"]:.2f}">{data}</teximage>')
        out += ['</layer>', '</page>', '</xournal>']
        with gzip.open(path, "wt", encoding="utf-8") as fh: fh.write("\n".join(out))
        return path

    def svg(self):
        out = []
        for p, col, w, fill in self.paths:
            d = "M" + " L".join("%.2f,%.2f" % (a, b) for a, b in p)
            f = f'fill="{fill}"' if fill else 'fill="none"'
            out.append(f'<path d="{d}" {f} stroke="{col}" stroke-width="{w:.2f}" stroke-linecap="round" stroke-linejoin="round"/>')
        return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.W} {self.H}" width="{self.W}" height="{self.H}">' + "".join(out) + "</svg>"
    def save(self, base, preview=True, dpi=150):
        open(base + ".svg", "w").write(self.svg())
        json.dump(self.latexes, open(base + ".latex.json", "w"), indent=1)
        if preview: self.preview(base + ".png", dpi)
        return base + ".svg", base + ".latex.json"
    def preview(self, png, dpi=150):
        import matplotlib; matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig = plt.figure(figsize=(self.W / 72, self.H / 72), dpi=dpi); ax = fig.add_axes([0, 0, 1, 1])
        ax.set_xlim(0, self.W); ax.set_ylim(self.H, 0); ax.axis("off"); ax.set_aspect("auto")
        for p, col, w, fill in self.paths:
            if fill: ax.fill(p[:, 0], p[:, 1], color=fill, lw=0)
            ax.plot(p[:, 0], p[:, 1], color=col, lw=w * 72 / 72 * 0.9, solid_capstyle="round")
        from matplotlib.mathtext import MathTextParser
        parser = MathTextParser("path")
        for L in self.latexes:
            src = L["latex"]
            for a, b in [(r"\big", ""), (r"\Big", ""), (r"\!", ""), (r"\textstyle", ""), (r"\left", ""), (r"\right", ""),
                         (r"\mathbb{E}", "E"), (r"\qquad", r"\quad"), (r"\top", "T"), (r"\dots", r"\ldots")]:
                src = src.replace(a, b)
            fs = L["height"] * 0.55
            r = tex_png(L["latex"])
            if r:
                img = plt.imread(r[0]); w = L["height"] * r[1]
                if img.ndim == 2: img = np.dstack([img] * 3)
                elif img.shape[-1] == 4: img = img[..., :3] * img[..., 3:] + (1 - img[..., 3:])
                ax.imshow(img, extent=(L["x"], L["x"] + w, L["y"] + L["height"], L["y"]), interpolation="lanczos", zorder=3)
                continue
            if r"\begin{cases}" in src:   # preview approximation: one line per case
                rows = src.split(r"\begin{cases}")[1].split(r"\end{cases}")[0].split(r"\\")
                for k, r in enumerate(rows):
                    r = r.replace("&", r"\quad ")
                    try: parser.parse(f"${r}$"); ax.text(L["x"] + 4, L["y"] + k * L["height"] / len(rows), f"${r}$", fontsize=fs / 1.6, va="top", color=L["color"])
                    except Exception: pass
                ax.text(L["x"], L["y"] - 1, "{", fontsize=fs * 1.1, va="top", color=L["color"])
                continue
            try:
                parser.parse(f"${src}$")
                ax.text(L["x"], L["y"], f"${src}$", fontsize=fs, va="top", ha="left", color=L["color"])
            except Exception:
                ax.add_patch(plt.Rectangle((L["x"], L["y"]), 60, L["height"], fill=False, ec="red", lw=0.4))
                ax.text(L["x"] + 1, L["y"] + 1, L["latex"][:60], fontsize=2.5, va="top", color="red")
        fig.savefig(png, dpi=dpi)
        plt.close(fig)
