# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Render a Click board's top side from MikroElektronika's "2D and 3D files"
DXF (outline GKO, silkscreen GTO, mask openings GTS) to an SVG, plus simple
part bodies listed in a parts file, then rasterize it with resvg.

    py tools/pp_web/clicks/render_click.py <click.dxf> <parts.json> <out.svg>
    then rasterize out.svg at 20 px/mm with a transparent background
    (resvg-js: new Resvg(svg, {fitTo: {mode: "width", value: px}}))

The SVG is in millimetres, DXF coordinates with y flipped; its viewBox is
the board outline, and the outline's lower-left corner is printed so that
board.json-style positions (DXF mm) can be placed on the image.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

import ezdxf

MASK = "#2b8a3e"
MASK_EDGE = "#1d6a2d"
SILK = "#f4f4f0"
GOLD = "#d8b45a"
TIN = "#c9ccd0"


def layer_of(name: str) -> str:
    return name.rsplit("_", 1)[-1]          # ..._GKO -> GKO


def chain(segments: list[list[tuple[float, float]]]) -> list[tuple[float, float]]:
    """Join open polylines end to end into one closed outline."""
    segs = [list(s) for s in segments]
    path = segs.pop(0)
    close = lambda a, b: math.dist(a, b) < 0.2
    while segs:
        for i, s in enumerate(segs):
            if close(path[-1], s[0]):
                path += s[1:]
            elif close(path[-1], s[-1]):
                path += s[-2::-1]
            else:
                continue
            segs.pop(i)
            break
        else:
            raise ValueError("outline does not close")
    return path


def main(dxf: str, parts_file: str, out: str) -> None:
    doc = ezdxf.readfile(dxf)
    msp = doc.modelspace()
    parts = json.loads(Path(parts_file).read_text(encoding="utf-8"))
    outline, silk, pads = [], [], []
    for e in msp:
        layer = layer_of(e.dxf.layer)
        if layer == "GKO" and e.dxftype() == "POLYLINE":
            outline.append([(p.x, p.y) for p in e.points()])
        elif layer == "GTO":
            silk.append(e)
        elif layer == "GTS" and e.dxftype() == "INSERT":
            pads.append(e)
    edge = chain(outline)               # the outline line's centre
    x0, y0 = min(p[0] for p in edge), min(p[1] for p in edge)
    x1, y1 = max(p[0] for p in edge), max(p[1] for p in edge)
    w, h = x1 - x0, y1 - y0
    fy = lambda y: y1 - y + y0              # flip, keeping DXF y range
    pt = lambda x, y: f"{x:.4f},{fy(y):.4f}"

    def solid(s) -> str:
        v = [s.dxf.vtx0, s.dxf.vtx1, s.dxf.vtx3, s.dxf.vtx2]
        return "M" + " L".join(pt(p.x, p.y) for p in v) + "Z"

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{x0:.4f} {y0:.4f} '
           f'{w:.4f} {h:.4f}" width="{w:.3f}mm" height="{h:.3f}mm">',
           '<defs><linearGradient id="sheen" x1="0" y1="0" x2="1" y2="1">'
           '<stop offset="0" stop-color="#fff" stop-opacity=".10"/>'
           '<stop offset="1" stop-color="#000" stop-opacity=".12"/>'
           '</linearGradient></defs>']
    board = "M" + " L".join(pt(*p) for p in edge) + "Z"
    svg.append(f'<path d="{board}" fill="{MASK}" stroke="{MASK_EDGE}" '
               'stroke-width="0.3"/>')
    svg.append(f'<path d="{board}" fill="url(#sheen)"/>')
    # Pads: gold, the through-hole header pins with a tinned joint
    for ins in pads:
        name = ins.dxf.name
        geo = "".join(solid(s) for s in ins.virtual_entities()
                      if s.dxftype() == "SOLID")
        svg.append(f'<path d="{geo}" fill="{GOLD}"/>')
        if name.startswith(("ROUND", "SQUARE")):
            c = ins.dxf.insert
            svg.append(f'<circle cx="{c.x:.4f}" cy="{fy(c.y):.4f}" r="0.62" '
                       f'fill="{TIN}" stroke="#8d9299" stroke-width="0.08"/>')
    # Silkscreen: stroked polylines and filled solids (the text)
    paths = []
    for e in silk:
        if e.dxftype() == "SOLID":
            paths.append(f'<path d="{solid(e)}" fill="{SILK}"/>')
        elif e.dxftype() == "POLYLINE":
            pts = [(p.x, p.y) for p in e.points()]
            width = e.dxf.get("default_start_width", 0.15) or 0.15
            paths.append(f'<polyline points="{" ".join(pt(*p) for p in pts)}" '
                         f'fill="none" stroke="{SILK}" stroke-width="{width}"/>')
    svg += paths
    # Part bodies from the parts file (DXF mm, centre and size)
    for p in parts["parts"]:
        cx, cy = p["at_mm"]
        pw, ph = p["size_mm"]
        x, y = cx - pw / 2, fy(cy) - ph / 2
        kind = p["kind"]
        if kind == "ic":
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.12" fill="#1c1c1e" stroke="#000" stroke-width="0.05"/>')
            svg.append(f'<circle cx="{x + 0.35:.3f}" cy="{y + 0.35:.3f}" r="0.14" '
                       'fill="#3a3a3e"/>')
        elif kind in ("resistor", "zero_ohm", "capacitor", "led"):
            body = {"resistor": "#1b1b1b", "zero_ohm": "#1b1b1b",
                    "capacitor": "#b49a72", "led": "#f2f6f2"}[kind]
            vertical = ph > pw
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       f'fill="{TIN}"/>')
            if vertical:
                svg.append(f'<rect x="{x:.3f}" y="{y + ph * 0.22:.3f}" width="{pw}" '
                           f'height="{ph * 0.56:.3f}" fill="{body}"/>')
            else:
                svg.append(f'<rect x="{x + pw * 0.22:.3f}" y="{y:.3f}" '
                           f'width="{pw * 0.56:.3f}" height="{ph}" fill="{body}"/>')
            if kind == "led":
                svg.append(f'<rect x="{x + pw * 0.3:.3f}" y="{y + ph * 0.2:.3f}" '
                           f'width="{pw * 0.4:.3f}" height="{ph * 0.6:.3f}" '
                           'fill="#9fe0a8" opacity=".8"/>')
    svg.append("</svg>")
    Path(out).write_text("\n".join(svg), encoding="utf-8")
    print(json.dumps({"origin_mm": [round(x0, 4), round(y1, 4)],
                      "size_mm": [round(w, 4), round(h, 4)],
                      "px": [round(w * 20), round(h * 20)]}))


if __name__ == "__main__":
    main(*sys.argv[1:4])
