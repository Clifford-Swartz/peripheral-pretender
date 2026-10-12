# generator — done

The prototype is now a maintainable generator, `generator/atdf2qemu.py` (package `generator/mchpgen/`, stdlib + PyYAML), with all 15 overlays from spec 6.1. Generated files are in place in pp-qemu and the project, and all 82 generator tests pass.

What it reads from the ATDF (PIC32CM5164JH01100, DFP 1.3.221):
- 18 memory segments
- 55 instances with IP id/version, base and slot size; PORT, DSU and DIVAS each appear once with their alias views
- 32 NVIC lines, with shared vectors 0/9/10/20/21/22 and each instance's OR-gate input
- PAC/GCLK/DMAC ids and all 344 instance parameters
- 578 pad signals
- register modes (SERCOM 6 views, TC COUNT8/16/32, RTC MODE0-2), register-level modes (BAUD FRAC etc.), arrays, and the nested PORT GROUP (3 groups at 0x80)

Overlays cover W1C/RO, aliases, enable protection, sync, strobes, hooks, dynamic registers, reset_overrides (all values the task listed) and user-row reset_from mappings. Heuristics handle everything else, and every source is listed in the report.

Checks:
- The generated SERCOM USART_INT table equals the research table. The test parses `microchip-data.md` directly.
- All 34 generated headers compile with UCRT64 gcc 16.2 using `-std=gnu11 -Wall -Wextra -fsyntax-only`, each alone and all together, against a shim. QEMU's `registerfields.h` cannot be included standalone (it needs `qemu/osdep.h`), so the shim has a minimal REG/FIELD copy with identical expansion.
- They also compile in the real pp-qemu environment (`qemu/osdep.h`, glib, QEMU's warning flags).
- The C NVM-rows fill function reproduces the Python image byte for byte.

`--check` confirms the committed outputs are current.

## Handoff
Regenerate from the project root with:
`py generator/atdf2qemu.py --atdf C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf --qemu-out C:/Users/C19257/Desktop/Code/pp-qemu --json-out devices --report generator/out/report-pic32cm5164jh01100.md`
Add `--check` in CI. Run the tests with `py -m pytest tests/generator -q`. They need C:/msys64/ucrt64/bin/gcc.exe, or set `PP_GCC`.

**Generated pp-qemu files**
- `include/hw/mchp/gen/<ip>_<id>.h` for: ac_u2245, adc_u2247, can_u2003, ccl_u2225, dac_u2214, divas_u2258, dmac_u2223, dsu_u2209, eic_u2254, evsys_u2256, freqm_u2257, fuses_03929 (constants only), gclk_u2122, icm_u2010, mclk_u2234, mcramc_03727, mtb_u2002, nvmctrl_03929, osc32kctrl_u2246, oscctrl_u2119, pac_u2120, pdec_u2263, pm_u2240, port_u2210, rstc_u2239, rtc_03608, sercom_u2201, smbist_03788, supc_u2117, tc_u2249, tcc_u2213, wdt_u2251.
- `include/hw/arm/pic32cm5164jh01100_gen.h`
- `hw/arm/pic32cm5164jh01100_nvmrows.h`
- Hand-written: `include/hw/mchp/mchp_device.h`.
- The headers include `hw/misc/mchp_regfile.h`, which the regfile agent must create verbatim from spec 5.1.

**Per-IP table symbols (MchpRegInfo tables and counts)**
- `mchp_<ip>_regs` / `MCHP_<IP>_NREGS` / `MCHP_<IP>_SIZE`:
  - GCLK 4 regs, size 0x12C
  - MCLK 9, 0x24
  - OSCCTRL 18, 0x3C
  - OSC32KCTRL 10, 0x20
  - NVMCTRL 19, 0xA0
  - SUPC 7, 0x20
  - PM 2, 0xC
  - RSTC 1, 0x4
  - WDT 8, 0x10
  - PAC 13, 0x44
  - DSU 24, 0x2000
  - DIVAS 7, 0x1C
  - EVSYS 8, 0x14C
  - PORT 42, 0x180
  - also AC, ADC, CAN, CCL, DAC, DMAC, EIC, FREQM, ICM, MCRAMC, MTB, PDEC, SMBIST, TCC
- SERCOM per mode: `mchp_sercom_{usart_ext,usart_int,spis,spim,i2cs,i2cm}_regs` with `MCHP_SERCOM_{USART_EXT,USART_INT,SPIS,SPIM,I2CS,I2CM}_NREGS` (12, 12, 11, 11, 9, 11); `MCHP_SERCOM_SIZE` 0x34.
- TC: `mchp_tc_count{8,16,32}_regs`, `MCHP_TC_COUNT{8,16,32}_NREGS`.
- RTC: `mchp_rtc_mode{0,1,2}_regs`, `MCHP_RTC_MODE{0,1,2}_NREGS`.
- View tables: `mchp_{sercom,tc,rtc}_views` (MchpRegView), `MCHP_<IP>_NVIEWS`, `MCHP_<IP>_DEFAULT_VIEW` (0), selector `MCHP_<IP>_VIEW_SEL_OFFSET/_SHIFT/_LENGTH` (CTRLA.MODE: SERCOM shift 2 / length 3; TC and RTC shift 2 / length 2), mode values `MCHP_SERCOM_MODE_USART_INT` (= 1) etc.

**Reset arrays**
- Overrides:
  - `mchp_gclk_reset_overrides` / `MCHP_GCLK_NRESET_OVERRIDES`: {0x20, 4, 0x106}
  - `mchp_port_reset_overrides` / `MCHP_PORT_NRESET_OVERRIDES`: {0x3F, 1, 0x06}
- Fuse-loaded resets (MchpFuseReset):
  - `mchp_wdt_reset_from` / `MCHP_WDT_NRESET_FROM` (6)
  - `mchp_supc_reset_from` (4; ENABLE inverted from BODVDD_DIS)
  - `mchp_nvmctrl_reset_from` (1; LOCK)
  - Apply after `mchp_regfile_reset()`: read the 32-bit word at `fuse_addr` from the NVM-rows ROM, take `(w & fuse_mask) >> ctz(fuse_mask)`, invert within the field if `invert`, and deposit into `reg_mask` of the register at `reg_offset`.
- Scalar reset fixes are already in the tables: NVMCTRL PARAM 0x08032000 (READ_ONLY), INTFLAG 0x1, CTRLB 0x80; OSC48MCTRL 0x82; OSC48MDIV 0x0B; OSCCTRL STATUS 0x10; MCLK INTFLAG 0x01; PAC STATUSB 0x2 and STATUSC 0x09000000 (impl includes bit 24); RSTC RCAUSE 0x01; SUPC STATUS 0x5; OSCULP32K 0x6.

**SoC tables (in pic32cm5164jh01100_gen.h)**
- `pic32cm5164jh01100_instances[PIC32CM5164JH01100_NUM_INSTANCES]` (55), with index macros `PIC32CM5164JH01100_INST_<NAME>`.
  - Skip entries flagged `MCHP_INST_CORE` (SystemControl, SysTick, NVIC, MPU) and `MCHP_INST_FUSES`.
  - `MCHP_INST_NO_REGS` (PTC): map an unimplemented device at base with slot_size 0x400.
  - Every other entry: `base`, `slot_size`, `regs`/`nregs`/`regs_size`/`ovr`/`novr`, `views`/`nviews`/`view_sel_*`, `reset_from`, `irq`/`irq_input`, `pac_id`, `gclk_core`/`gclk_slow`, `dmac_rx`/`dmac_tx`.
- Aliases:
  - PORT: `alias_base` 0x60000000, size 0x200.
  - DIVAS: 0x60000200, size 0x20.
  - DSU_EXT: 0x41002100, size 0x100, inside the 0x2000 DSU slot. Use an overlapping alias, or decode internally.
- IRQs: `pic32cm5164jh01100_irq_lines[PIC32CM5164JH01100_NUM_IRQ_LINES]` gives `{name, line, nsources}`.
  - When nsources > 1, create TYPE_OR_IRQ with nsources inputs into NVIC `line`, and connect each instance to OR input `irq_input`.
  - SYSTEM (line 0): MCLK=0, OSCCTRL=1, OSC32KCTRL=2, PAC=3, PM=4, SUPC=5.
  - Lines 9/10/20/21/22: SERCOM0/1 and TC0/1/2 on input 0; SERCOM6/7 and TC5/6/7 on input 1.
- Other tables:
  - `pic32cm5164jh01100_mem_segments` (18), plus `PIC32CM5164JH01100_<SEG>_BASE/_SIZE`
  - `pic32cm5164jh01100_gclk_channels` (53)
  - `pic32cm5164jh01100_dmac_triggers` (60)
  - `pic32cm5164jh01100_signals` (578; `pin` = group*32+n, matching port-out[96])
  - Core parameters: `PIC32CM5164JH01100_NUM_IRQ` 32, `_NVIC_PRIO_BITS` 2, `_CM0P_REV`, `_MPU_PRESENT`, `_VTOR_PRESENT`, `_DSU_DID_SIGNATURE`.
- NVM rows: `pic32cm5164jh01100_nvmrows_fill(uint8_t *buf, const uint32_t serial[4])`.
  - Constants: `PIC32CM5164JH01100_NVMROWS_BASE` 0x00800000, `_SIZE` 0x10000, `pic32cm5164jh01100_serial_default[4]`, `pic32cm5164jh01100_serial_addr[4]`.
  - Requires qemu/osdep.h first (memset, ARRAY_SIZE) and uses stl_le_p.

**Hook flags**
- GCLK: CTRLA post_write (SWRST), SYNCBUSY dynamic, GENCTRL post_write, PCHCTRL post_write (CHEN reads back from storage).
- MCLK: CPUDIV post_write (set INTFLAG.CKRDY again, update cpuclk).
- OSCCTRL:
  - STATUS dynamic (OSC48MRDY = OSC48MCTRL.ENABLE; XOSCRDY)
  - OSC48MSYNCBUSY, DPLLSYNCBUSY, DPLLSTATUS dynamic
  - post_write on XOSCCTRL, OSC48MCTRL, OSC48MDIV, DPLLCTRLA, DPLLRATIO, DPLLCTRLB, DPLLPRESC
- OSC32KCTRL: STATUS dynamic; RTCCTRL, XOSC32K, OSC32K post_write.
- NVMCTRL: CTRLA post_write (CMDEX 0xA5; CMDEX is rdz).
- SUPC: STATUS dynamic, BODVDD post_write.
- WDT: CTRLA post_write (keep ALWAYSON set-only; it is in wmask), SYNCBUSY dynamic, CLEAR post_write (0xA5 key; write-only).
- PAC: WRCTRL post_write (update STATUSx).
- DSU: CTRL post_write (write-only strobes). DID: poke REVISION from silicon-rev.
- DIVAS: DIVISOR and SQRNUM post_write; STATUS: BUSY RO, DBZ W1C.
- EVSYS: CTRLA and SWEVT post_write.
- PORT: DIR, OUT, WRCONFIG, PINCFG post_write; IN dynamic. Alias writes run the target's post_write with ri = the alias.
- SERCOM:
  - all modes: CTRLA and CTRLB post_write, SYNCBUSY dynamic, DATA pre_read + post_write (CTRLB also has BUSY_BUSERR)
  - I2CM also: STATUS and ADDR post_write
- PM and RSTC: no hooks. The RSTC model pokes RCAUSE after reset.

**Generic fallback for DYNAMIC registers in a regblock**: return the stored value. The reset values are chosen so that this works: every SYNCBUSY reads 0, and OSCCTRL.STATUS and SUPC.STATUS hold their ready values.

## Tests
- [pass] tests/generator/test_atdf.py (9): 55 instances / 37 modules / 18 segments / 578 signals / 344 params; 32 NVIC lines, shared vectors 0/9/10/20/21/22 with OR inputs, SERCOM4 on IRQ 13; PAC/GCLK/DMAC ids; slots (DSU 0x2000, MTB 0x1000, DIVAS 0x20) and aliases; signal pmux and pin; mode views and selectors; arrays; PORT nesting
- [pass] tests/generator/test_sercom_usart.py (11): USART_INT offsets/sizes/resets/impl equal the research markdown table, and impl/wmask/w1c/rdz/enprot/syncbusy/flags equal the prototype table in microchip-data.md. The only difference is the new READ_ONLY flag on SYNCBUSY (spec 5.2). The prose rules (W1C/RO/enable-protect/sync/strobe) are checked against the doc text.
- [pass] tests/generator/test_port.py (20): DIR/OUT SET/CLR/TGL aliases stay within each of the 3 groups; IN dynamic+RO; WRCONFIG write-only with post_write; group-relative constants; PMUX15 override row in the header
- [pass] tests/generator/test_resets.py (13): GENCTRL[0]=0x106 override; PORT GROUP[0].PMUX[15]=0x06 at offset 0x3F; NVMCTRL PARAM 0x08032000 read-only, CTRLB 0x80, INTFLAG READY=1 and RO; OSC48MCTRL 0x82 / OSC48MDIV 0x0B; MCLK CKRDY; PAC STATUSB/STATUSC (bit 24 reads back); RCAUSE POR. WDT/SUPC/NVMCTRL reset_from decoded with both the production and the erased user row. NVM image words; user row decodes to Table 10-3 settings
- [pass] tests/generator/test_heuristics.py (8): TC without overlay: SYNCBUSY dynamic+RO, SWRST rdz, CTRLBSET/CLR pair with CMD self-clearing, INTFLAG W1C, INTEN alias, sync-by-name. Also MTB missing initval and no-bitfield impl, ICM IER/IDR aliases onto IMR, DMAC CHINTEN pair, and the report's heuristics section
- [pass] tests/generator/test_overlays.py (10): All 15 spec overlays present and applied. Version guard: versions [2.0.0] gives a warning, the overlay is not applied and heuristics take over (INTFLAG w1c 0xBF). Errors for unknown register, field, sync bit, reset-override target and key, a wrong file name, and a modes mismatch. The ATDF-vs-overlay conflict is reported
- [pass] tests/generator/test_compile.py (6): Spec 5.1 block copied verbatim into the shim. Each of the 34 headers compiles alone, and all compile together, with UCRT64 gcc 16.2 -std=gnu11 -Wall -Wextra -fsyntax-only: no diagnostics. The C nvmrows_fill() image equals the Python image. One translation unit (qemu/osdep.h + SoC header + nvmrows header) compiles cleanly with QEMU's flags plus -Wextra -Wno-unused-parameter
- [pass] tests/generator/test_outputs.py (5): Deterministic output; 80 columns, no tabs, no //, provenance header present; JSON content; --check exit codes; committed pp-qemu/devices/report outputs equal a fresh generation
- [pass] py -m pytest tests/generator tests/runner (coexistence with other components' tests): 165 passed; the whole tests/ tree collects 169 tests

## Deviations
- Overlay vocabulary extensions (all documented in generator/README.md): `w1c: all`; `enable_protected: [fields]`; `sync` as a scalar bit, as `REG.BIT` (OSC48MSYNCBUSY, DPLLSYNCBUSY) or as `NAME{n}` (per-element GENCTRLn, stored as element 0's bit with element i = bit0 << i); `reset_overrides` key `REG` patches the table reset while `REG[i]` / `GROUP[g].REG[i]` emit MchpResetOverride; `reset_from` is a validated mapping that also emits C `MchpFuseReset` tables; `note`/`references`. The alias storage for INTENSET/INTENCLR is INTENSET itself (spec 5.2), not the prototype's 'INTEN'.
- mchp_device.h defines more than MchpInstanceInfo (MchpRegView, MchpFuseReset, MchpIrqLine, MchpMemSegment, MchpSignalInfo, MchpPeriphRef). MchpInstanceInfo has extra fields: alias_name/base/size, irq_input, flags, regs_size, ovr/novr, views/nviews, view_sel_offset/shift/length, reset_from/nreset_from.
- PORT is one flattened table for all 3 groups (42 entries named 'GROUP[g].REG', size 0x180) so a single regblock/regfile covers it. REG/FIELD constants are group-relative (MCHP_PORT_GROUP_*).
- MCHP_<IP>_SIZE is the register extent rounded up to 4 bytes (SERCOM 0x34). Value-group constants (#define MCHP_..._<FIELD>_<VALUE>) are also emitted.
- The default table of multi-mode instances is the view selected at reset: SERCOM USART_EXT (USART_EXT and USART_INT layouts are identical), TC COUNT16, RTC MODE0.
- Reset overrides beyond the task list, each documented in its overlay: OSCCTRL.STATUS 0x10 (OSC48MRDY, the observable steady state; the register is also DYNAMIC), SUPC.STATUS 0x05 (production user row), OSC32KCTRL.OSCULP32K 0x06 (DS reset 0x0000XX06), RSTC.RCAUSE 0x01, DSU.DID 0x11060100 (same as ATDF). The spec values that already match the ATDF (CTRLB 0x80, OSC48M*, CKRDY, PAC STATUSB/C) are restated and reported as 'same'.
- NVMCTRL INTFLAG.READY is read-only per DS 26.6.6 (Access R), so W1C cannot clear it. It is not modeled as W1C followed by re-assertion.
- The compile check uses a minimal REG/FIELD shim because QEMU's registerfields.h needs qemu/osdep.h. I added a second check that compiles in the real pp-qemu environment.
- Tests import shared paths from tests/generator/gen_support.py rather than `from conftest import`, because the new tests/conftest.py (another agent's) would otherwise shadow the module name.

## Open issues
- generator/out/ is listed in the project .gitignore, which I do not own. The spec 5.3 report path is therefore not committed unless that rule changes. test_outputs still checks the locally generated report.
- 15 heuristic warnings remain, for registers marked access=WSYNC/RWSYNC whose sync bit cannot be resolved: no SYNCBUSY bit matches by name (TC PERBUF/CCBUF; TCC PATTBUF/PERBUF/CCBUF; PDEC PRESCBUF/FILTERBUF/CCBUF; SERCOM I2CS CTRLB/DATA), or the IP has no SYNCBUSY register (CAN CCCR, SUPC BODVDD). SUPC BODVDD.ENABLE syncs through STATUS.BVDDSRDY, a ready bit, so no syncbusy mask is given.
- The SERCOM SPI/I2C overlays are minimal (from the DS 31.7/32.7/32.8 summaries); I2C transfer semantics are out of scope for v0.
- Placeholders that need a silicon dump: OTP1 NVM_LOCK (left 0xFF), CAL row, TEMP_LOG, the calibration row values (ADC bias 4, OSC32K CAL 0x3F, CAL48M 0x200220), and the serial-number default (0x50500000, 1, 2, 3).
- PA30 PINCFG30 reset value is unconfirmed (the DS summary says 0x00, while DS 7.4 says SWCLK with pull-up). Only PMUX15 = 0x06 is overridden.
- The ATDF mode mask '6' on SERCOM modes is ignored, and PTC's instance version 7a0 differs from its module version 7b0. Both are noted in the report.
- The pyflakes warnings in generator/prototype/atdf2qemu.py (f-strings without placeholders) are left alone, since the prototype is kept unchanged for reference.

## Commands
- Generate all outputs into pp-qemu, devices/ and generator/out/: `py generator/atdf2qemu.py --atdf C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf --qemu-out C:/Users/C19257/Desktop/Code/pp-qemu --json-out devices --report generator/out/report-pic32cm5164jh01100.md`
- Verify committed outputs are current (exit 1 if stale): `py generator/atdf2qemu.py --atdf C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf --qemu-out C:/Users/C19257/Desktop/Code/pp-qemu --json-out devices --report generator/out/report-pic32cm5164jh01100.md --check`
- Run generator tests (82): `py -m pytest tests/generator -q`
- Lint the new Python code: `py -m pyflakes generator/mchpgen generator/atdf2qemu.py tests/generator`

## Files
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/atdf2qemu.py: CLI entry point (calls mchpgen.cli.main)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/atdf.py: ATDF parser to dataclasses; DFP name/version from .pdsc
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/overlay.py: Overlay YAML loading and schema validation (unknown keys are errors)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/semantics.py: Merges ATDF, overlays and heuristics into per-view MchpRegInfo rows with provenance; version guard; name errors; reset overrides; reset_from; views and selector
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/device.py: Instances, slot sizes, aliases, IRQ sharing and OR inputs, ids, signals
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/nvmrows.py: Default NVM rows image (spec 4.3)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/cgen.py: C header writers (QEMU style, 80 columns)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/jsongen.py: devices JSON writer
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/report.py: Markdown provenance report writer
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/mchpgen/cli.py: Arguments, change-only writes, --check, pruning of stale generated headers
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/README.md: CLI, outputs, C naming, overlay schema, heuristics, tests
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/overlays/: 15 overlays: sercom_u2201, port_u2210, oscctrl_u2119, osc32kctrl_u2246, gclk_u2122, mclk_u2234, nvmctrl_03929, supc_u2117, pm_u2240, rstc_u2239, wdt_u2251, pac_u2120, dsu_u2209, divas_u2258, evsys_u2256 (.yaml)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/devices/pic32cm5164jh01100.json: Generated device description for tools and harness (1.27 MB)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/generator/out/report-pic32cm5164jh01100.md: Generated provenance report (this path is gitignored)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/generator/: Tests: conftest.py, gen_support.py, test_atdf, test_sercom_usart, test_port, test_resets, test_heuristics, test_overlays, test_compile, test_outputs
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/generator/shim/: Compile shim: hw/misc/mchp_regfile.h (spec 5.1 copied verbatim), hw/core/registerfields.h (REG/FIELD only), qemu/bswap.h (stl_le_p)
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/mchp/mchp_device.h: Hand-written types: MchpInstanceInfo, MchpRegView, MchpFuseReset, MchpIrqLine, MchpMemSegment, MchpSignalInfo, MchpPeriphRef, MCHP_INST_* flags
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/mchp/gen/: 32 generated per-IP headers <ip>_<id>.h (fuses_03929.h has constants only; PTC has no registers, so no header)
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/arm/pic32cm5164jh01100_gen.h: Generated SoC description: segments, instances, IRQ lines, GCLK/DMAC refs, signals; includes all IP headers
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/arm/pic32cm5164jh01100_nvmrows.h: Generated pic32cm5164jh01100_nvmrows_fill(uint8_t *buf, const uint32_t serial[4])
