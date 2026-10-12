# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""ATDF + behavior overlays -> QEMU register tables and device descriptions.

Pipeline: atdf.parse() -> overlay.load_dir() -> semantics.build_ip() per
module -> device.build() -> cgen / jsongen / report writers.
"""

GENERATOR = "generator/atdf2qemu.py"
