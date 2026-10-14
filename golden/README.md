# Golden data from silicon

Captured from real boards, kept byte-exact (`.gitattributes`: no line-ending
conversion), and checked against the twin by `tests/golden/` without hardware.

## `pic32cm1216mc00048-a1/`

PIC32CM MC00 Curiosity Pro (EV15N46A), PIC32CM1216MC00048 rev A1, EDBG probe
ATML3396053000000017, 2026-10-13.

| file | content | how |
|---|---|---|
| `reset.json` | 811 register elements right after a reset | pyOCD halt connect, DFP reset-and-halt (SYSRESETREQ), each register read at its width; read-side-effect and write-only registers skipped |
| `hello-vcom.bin` | VCOM bytes of `firmware/hello` (board `pic32cm_mc00_cpro`) | `py -m harness.hw_run firmware/hello/build/pic32cm_mc00_cpro/hello.elf --allow-program --target pic32cm1216mc00048 --pack <MC DFP .pack> --out ...` |

The twin reproduces `hello-vcom.bin` byte for byte when configured as this
part: `-global pic32cm1216mc00048-soc.silicon-rev=A1 -global
pic32cm1216mc00048-soc.serial-number=F79A74AF51524B5939202020FF190418`.
`tests/golden/test_mc00_silicon.py` lists the register differences that are
allowed, with the reason for each.
