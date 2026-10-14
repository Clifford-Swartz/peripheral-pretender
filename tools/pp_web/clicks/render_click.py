# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Render a Click board's top side to an SVG.

    py tools/pp_web/clicks/render_click.py <click.dxf | -> <parts.json> <out.svg>
    then rasterize out.svg at 20 px/mm with a transparent background
    (resvg-js: new Resvg(svg, {fitTo: {mode: "width", value: px}}))

From MikroElektronika's "2D and 3D files" DXF: the outline (layer ..._GKO),
the silkscreen (..._GTO) and the mask openings (..._GTS); block references
are expanded, and drawings in inches (an outline under 5 units wide) are
scaled to mm. Clicks outlines are convex, so the outline is the convex hull
of the GKO points, which tolerates the duplicated segments some DXFs have.
Part bodies come from parts.json (positions in the DXF's coordinates, in
mm). With "-" instead of a DXF, parts.json must give "outline_mm" (the board
polygon) and may ask for the mikroBUS header pads ("header": true) and
silkscreen "texts".

The SVG is in millimetres with y flipped; its viewBox is the board outline.
The outline's top-left corner and size are printed for clicks.json.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

MASK = "#2b8a3e"
MASK_EDGE = "#1d6a2d"
SILK = "#f4f4f0"
GOLD = "#d8b45a"
TIN = "#c9ccd0"

# mikroBUS header pads (mm, relative to pin 1 of the left row): 2 x 8
HEADER_PADS = [(x, 2.54 * i) for x in (0.0, 22.86) for i in range(8)]


def layer_of(name: str) -> str:
    return name.rsplit("_", 1)[-1]          # ..._GKO -> GKO


