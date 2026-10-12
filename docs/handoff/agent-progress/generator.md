# generator component progress

## Status
- [x] Read spec v0-vertical-slice.md, microchip-data.md, boot-path.md
- [x] Datasheet text summaries per chapter: scratchpad/generator/ds/*.txt (made by dsregs.py from
      scratchpad/factcheck-microchip-dfp-datasheet/ds.txt)
- [x] generator/mchpgen/atdf.py, overlay.py, semantics.py written and working (all 37 modules)
- [x] device.py, cgen.py, jsongen.py, report.py, nvmrows.py, cli.py + CLI generator/atdf2qemu.py
- [x] pp-qemu/include/hw/mchp/mchp_device.h (hand-written, provenance header)
- [x] Overlays (15 IPs) in overlays/ -- all apply; SERCOM USART_INT matches research table
- [x] Emitted (2026-10-12): pp-qemu include/hw/mchp/gen/*.h, include/hw/arm/pic32cm5164jh01100_gen.h,
      hw/arm/pic32cm5164jh01100_nvmrows.h, devices/pic32cm5164jh01100.json,
      generator/out/report-pic32cm5164jh01100.md
- [x] Tests tests/generator/ (+ shim/) incl. gcc -fsyntax-only + real QEMU env compile: 82 pass
- [x] generator/README.md
- [x] MchpInstanceInfo gained view_sel_offset/shift/length; tests moved shared paths to tests/generator/gen_support.py
- DONE: final report via StructuredOutput (status done)

## Design decisions (keep consistent if resuming)
- Prefixes: MCHP_<IP>[_<MODE>]_<REG> for REG/FIELD; tables mchp_<ip>[_<mode>]_regs; NREGS #define literal;
  MCHP_<IP>_SIZE = register extent rounded up to 4 (PORT 0x180, DSU 0x2000).
- Nested group (PORT GROUP x3 @0x80) flattened into one table, names "GROUP[g].REG"; constants are
  group-relative: MCHP_PORT_GROUP_<REG> + MCHP_PORT_GROUP_STRIDE/COUNT/OFFSET.
- Register-level modes (BAUD FRAC...) -> extra REG <REG>_<REGMODE> like CMSIS.
- Moded IPs: MchpRegView table mchp_<ip>_views[] {name, value, regs, nregs} + selector macros;
  default view = mode selected by the reset value of the selector (SERCOM USART_EXT, TC COUNT16, RTC MODE0).
- Overlay reset_overrides: "REG" -> patches table reset; "REG[i]"/"GROUP[g].REG[i]" -> MchpResetOverride.
- impl = union(bitfields) | reset bits (PAC STATUSC bit 24); all ones if no bitfields.
- sync: {FIELD: BIT | REG.BIT} or scalar BIT / "GENCTRL{n}" (per element: element i raises bit0 << i).
- Instance table includes all 55 instances; flags MCHP_INST_CORE/FUSES/NO_REGS; alias_base/alias_size
  for PORT_IOBUS (0x60000000, 0x200), DIVAS_IOBUS (0x60000200, 0x20), DSU_EXT (0x41002100, 0x100).
- slot_size: 0x400 unless register extent > 0x400 (pow2 round up) or io segment smaller (DIVAS 0x20).
- NVM rows: OTP1 = 0xE000FFFF/0xFFFFFF80 (NVMP 8192, PSZ 3, DFP 128), OTP5 = ADC bias 4s, OSC32K CAL 0x3F,
  CAL48M 0x200220, reserved 1s; USER 0xB15088FB/0xFFFFF8BB; serial default 0x50500000,1,2,3.

## Commands that work
- ATDF: C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf
- py -I scratchpad/generator/explore3.py <atdf> MOD1,MOD2   (dump module registers)
- py -I scratchpad/generator/dsregs.py <ds.txt> "(GCLK)"    (datasheet register summary)

- Run tests: cd project; py -m pytest tests/generator -q   (needs C:/msys64/ucrt64/bin/gcc.exe)
- Generate: py generator/atdf2qemu.py --atdf <atdf> --qemu-out C:/Users/C19257/Desktop/Code/pp-qemu --json-out devices --report generator/out/report-pic32cm5164jh01100.md
- GOTCHA: backslashes in Bash heredocs get unescaped by the tool JSON layer; use Edit for code with 
.
