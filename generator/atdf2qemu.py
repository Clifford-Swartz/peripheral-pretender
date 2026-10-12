#!/usr/bin/env python3
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""ATDF + behavior overlays -> QEMU register tables and device JSON.

Example (from the project root):
  py generator/atdf2qemu.py --atdf <PIC32CM5164JH01100.atdf> \
      --qemu-out ../pp-qemu --json-out devices \
      --report generator/out/report-pic32cm5164jh01100.md
See generator/README.md.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from mchpgen.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