def hull(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    """Convex hull, counter-clockwise (Andrew's monotone chain)."""
    pts = sorted(set((round(x, 5), round(y, 5)) for x, y in points))
    if len(pts) < 3:
        raise ValueError("outline has fewer than 3 points")
    cross = lambda o, a, b: (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def flat(entity):
    """An entity with its block references expanded."""
    if entity.dxftype() == "INSERT":
        for v in entity.virtual_entities():
            yield from flat(v)
    else:
        yield entity


def read_dxf(path: str):
    import ezdxf

    msp = ezdxf.readfile(path).modelspace()
    layers = {"GKO": [], "GTO": [], "GTS": []}
    for e in msp:
        layer = layer_of(e.dxf.layer)
        if layer in layers:
            layers[layer].append(e)
    outline = []
    for e in layers["GKO"]:
        for v in flat(e):
            if v.dxftype() == "POLYLINE":
                outline += [(p.x, p.y) for p in v.points()]
            elif v.dxftype() == "LINE":
                outline += [(v.dxf.start.x, v.dxf.start.y),
                            (v.dxf.end.x, v.dxf.end.y)]
    w = max(p[0] for p in outline) - min(p[0] for p in outline)
    return layers, outline, (25.4 if w < 5 else 1.0)


def main(dxf: str, parts_file: str, out: str) -> None:
    parts = json.loads(Path(parts_file).read_text(encoding="utf-8"))
    if dxf == "-":
        layers, k = {"GKO": [], "GTO": [], "GTS": []}, 1.0
        outline = [tuple(p) for p in parts["outline_mm"]]
    else:
        layers, outline, k = read_dxf(dxf)
    edge = hull(outline)
    x0, y0 = min(p[0] for p in edge), min(p[1] for p in edge)
    x1, y1 = max(p[0] for p in edge), max(p[1] for p in edge)
    # Everything below is drawn in mm: DXF units times k, y flipped
    X = lambda x: x * k
    Y = lambda y: (y1 + y0 - y) * k
    pt = lambda x, y: f"{X(x):.4f},{Y(y):.4f}"
    W, H = (x1 - x0) * k, (y1 - y0) * k
    vx, vy = x0 * k, y0 * k

    def solid(s) -> str:
        v = [s.dxf.vtx0, s.dxf.vtx1, s.dxf.vtx3, s.dxf.vtx2]
        return "M" + " L".join(pt(p.x, p.y) for p in v) + "Z"

    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vx:.4f} {vy:.4f} '
           f'{W:.4f} {H:.4f}" width="{W:.3f}mm" height="{H:.3f}mm">',
           '<defs><linearGradient id="sheen" x1="0" y1="0" x2="1" y2="1">'
           '<stop offset="0" stop-color="#fff" stop-opacity=".10"/>'
           '<stop offset="1" stop-color="#000" stop-opacity=".12"/>'
           '</linearGradient></defs>']
    board = "M" + " L".join(pt(*p) for p in edge) + "Z"
    svg.append(f'<path d="{board}" fill="{MASK}" stroke="{MASK_EDGE}" '
               'stroke-width="0.3"/>')
    svg.append(f'<path d="{board}" fill="url(#sheen)"/>')

    # Pads: gold, the through-hole header pins with a tinned joint
    for ins in layers["GTS"]:
        geo, circles = [], []
        for v in flat(ins):
            if v.dxftype() == "SOLID":
                geo.append(solid(v))
            elif v.dxftype() == "CIRCLE":
                circles.append(v)
        if geo:
            svg.append(f'<path d="{"".join(geo)}" fill="{GOLD}"/>')
        for c in circles:
            svg.append(f'<circle cx="{X(c.dxf.center.x):.4f}" '
                       f'cy="{Y(c.dxf.center.y):.4f}" '
                       f'r="{c.dxf.radius * k:.4f}" fill="{GOLD}"/>')
    # The mikroBUS header: a tinned joint on each pin (pads drawn above when
    # the DXF has them; drawn here too when there is no DXF)
    if parts.get("header") or dxf != "-":
        origin = parts.get("header_origin_mm", [0.0, 0.0])
        for hx, hy in HEADER_PADS:
            cx, cy = origin[0] + hx, origin[1] + hy
            if dxf == "-":
                svg.append(f'<circle cx="{cx:.4f}" cy="{Y(cy / k):.4f}" '
                           f'r="0.92" fill="{GOLD}"/>')
            svg.append(f'<circle cx="{cx:.4f}" cy="{Y(cy / k):.4f}" r="0.62" '
                       f'fill="{TIN}" stroke="#8d9299" stroke-width="0.08"/>')

    # Silkscreen: stroked polylines and lines, filled solids and circles
    for e in layers["GTO"]:
        for v in flat(e):
            t = v.dxftype()
            if t == "SOLID":
                svg.append(f'<path d="{solid(v)}" fill="{SILK}"/>')
            elif t == "POLYLINE":
                pts = [(p.x, p.y) for p in v.points()]
                width = (v.dxf.get("default_start_width", 0) or 0.15 / k) * k
                svg.append(f'<polyline points="{" ".join(pt(*p) for p in pts)}" '
                           f'fill="none" stroke="{SILK}" stroke-width="{width:.4f}"/>')
            elif t == "LINE":
                svg.append(f'<line x1="{X(v.dxf.start.x):.4f}" y1="{Y(v.dxf.start.y):.4f}" '
                           f'x2="{X(v.dxf.end.x):.4f}" y2="{Y(v.dxf.end.y):.4f}" '
                           f'stroke="{SILK}" stroke-width="0.15"/>')
            elif t == "CIRCLE":
                svg.append(f'<circle cx="{X(v.dxf.center.x):.4f}" '
                           f'cy="{Y(v.dxf.center.y):.4f}" r="{v.dxf.radius * k:.4f}" '
                           f'fill="{SILK}"/>')
    for t in parts.get("texts", []):
        tx, ty = t["at_mm"]
        svg.append(f'<text x="{tx:.3f}" y="{Y(ty / k):.3f}" fill="{SILK}" '
                   f'font-family="Arial, Helvetica, sans-serif" font-weight="bold" '
                   f'font-size="{t.get("size", 1.2)}" text-anchor="{t.get("anchor", "middle")}" '
                   f'dominant-baseline="central"'
                   + (f' transform="rotate({t["rotate"]} {tx:.3f} {Y(ty / k):.3f})"'
                      if t.get("rotate") else "")
                   + f'>{t["text"]}</text>')

    # Part bodies from the parts file (mm, centre and size)
    for p in parts["parts"]:
        cx, cy = p["at_mm"]
        pw, ph = p["size_mm"]
        x, y = cx - pw / 2, Y(cy / k) - ph / 2
        kind = p["kind"]
        if kind == "ic":
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.12" fill="#1c1c1e" stroke="#000" stroke-width="0.05"/>')
            svg.append(f'<circle cx="{x + 0.35:.3f}" cy="{y + 0.35:.3f}" r="0.14" '
                       'fill="#3a3a3e"/>')
        elif kind in ("resistor", "zero_ohm", "capacitor", "led"):
            body = {"resistor": "#1b1b1b", "zero_ohm": "#1b1b1b",
                    "capacitor": "#b49a72", "led": "#f2f6f2"}[kind]
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       f'fill="{TIN}"/>')
            if ph > pw:
                svg.append(f'<rect x="{x:.3f}" y="{y + ph * 0.22:.3f}" width="{pw}" '
                           f'height="{ph * 0.56:.3f}" fill="{body}"/>')
            else:
                svg.append(f'<rect x="{x + pw * 0.22:.3f}" y="{y:.3f}" '
                           f'width="{pw * 0.56:.3f}" height="{ph}" fill="{body}"/>')
            if kind == "led":
                tint = p.get("color", "#9fe0a8")
                svg.append(f'<rect x="{x + pw * 0.3:.3f}" y="{y + ph * 0.2:.3f}" '
                           f'width="{pw * 0.4:.3f}" height="{ph * 0.6:.3f}" '
                           f'fill="{tint}" opacity=".8"/>')
        elif kind == "button":          # tactile switch with a round cap
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.5" fill="#c3c7cc" stroke="#7d838a" stroke-width="0.12"/>')
            r = min(pw, ph) * 0.36
            ring = p.get("ring", "#7bd88f")
            svg.append(f'<circle cx="{cx:.3f}" cy="{Y(cy / k):.3f}" r="{r:.3f}" '
                       f'fill="#eef0f2" stroke="{ring}" stroke-width="{r * 0.28:.3f}"/>')
        elif kind == "relay":
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.4" fill="#16171a" stroke="#000" stroke-width="0.1"/>')
            svg.append(f'<text x="{cx:.3f}" y="{Y(cy / k):.3f}" fill="#9aa0a6" '
                       'font-family="Arial, sans-serif" font-size="1.1" '
                       'text-anchor="middle" dominant-baseline="central">G6D</text>')
        elif kind == "terminal":        # screw terminal block, screws along x
            n = int(p.get("ways", 2))
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.3" fill="#2f7d3a" stroke="#1f5a28" stroke-width="0.12"/>')
            for i in range(n):
                sx = x + pw * (i + 0.5) / n
                svg.append(f'<circle cx="{sx:.3f}" cy="{y + ph * 0.42:.3f}" '
                           f'r="{min(pw / n, ph) * 0.3:.3f}" fill="#c9ccd0" '
                           'stroke="#6b7076" stroke-width="0.1"/>')
        elif kind == "digit":           # 7-segment display body, segments off
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.3" fill="#232427" stroke="#000" stroke-width="0.1"/>')
        elif kind == "buzzer":          # round piezo transducer, sound hole
            r, cyy = pw / 2, Y(cy / k)
            svg.append(f'<circle cx="{cx:.3f}" cy="{cyy:.3f}" r="{r:.3f}" '
                       'fill="#1d1e21" stroke="#000" stroke-width="0.15"/>')
            svg.append(f'<circle cx="{cx:.3f}" cy="{cyy:.3f}" r="{r * 0.82:.3f}" '
                       'fill="none" stroke="#2c2e33" stroke-width="0.25"/>')
            svg.append(f'<circle cx="{cx:.3f}" cy="{cyy:.3f}" r="{r * 0.16:.3f}" '
                       'fill="#050506" stroke="#3a3c42" stroke-width="0.1"/>')
            svg.append(f'<text x="{cx + r * 0.5:.3f}" y="{cyy + r * 0.5:.3f}" '
                       'fill="#8a8d93" font-family="Arial, sans-serif" '
                       'font-size="1.6" text-anchor="middle" '
                       'dominant-baseline="central">+</text>')
        elif kind == "oled":            # display module: frame, dark glass
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.2" fill="#3b3d42" stroke="#1a1b1e" stroke-width="0.1"/>')
            ax, ay = p.get("active_mm", [pw * 0.9, ph * 0.9])
            acx, acy = p.get("active_at_mm", [cx, cy])
            svg.append(f'<rect x="{acx - ax / 2:.3f}" y="{Y(acy / k) - ay / 2:.3f}" '
                       f'width="{ax}" height="{ay}" fill="#050506"/>')
        elif kind == "joystick":        # thumbstick: housing, round cap
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.6" fill="#2a2c30" stroke="#111" stroke-width="0.15"/>')
            r = min(pw, ph) * 0.38
            svg.append(f'<circle cx="{cx:.3f}" cy="{Y(cy / k):.3f}" r="{r:.3f}" '
                       'fill="#4a4d53" stroke="#16171a" stroke-width="0.3"/>')
            svg.append(f'<circle cx="{cx:.3f}" cy="{Y(cy / k):.3f}" r="{r * 0.6:.3f}" '
                       'fill="none" stroke="#3b3e44" stroke-width="0.35"/>')
        elif kind == "battery":         # coin cell in its holder
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       'rx="0.4" fill="#1b1c1f" stroke="#000" stroke-width="0.1"/>')
            r = min(pw, ph) * 0.44
            svg.append(f'<circle cx="{cx:.3f}" cy="{Y(cy / k):.3f}" r="{r:.3f}" '
                       'fill="#c9ccd0" stroke="#8d9299" stroke-width="0.15"/>')
            svg.append(f'<text x="{cx:.3f}" y="{Y(cy / k):.3f}" fill="#6b7076" '
                       'font-family="Arial, sans-serif" font-size="1.4" '
                       'text-anchor="middle" dominant-baseline="central">+</text>')
        elif kind == "crystal":         # small metal can
            svg.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{pw}" height="{ph}" '
                       f'rx="{min(pw, ph) / 2:.3f}" fill="#c9ccd0" stroke="#8d9299" '
                       'stroke-width="0.1"/>')
    svg.append("</svg>")
    Path(out).write_text("\n".join(svg), encoding="utf-8")
    print(json.dumps({"origin_mm": [round(vx, 4), round(y1 * k, 4)],
                      "size_mm": [round(W, 4), round(H, 4)],
                      "px": [round(W * 20), round(H * 20)]}))


if __name__ == "__main__":
    main(*sys.argv[1:4])
