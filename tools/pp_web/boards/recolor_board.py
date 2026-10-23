# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Recolour a tracespace board render (the "<name>.top.svg" from
tracespace 4.2.1), crop it to the board outline and optionally draw part
bodies on it.

    py tools/pp_web/boards/recolor_board.py top.svg out.svg \\
        --mask "#a8101a" --edge 458,600,58980,99026

--edge is the board outline in the SVG's units (Gerber micrometres or
mils: x, y of the lower-left corner, width, height); without an outline
layer, take it from the silkscreen frame or the user guide's board size.
--flip is the Y of tracespace's "translate(0,Y) scale(1,-1)", read from the
SVG when omitted; --unit says which units the Gerbers used. --parts adds part bodies from a JSON list in board mm (y up,
the pick-and-place coordinates). Rasterize out.svg at 20 px/mm with
resvg-js. The EV15N46A uses the defaults: red solder mask, gold pads.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

PALETTES = {
    "red": {"fr4": "#4a0a0c", "cu": "#e8a0a0"},
    "blue": {"fr4": "#0b1d3a", "cu": "#8fb4ec"},
}
UNITS = {"mm": 1000.0, "in": 1000.0 / 25.4}     # SVG units per mm


def part_svg(p: dict) -> str:
    """One part body in board mm (y up), drawn about its centre."""
    (x, y), (w, h) = p["at_mm"], p["size_mm"]
    kind = p["kind"]
    out = [f'<g transform="translate({x} {y}) rotate({p.get("rotation", 0)})">']
    box = lambda dx, dy, bw, bh, fill, extra="": out.append(
        f'<rect x="{dx - bw / 2:.3f}" y="{dy - bh / 2:.3f}" width="{bw:.3f}" '
        f'height="{bh:.3f}" fill="{fill}" {extra}/>')
    if kind == "ic":
        box(0, 0, w, h, "#1c1c1e", 'rx="0.12"')
        out.append(f'<circle cx="{-w / 2 + 0.35:.3f}" cy="{h / 2 - 0.35:.3f}" '
                   'r="0.14" fill="#3a3a3e"/>')
    elif kind in ("resistor", "capacitor"):
        box(0, 0, w, h, "#c9ccd0")
        box(0, 0, w * 0.56, h, "#1b1b1b" if kind == "resistor" else "#b49a72")
    elif kind in ("header_female", "header_male", "socket"):
        # A row-major grid of pins (pitch 2.54) in a black body
        cols, rows = p["pins"]
        box(0, 0, w, h, "#18181a", 'rx="0.3"')
        for c in range(cols):
            for r in range(rows):
                px = (c - (cols - 1) / 2) * 2.54 + p.get("pin_dx", 0)
                py = (r - (rows - 1) / 2) * 2.54
                if kind == "header_male":
                    box(px, py, 0.64, 0.64, "#d9b36a")
                else:
                    box(px, py, 1.0, 1.0, "#050505", 'rx="0.15"')
        for x0, y0, x1, y1 in p.get("legs", []):
            out.append(f'<line x1="{x0}" y1="{y0}" x2="{x1}" y2="{y1}" '
                       'stroke="#c9ccd0" stroke-width="0.5"/>')
    elif kind == "jumper":
        box(0, 0, w, h, "#18181a", 'rx="0.4"')
        box(0, 0, w * 0.3, h * 0.55, "#e2b93b", 'rx="0.2"')
    out.append("</g>")
    return "".join(out)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--mask", default="#a8101a", help="solder mask colour")
    ap.add_argument("--palette", choices=PALETTES, default="red",
                    help="laminate and copper colours under the mask")
    ap.add_argument("--opacity", type=float, default=0.82,
                    help="mask opacity: copper shows through as traces")
    ap.add_argument("--edge", default="458,600,58980,99026")
    ap.add_argument("--flip", type=float, help="default: from the SVG")
    ap.add_argument("--unit", choices=UNITS, default="mm")
    ap.add_argument("--parts", help="JSON file: {\"parts\": [...]}")
    ap.add_argument("--any-outline", action="store_true",
                    help="clip to the outline layer however many contours it has")
    args = ap.parse_args()

    s = Path(args.src).read_text(encoding="utf-8")
    # tracespace clips the board to the layer it took for the outline. A
    # board edge is one contour (a few with cut-outs); a drawing on a
    # mechanical layer has hundreds, and clipping to it cuts wedges out of
    # the board (the EV15N46A R3's GM12, a test-point drawing)
    m = re.search(r'<clipPath id="\w+_top_outline">(.*?)</clipPath>', s, re.S)
    if m and not args.any_outline:
        contours = len(re.findall(r"[Mm]\s", m.group(1))) + len(
            re.findall(r"<(?:circle|rect)\b", m.group(1)))
        assert contours <= 16, (
            f"the outline layer has {contours} contours: a drawing, not the "
            "board edge? Render without it and give --edge, or pass --any-outline")
    colors = {**PALETTES[args.palette], "cf": "#d9b36a",
              "sm": f"{args.mask}; opacity: {args.opacity}", "ss": "#ffffff",
              "sp": "#bbbbbb", "out": "#000000"}
    for layer, color in colors.items():
        s, n = re.subn(r"(\.\w+_%s \{color: )[^}]*\}" % layer,
                       r"\g<1>%s;}" % color, s, count=1)
        assert n == 1, f"no .{layer} style in {args.src}"
    if args.flip is None:
        m = re.search(r"translate\(0,([-\d.]+)\) scale\(1,-1\)", s)
        assert m, "no translate(0,Y) scale(1,-1): pass --flip"
        args.flip = float(m.group(1))
    k = UNITS[args.unit]
    x0, y0, w, h = (float(v) for v in args.edge.split(","))
    top = args.flip - (y0 + h)          # SVG y grows downwards
    fmt = lambda v: f"{v:g}"
    s, n = re.subn(r'viewBox="[^"]*" width="[^"]*" height="[^"]*"',
                   f'viewBox="{fmt(x0)} {fmt(top)} {fmt(w)} {fmt(h)}" '
                   f'width="{w / k:g}mm" height="{h / k:g}mm"', s, count=1)
    assert n == 1, "no viewBox"
    if args.parts:
        parts = json.loads(Path(args.parts).read_text(encoding="utf-8"))
        group = (f'<g transform="matrix({k:g} 0 0 {-k:g} 0 {fmt(args.flip)})">'
                 + "".join(part_svg(p) for p in parts["parts"]) + "</g>")
        s = s[:s.rindex("</svg>")] + group + "</svg>"
    Path(args.out).write_text(s, encoding="utf-8")


if __name__ == "__main__":
    main()
