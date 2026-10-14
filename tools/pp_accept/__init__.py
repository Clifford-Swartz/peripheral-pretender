# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Acceptance with Microchip's own code: build a kit's Harmony example
projects with XC32 and play scenarios on the twin.

    py -m tools.pp_accept build <examples clone> --kit <kit> --out <dir>
    py -m tools.pp_accept run <elf> --machine <m> --steps "wait 1; press; wait .5; release"
    py -m tools.pp_accept suite acceptance/<kit>.yaml --elves <dir> [--json out.json]
"""
