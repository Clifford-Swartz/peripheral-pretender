# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The VSIX build bundles every ELF the engine starts on its own."""

import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def _build_vsix():
    spec = importlib.util.spec_from_file_location(
        "build_vsix", ROOT / "scripts" / "build_vsix.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_bundled_firmware_covers_boards_and_templates():
    """A template whose firmware is not in the VSIX runs the board's demo
    instead (the Rotary/OLED C template showed a blank OLED in 0.6.2)."""
    firmware = set(_build_vsix().bundled_firmware())
    web = ROOT / "tools" / "pp_web"
    for t in json.loads((web / "templates.json").read_text(encoding="utf-8")):
        if t.get("firmware"):
            assert t["firmware"] in firmware, t["id"]
    for b in (web / "boards").glob("*/board.json"):
        demo = json.loads(b.read_text(encoding="utf-8")).get("demo_elf")
        if demo:
            assert demo in firmware, b.parent.name
