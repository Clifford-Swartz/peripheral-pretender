# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Board art for the Curiosity PyKit Explorer: no design files (Gerbers)
are published for either board, so the picture is composed from the top
renders in the kit's workshop slides (UC_CircuitPython_Workshop.pptx in
github.com/CoffeeTronics/PyKit-Explorer-Code-Modules): the PyKit Ruler
(04-12387 rev 4, image51) with the Curiosity CircuitPython board
(04-12379 rev 4, image49) on it, turned 180 degrees so that its J9 lands
on the Ruler's J2 and its USB-C at the right-hand end.

    py tools/pp_web/boards/pykit/make_art.py <workshop.pptx> [out.png]

Scales, measured on the renders: the Ruler's centimetre scale is 5.03
px/mm (1 cm at x 52, 30 cm at x 1512); the Curiosity board's 0.1-inch
header pitch is 4.92 px/mm. The output is 10 px/mm (board.json
"px_per_mm"), 306.6 x 40.8 mm. to_mm() gives the part positions in
board.json.
"""

from __future__ import annotations

import io
import sys
import zipfile
from pathlib import Path

from PIL import Image

RULER_PX_MM = 5.03
BOARD_PX_MM = 4.92
OUT_PX_MM = 10
SIZE_MM = (306.6, 40.8)
# The Curiosity board on the Ruler: its render pixel (u, v) lands on Ruler
# pixel (TX - S*u, TY - S*v): J9's inner header rows over the Ruler's J2
S = RULER_PX_MM / BOARD_PX_MM
TX, TY = 1521, 179
RULER_H_PX = 205


def ruler_mm(x: float, y: float) -> tuple[float, float]:
    """Ruler render pixel to board mm (origin bottom-left, y up)."""
    return (round(x / RULER_PX_MM, 1),
            round((RULER_H_PX - y) / RULER_PX_MM, 1))


def board_mm(u: float, v: float) -> tuple[float, float]:
    """Curiosity board render pixel to board mm."""
    return ruler_mm(TX - S * u, TY - S * v)


def clear_background(img: Image.Image) -> Image.Image:
    """The render's white surroundings (connected to its border) made
    transparent, so the Ruler shows around the board."""
    w, h = img.size
    px = img.load()
    seen = set()
    todo = [(x, y) for x in range(w) for y in (0, h - 1)]
    todo += [(x, y) for y in range(h) for x in (0, w - 1)]
    while todo:
        x, y = todo.pop()
        if (x, y) in seen or not (0 <= x < w and 0 <= y < h):
            continue
        seen.add((x, y))
        r, g, b, a = px[x, y]
        if a and min(r, g, b) < 225:
            continue
        px[x, y] = (0, 0, 0, 0)
        todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    return img


def compose(pptx: Path) -> Image.Image:
    with zipfile.ZipFile(pptx) as z:
        ruler = Image.open(io.BytesIO(z.read("ppt/media/image51.png")))
        cp = Image.open(io.BytesIO(z.read("ppt/media/image49.png")))
    ruler = ruler.convert("RGBA")
    cp = clear_background(cp.convert("RGBA")).rotate(180)
    scale = OUT_PX_MM / RULER_PX_MM
    out = ruler.resize((round(SIZE_MM[0] * OUT_PX_MM),
                        round(SIZE_MM[1] * OUT_PX_MM)), Image.LANCZOS)
    w = round(cp.width * S * scale)
    h = round(cp.height * S * scale)
    cp = cp.resize((w, h), Image.LANCZOS)
    # After the turn, render pixel (0, 0) is the bottom-right corner
    left = round((TX - S * cp.width / (S * scale)) * scale)
    top = round((TY - S * 153) * scale)
    out.alpha_composite(cp, (left, top))
    return out.convert("RGB")


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    out = Path(argv[1]) if len(argv) > 1 else Path(__file__).with_name(
        "top.png")
    compose(Path(argv[0])).save(out, optimize=True)
    print(f"wrote {out}")
    for name, mm in (("RESET", board_mm(87, 118)), ("USER", board_mm(636, 127)),
                     ("D13", board_mm(94, 93)), ("NEO D3", board_mm(458, 81)),
                     ("D1", ruler_mm(320, 83.75)), ("D2", ruler_mm(320, 120.5)),
                     ("D3", ruler_mm(350.75, 120.5)),
                     ("D4", ruler_mm(350.75, 83.75)),
                     ("IMU", ruler_mm(875, 116.7)),
                     ("LCD", ruler_mm(489.5, 100.5)),
                     ("SP1", ruler_mm(255, 105))):
        print(f"{name:7s} {mm}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
