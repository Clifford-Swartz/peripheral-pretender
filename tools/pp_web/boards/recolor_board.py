# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Recolour a tracespace board render (the "<name>.top.svg" from
tracespace 4.2.1) and crop it to the board outline.

    py tools/pp_web/boards/recolor_board.py top.svg out.svg \\
        --mask "#a8101a" --edge 458,600,58980,99026 --flip 118447

--edge is the board outline in Gerber micrometres (x, y of the lower-left
corner, width, height); --flip is the SVG's y flip (the viewBox height
tracespace wrote). Rasterize out.svg at 20 px/mm with resvg-js. The
EV15N46A uses the defaults: red solder mask over copper, gold pads.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--mask", default="#a8101a", help="solder mask colour")
    ap.add_argument("--opacity", type=float, default=0.82,
                    help="mask opacity: copper shows through as traces")
    ap.add_argument("--edge", default="458,600,58980,99026")
    ap.add_argument("--flip", type=int, default=118447)
    args = ap.parse_args()

    s = Path(args.src).read_text(encoding="utf-8")
    colors = {"fr4": "#4a0a0c", "cu": "#e8a0a0", "cf": "#d9b36a",
              "sm": f"{args.mask}; opacity: {args.opacity}", "ss": "#ffffff",
              "sp": "#bbbbbb", "out": "#000000"}
    for layer, color in colors.items():
        s, n = re.subn(r"(\.\w+_%s \{color: )[^}]*\}" % layer,
                       r"\g<1>%s;}" % color, s, count=1)
        assert n == 1, f"no .{layer} style in {args.src}"
    x0, y0, w, h = (int(v) for v in args.edge.split(","))
    top = args.flip - (y0 + h)          # SVG y grows downwards
    s, n = re.subn(r'viewBox="[^"]*" width="[^"]*" height="[^"]*"',
                   f'viewBox="{x0} {top} {w} {h}" '
                   f'width="{w / 1000}mm" height="{h / 1000}mm"', s, count=1)
    assert n == 1, "no viewBox"
    Path(args.out).write_text(s, encoding="utf-8")


if __name__ == "__main__":
    main()
