# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""List a Click DXF's pads (mask openings, layer GTS) as centre and size in
mm, to place the part bodies of parts.json: pads of one part sit in pairs or
rows; a body goes over the pair, between its centres.

    py tools/pp_web/clicks/dxf_pads.py <click.dxf> [--min 0.4]

Rounded pads come as many thin slivers; --min (mm) drops pieces narrower or
lower than that, and duplicates are merged.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import render_click as rc  # noqa: E402


def pads(path: str, minimum: float) -> list[tuple[str, float, float, float, float]]:
    layers, _, k = rc.read_dxf(path)
    found = set()
    for e in layers["GTS"]:
        for v in rc.flat(e):
            if v.dxftype() == "SOLID":
                pts = [(p.x, p.y) for p in (v.dxf.vtx0, v.dxf.vtx1,
                                           v.dxf.vtx2, v.dxf.vtx3)]
                xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                w, h = (max(xs) - min(xs)) * k, (max(ys) - min(ys)) * k
                if w >= minimum and h >= minimum:
                    found.add(("rect", round((min(xs) + max(xs)) / 2 * k, 2),
                               round((min(ys) + max(ys)) / 2 * k, 2),
                               round(w, 2), round(h, 2)))
            elif v.dxftype() == "CIRCLE":
                d = 2 * v.dxf.radius * k
                found.add(("round", round(v.dxf.center.x * k, 2),
                           round(v.dxf.center.y * k, 2), round(d, 2),
                           round(d, 2)))
    return sorted(found, key=lambda p: (-p[2], p[1]))


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dxf")
    ap.add_argument("--min", type=float, default=0.4)
    args = ap.parse_args(argv)
    for kind, x, y, w, h in pads(args.dxf, args.min):
        print(f"{kind:5} at {x:7.2f},{y:7.2f}  {w:.2f} x {h:.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
