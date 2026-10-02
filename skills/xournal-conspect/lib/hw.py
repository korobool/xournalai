#!/usr/bin/env python3
"""Quick handwriting snippet for live editing: text in the owner's v3 font -> SVG at page coordinates.
usage: hw.py OUT.svg X BASELINE SIZE "text" [--color #hex] [--width 0.8] [--underline] [--line L]
SIZE = x-height in pt (measure from their strokes: lowercase letter height). '\\n' in text = new line.
Draw with xournalai create_from_svg(path=OUT.svg, x=0, y=0, scale=1, fit="none", layer="current", profile="ink")."""
import sys, os, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conspect import Sheet
ap = argparse.ArgumentParser(); ap.add_argument("out"); ap.add_argument("x", type=float); ap.add_argument("y", type=float)
ap.add_argument("size", type=float); ap.add_argument("text"); ap.add_argument("--color", default="#008000")
ap.add_argument("--width", type=float, default=0.8); ap.add_argument("--underline", action="store_true")
ap.add_argument("--line", type=float, default=None)
a = ap.parse_args()
s = Sheet(842, 1191, ink=a.color)
t = a.text.replace("\\n", "\n")
b = s.heading(a.x, a.y, t, size=a.size, width=a.width, color=a.color) if a.underline else \
    s.text(a.x, a.y, t, size=a.size, width=a.width, color=a.color, line=a.line)
open(a.out, "w").write(s.svg().replace('viewBox="0 0 842 1191" width="842" height="1191"', 'viewBox="0 0 842 1191" width="842" height="1191"'))
print("bbox", [round(v, 1) for v in b])
