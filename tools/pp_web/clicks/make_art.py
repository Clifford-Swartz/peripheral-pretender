# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Render a Click's art in one step: render_click.py, then resvg at 20 px/mm
with a transparent background, resized to round(mm * 20) when resvg rounds
up (the web tests check the size). Prints the renderer's origin and size for
clicks.json.

    py tools/pp_web/clicks/make_art.py <click.dxf | -> <parts.json> <top.png>
        --resvg <folder with node_modules/@resvg/resvg-js>

resvg-js comes from a one-time `npm install --ignore-scripts
@resvg/resvg-js@2.6.2` in a scratch folder (as for the boards). Needs node
and Pillow.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

RASTER_JS = """
const { Resvg } = require(process.argv[2] + "/node_modules/@resvg/resvg-js");
const fs = require("fs");
const r = new Resvg(fs.readFileSync(process.argv[3], "utf8"), {
  fitTo: { mode: "width", value: Number(process.argv[5]) },
  background: "rgba(0,0,0,0)", font: { loadSystemFonts: true } });
fs.writeFileSync(process.argv[4], r.render().asPng());
"""


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dxf", help="the Click's DXF, or - (outline_mm in parts)")
    ap.add_argument("parts", type=Path)
    ap.add_argument("out", type=Path, help="top.png")
    ap.add_argument("--resvg", type=Path, required=True,
                    help="folder whose node_modules holds @resvg/resvg-js")
    args = ap.parse_args(argv)
    from PIL import Image

    with tempfile.TemporaryDirectory() as tmp:
        svg = Path(tmp) / "top.svg"
        info = json.loads(subprocess.check_output(
            [sys.executable, str(HERE / "render_click.py"), args.dxf,
             str(args.parts), str(svg)]))
        w, h = info["px"]
        script = Path(tmp) / "raster.js"
        script.write_text(RASTER_JS, encoding="utf-8")
        subprocess.check_call(["node", str(script), str(args.resvg.resolve()),
                               str(svg), str(args.out), str(w)])
    im = Image.open(args.out)
    if im.size != (w, h):
        im.resize((w, h), Image.LANCZOS).save(args.out)
    print(json.dumps({"origin_mm": info["origin_mm"],
                      "size_mm": info["size_mm"]}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
