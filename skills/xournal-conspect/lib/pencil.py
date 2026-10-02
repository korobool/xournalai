"""Pencil sketch of a reference image as native strokes (the technique behind the Einstein portrait, 2026-09-29).

A photo (found on the web, generated, or the user's) becomes an SVG of short, direction-following pencil strokes
with varying width; import it with create_from_svg so every line is an editable stroke with pressure.

    python3 pencil.py photo.jpg out.svg --width 84 [--crop X0 Y0 X1 Y1] [--step 0.5] [--png preview.png]

Pipeline: grey → "dodge" pencil tone (gs / (1 - blur(1 - gs))) → edges and dark areas get strokes; the stroke
direction follows the image structure (structure tensor, perpendicular to the gradient); smooth background is
dropped. `step` is the stroke spacing in output points (smaller = denser, darker, more strokes).
"""

import argparse
import math
import random

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, map_coordinates, sobel


def pencil_sketch(image_path, out_svg, width_pt=84.0, crop=None, step=0.5, seed=3, color="#1c1c1c",
                  fade_bottom=True, preview_png=None):
    random.seed(seed)
    np.random.seed(seed)
    im = np.asarray(Image.open(image_path).convert("L"), float) / 255
    if crop:
        x0, y0, x1, y1 = crop
        im = im[y0:y1, x0:x1]
    H, W = im.shape
    OW = float(width_pt)
    OH = OW * H / W
    k = W / OW
    gs = gaussian_filter(im, 1.0)
    blur = gaussian_filter(1 - gs, 7)
    dodge = np.clip(gs / np.maximum(1 - blur, 1e-3), 0, 1)  # the pencil tone
    tone = np.clip(1.9 * (1 - dodge) ** 0.75 + 0.5 * np.clip(0.6 - gs, 0, 1), 0, 1)
    # drop smooth background (low local texture)
    gm = np.hypot(sobel(gs, 0), sobel(gs, 1))
    tone *= np.clip((gaussian_filter(gm, 9) - 0.035) / 0.04, 0, 1)
    if fade_bottom:  # portraits: lighter shoulders, soft edges
        yy, xx = np.mgrid[0:H, 0:W]
        tone *= np.where(yy > 0.74 * H, 0.42, np.where(yy > 0.62 * H, 0.8, 1.0))
        fy = np.clip((H - yy) / (0.2 * H), 0, 1)
        fx = np.clip(np.minimum(xx, W - xx) / (0.1 * W), 0, 1)
        tone *= (fy * fx) ** 1.5
    g2 = gaussian_filter(im, 2.5)
    gx, gy = sobel(g2, 1), sobel(g2, 0)
    jxx, jyy, jxy = gaussian_filter(gx * gx, 6), gaussian_filter(gy * gy, 6), gaussian_filter(gx * gy, 6)
    theta = 0.5 * np.arctan2(2 * jxy, jxx - jyy) + math.pi / 2  # along the structure

    def at(a, x, y):
        return map_coordinates(a, [[y * k], [x * k]], order=1, mode="nearest")[0]

    strokes = []
    for y in np.arange(0, OH, step):
        for x in np.arange(0, OW, step):
            x1, y1 = x + random.uniform(0, step), y + random.uniform(0, step)
            t = at(tone, x1, y1)
            if random.random() > t ** 1.25:
                continue
            length = random.uniform(1.2, 3.0) * (0.7 + 0.6 * t)
            pts = [(x1, y1)]
            px, py, sgn = x1, y1, random.choice([-1, 1])
            for _ in range(6):
                a = at(theta, px, py)
                px += sgn * math.cos(a) * length / 6
                py += sgn * math.sin(a) * length / 6
                if not (0 <= px < OW and 0 <= py < OH):
                    break
                pts.append((px, py))
            if len(pts) > 2:
                strokes.append((pts, 0.08 + 0.12 * t))
    paths = "".join(
        '<path d="M' + " L".join(f"{x:.2f},{y:.2f}" for x, y in p) +
        f'" fill="none" stroke="{color}" stroke-width="{w:.2f}" stroke-linecap="round"/>' for p, w in strokes)
    with open(out_svg, "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {OW:.1f} {OH:.1f}" '
                f'width="{OW:.1f}" height="{OH:.1f}">{paths}</svg>')
    if preview_png:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig = plt.figure(figsize=(OW / 12, OH / 12), dpi=100)
        ax = fig.add_axes([0, 0, 1, 1])
        ax.set_xlim(0, OW)
        ax.set_ylim(OH, 0)
        ax.axis("off")
        for p, w in strokes:
            a = np.array(p)
            ax.plot(a[:, 0], a[:, 1], color=color, lw=w * 1.4, solid_capstyle="round")
        fig.savefig(preview_png)
    return len(strokes)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image")
    ap.add_argument("out_svg")
    ap.add_argument("--width", type=float, default=84.0, help="output width in page points")
    ap.add_argument("--crop", type=int, nargs=4, metavar=("X0", "Y0", "X1", "Y1"))
    ap.add_argument("--step", type=float, default=0.5)
    ap.add_argument("--seed", type=int, default=3)
    ap.add_argument("--no-fade", action="store_true", help="don't fade the bottom and edges (not a portrait)")
    ap.add_argument("--png", help="also write a preview PNG")
    a = ap.parse_args()
    n = pencil_sketch(a.image, a.out_svg, a.width, a.crop, a.step, a.seed, fade_bottom=not a.no_fade,
                      preview_png=a.png)
    print(f"{n} strokes → {a.out_svg}")
