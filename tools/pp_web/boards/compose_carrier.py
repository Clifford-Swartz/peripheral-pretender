# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Put a board render into its carrier's socket: one SVG of both.

    py tools/pp_web/boards/compose_carrier.py carrier.svg W,H board.svg w,h \
        --place X,Y,ROT [--point u,v ...] > out.svg

A Curiosity Nano on a Curiosity Nano Base, for example: both SVGs come from
recolor_board.py (sizes in mm). --place puts the board's lower-left corner
(its pick-and-place origin) at carrier mm (X, Y) and turns it ROT degrees
clockwise (0, 90, 180, 270). The result is in carrier mm; rasterize it at
20 px/mm. Each --point u,v (board mm, e.g. an LED from the board's
pick-and-place file) is printed in carrier mm on stderr, for board.json.
"""

from __future__ import annotations

import argparse
import re
import sys


def to_carrier(u: float, v: float, x: float, y: float, rot: int):
    """Board mm (y up) to carrier mm (y up), board turned rot clockwise."""
    du, dv = {0: (u, v), 90: (v, -u), 180: (-u, -v), 270: (-v, u)}[rot]
    return round(x + du, 3), round(y + dv, 3)


def inner(svg: str, w: float, h: float) -> str:
    """The SVG as a nested <svg> of w x h mm at the origin."""
    body = re.sub(r"<\?xml[^>]*>", "", svg).strip()
    head, rest = body.split(">", 1)
    head = re.sub(r'\s(?:x|y|width|height)="[^"]*"', "", head)
    head = head.replace("<svg", f'<svg x="0" y="0" width="{w}" height="{h}"',
                        1)
    return head + ">" + rest


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("carrier")
    ap.add_argument("carrier_size", help="W,H in mm")
    ap.add_argument("board")
    ap.add_argument("board_size", help="w,h in mm")
    ap.add_argument("--place", required=True, help="X,Y,ROT")
    ap.add_argument("--point", action="append", default=[], help="u,v")
    args = ap.parse_args(argv)
    W, H = (float(t) for t in args.carrier_size.split(","))
    w, h = (float(t) for t in args.board_size.split(","))
    x, y, rot = args.place.split(",")
    x, y, rot = float(x), float(y), int(rot)
    # The board image's top-left corner is board mm (0, h)
    tx, ty = to_carrier(0.0, h, x, y, rot)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" '
             f'viewBox="0 0 {W} {H}" width="{W}mm" height="{H}mm">',
             inner(open(args.carrier, encoding="utf-8").read(), W, H),
             f'<g transform="translate({tx} {H - ty}) rotate({rot})">',
             inner(open(args.board, encoding="utf-8").read(), w, h),
             "</g>", "</svg>"]
    sys.stdout.write("\n".join(parts) + "\n")
    for p in args.point:
        u, v = (float(t) for t in p.split(","))
        print(f"{p} -> {to_carrier(u, v, x, y, rot)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
