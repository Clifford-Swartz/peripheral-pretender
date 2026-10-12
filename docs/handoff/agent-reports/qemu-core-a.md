# qemu-core-a — done

The pic32cm-jh01-cpro machine is built and working, apart from one documented limitation: MPU_TYPE reads 0 instead of 8 regions. It has the regfile (§5.1), the generic mchp-regblock, the system controllers, the SoC, the board, a qtest and the docs.

- **Firmware run:** hello.elf runs for 10 s under `-d unimp,guest_errors` with no assertion or abort and no guest errors. The only LOG_UNIMP lines are the expected SERCOM4/PORT writes. It hangs at the SERCOM4 DRE poll, which is expected in part A.
- **gdb check (gdb-multiarch, batch mode, against `-s -S`):** the breakpoint on the DRE poll (0x480) is hit in `console_putc('P')`, called from `console_puts` in `main()` after `sys_initialize()`. So the whole clock bring-up passes; there is no hang on OSC48MRDY or on the PCHCTRL CHEN read-back. At that point:
  - GENCTRL0=0x00010106, PCHCTRL22=0x40, OSC48MDIV=0, OSCCTRL.STATUS=0x10
  - NVMCTRL.CTRLB=0x84, APBCMASK=0x08000020, SERCOM4 CTRLA=0x40310106, BAUD=0xF62B
  - SysTick LOAD=47999
  - `info qtree` shows armv7m `clock-in "cpuclk" freq_hz=48 MHz`, and the `mchp_clk_update` trace shows mclk.cpu going 4 MHz → 48 MHz.
- **CPU identity:** the CPUID override works (reads 0x410CC601). With mpu-ns-regions=8, MPU_TYPE still reads 0: QEMU only creates `pmsav7-dregion` for Armv7 cores. I documented this; I did not patch target/arm.
- **Reset causes:** SYSRESETREQ is wired to RSTC. RCAUSE reads POR after power-up, SYST after SYSRESETREQ and WDT after a wrong WDT.CLEAR key, and SRAM survives.
- **Logging:** LOG_UNIMP lines name the instance and register, e.g. `sercom4.CTRLA: write 0x40310104 not modelled`.
- **Memory map:** `info mtree` shows the whole map: 49 register blocks (12 dedicated models, 37 generic), HPB0-3 background unimplemented devices, PTC as an unimplemented device, DSU_EXT, and both IOBUS views.
- **Tests:** my qtest passes 12/12. No regressions: microbit 6/6, stm32l4x5 45/45, device-introspect 6/6, qom-test 79/79, test-hmp 79/79.
- **SERCOM/PORT swap for part B:** each is one line in the SoC's model table; the SERCOM chardev is then connected automatically.

## Handoff
Scratch directory: C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/ (progress.md, build.sh, qtest.sh, run10.sh, gdbcheck.sh, mkgdb.py, rstcheck.py, plus evidence gdbcheck.out and run10.log).

Run every script through the MSYS2 pattern from PowerShell:
$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/.../scratchpad/qemu-core-a/<script>.sh

PART B (SERCOM, PORT, LED0, SW0)
1. In hw/arm/pic32cm_jh_soc.c, add { "SERCOM", TYPE_MCHP_SERCOM } and { "PORT", TYPE_MCHP_PORT } to pic32cm_jh_models[]. Nothing else in the SoC changes.
2. Both types must subclass TYPE_MCHP_REGBLOCK (include/hw/misc/mchp_regblock.h). The SoC calls mchp_regblock_set_info() in instance_init. The base realize then:
   - builds MchpRegBlock.rf from the generated tables;
   - creates MMIO 0 (the slot) and MMIO 1 (alias_base: the PORT IOBUS at 0x60000000, mapped automatically);
   - provides sysbus IRQ 0, wired to the NVIC or to the OR gate for SERCOM0/1/6/7;
   - switches the register view on CTRLA.MODE and handles CTRLA.SWRST.
3. In class_init:
   - set c->pre_read (DATA pops RX only when side_effects is true) and c->post_write;
   - set c->realize (bool fn) for qemu_chr_fe_set_handlers;
   - set c->reset_hold / c->reset_exit (called after the register reset, also on SWRST);
   - set c->log_writes = false;
   - keep the default IRQ (INTFLAG & INTENSET) and call mchp_regblock_update_irq() after state changes made outside MMIO; the base already updates it after every access.
4. SERCOM chardev: declare DEFINE_PROP_CHR("chardev", ...). The SoC sets it from s->sercom_chr[n] (pic32cm_jh_connect_sercom). The board already maps serial_hd(0..4) to SERCOM4, 2, 3, 6, 0.
5. Core clock: input "gclk" (MchpRegBlock.gclk) is already connected to GCLK pch[gclk_core]; SERCOM4 uses pch[22], 48 MHz after Harmony init. The slow clock (pch[17]) is not wired; add a clock input plus a line in pic32cm_jh_connect_clocks() if needed.
6. Kconfig: define MCHP_SERCOM and MCHP_PORT, and add 'select' lines to PIC32CM_JH_SOC in hw/arm/Kconfig.
7. Board: the PORT device is s->soc.periph[PIC32CM5164JH01100_INST_PORT]. LED0 is PC05, index 2*32+5 = 69 in 'port-out'. SW0 is PB19, index 51.
8. Update docs/system/arm/pic32cm-jh01-cpro.rst ("Missing devices" and "Board and SoC properties"). Add tests to tests/qtest/pic32cm-jh-test.c or new files.

ADDING ANOTHER DEDICATED MODEL
Subclass mchp-regblock (the hw/misc/mchp_sysctrl.c sections are short examples) and add one line to pic32cm_jh_models[]. Register constants come from include/hw/mchp/gen/<ip>_<id>.h (A_MCHP_*, R_MCHP_*_MASK, FIELD_EX32).

RUNNING THINGS
- Raw runs (from pp-qemu/build):
  ./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel <elf> -display none -serial stdio -d unimp,guest_errors [-trace 'mchp_*'] [-s -S]
- Raw qtest:
  QTEST_QEMU_BINARY=./qemu-system-arm.exe QTEST_QEMU_ARGS="-L C:/Users/C19257/Desktop/Code/pp-qemu/pc-bios" ./tests/qtest/pic32cm-jh-test.exe --tap -k
- gdb-multiarch is a native Windows binary: give it a Windows path for the ELF (cygpath -m). Its 'file' command fails on the 'Peripheral Pretender' path with the space, so gdbcheck.sh copies the ELF to scratch first.
- Clock frequencies under qtest: QOM property 'qtest-clock-period' on, e.g., /machine/soc/mclk/cpu, /machine/soc/armv7m/cpuclk, /machine/soc/gclk/pch[22], /machine/soc/sercom4/gclk.

## Tests
- [pass] build qemu-system-arm.exe: ninja exit 0; no warnings or errors from the new files (MSYS2 UCRT64 gcc, QEMU flags)
- [pass] qemu-system-arm -M help: 'pic32cm-jh01-cpro    Microchip PIC32CM JH01 Curiosity Pro (EV81X90A, Cortex-M0+)'
- [pass] hello.elf for 10 s with -d unimp,guest_errors: timeout exit 124 (still running, stuck in the DRE poll as expected); stderr empty. Log has 11 lines and no guest errors: 'sercom4: no UART model; serial port not connected', 6 port.GROUP[g].* lines (PINCFG10/11, PMUX5=0x33, PC DIR=0x20, ...), and sercom4.CTRLA 0x40310104, BAUD 0xf62b, CTRLB 0x30000, CTRLA 0x40310106
- [pass] gdb-multiarch batch against -s -S (gdbcheck.sh): At reset: PC=Reset_Handler, RCAUSE=0x01, GENCTRL0=0x106, OSC48MDIV=0x0b. Breakpoint at 0x480 hit in console_putc(c='P') ← console_puts ← main() src/main.c:190, i.e. after sys_initialize(). There: NVMCTRL.CTRLB=0x84, OSCCTRL.STATUS=0x10, OSC48MDIV=0, CAL48M=0x00200220, GENCTRL0=0x00010106, GCLK.SYNCBUSY=0, PCHCTRL22=0x40, APBCMASK=0x08000020, CPUDIV=1, SERCOM4 CTRLA=0x40310106, BAUD=0xf62b, INTFLAG=0, SysTick LOAD=47999, DSU.DID=0x11060100, user row b15088fb/fffff8bb, serial 50500000/1/2/3. Still at 0x480-0x484 after 4 stepi. info qtree: armv7m clock-in "cpuclk" freq_hz=48 MHz; mclk cpu, gclk gen[0] and pch[22] at 48 MHz. Trace: mclk.cpu 4000000 Hz, then 48000000 Hz after the OSC48MDIV write
- [pass] info mtree -f: flash/dataflash/nvm-rows ROM, SRAM, 49 register blocks at their ATDF bases, hpb0-3 backgrounds at prio -1000, ptc unimplemented, dsu_ext view at 0x41002100, port and divas IOBUS views at 0x60000000/0x60000200, armv7m PPB
- [fail] MPU_TYPE with armv7m mpu-ns-regions=8: MPU_TYPE (0xE000ED90) reads 0x00000000, not DREGION=8. Cause: target/arm/cpu.c only adds the 'pmsav7-dregion' property when ARM_FEATURE_V7 is set, so armv7m skips the setting on cortex-m0 and the CPU realizes without an MPU. Documented in the SoC code and the board rst; not patched, since target/arm is outside my paths
- [pass] SCB->CPUID override: 0xE000ED00 reads 0x410CC601 (gdb and qtest), via the machine compat_props global cortex-m0-arm-cpu.midr
- [pass] tests/qtest/pic32cm-jh-test (12 cases): 12/12 in about 7 s. Reset values (GENCTRL0=0x106, PARAM=0x08032000, DID through DSU and DSU_EXT, PAC STATUSB=0x2 and STATUSC=0x09000000, user row words, SYNCBUSY=0, CPUID); ready bits (OSC48MRDY, XOSCRDY with flag, XOSC32KRDY/OSC32KRDY, SUPC 0x5); RCAUSE POR → SYST (AIRCR SYSRESETREQ, SRAM kept) → WDT (wrong CLEAR key) → EXT (QMP system_reset); WDT/SUPC/LOCK reset from the user row; fuse words from a -device loader HEX file, checked across 2 resets; regfile semantics; TC view switch and SWRST; GCLK SWRST; clock tree via qtest-clock-period (4 MHz → 48 MHz, CPUDIV 24 MHz, FDPLL 48 MHz from XOSC, gen1/gen2 → sercom4 gclk 12 MHz, WRTLOCK); MCLK CKRDY on NVIC line 0 through the OR gate; PAC errors; DIVAS; silicon-rev and serial-number
- [pass] microbit-test: 6/6
- [pass] stm32l4x5 exti/syscfg/rcc/gpio/usart qtests: 9/9, 10/10, 5/5, 14/14, 7/7 (re-run on the final tree)
- [pass] device-introspect-test: 6/6 (instantiates every device type, including the new ones)
- [pass] qom-test: 79/79 machines, including /arm.exe/qom/pic32cm-jh01-cpro. Needs QTEST_QEMU_ARGS='-L C:/Users/C19257/Desktop/Code/pp-qemu/pc-bios': without it the run stops at mori-bmc ('Could not find ROM image npcm7xx_bootrom.bin'), because pc-bios is not installed into the Windows build tree (symlinks fail). This happens regardless of my change
- [pass] test-hmp: 79/79, including /arm.exe/hmp/pic32cm-jh01-cpro; same -L note as qom-test
- [pass] board rst validation: Docs are disabled in this build, so I parsed the rst with docutils 0.23 (code-block mapped to code, warnings treated as errors); no messages
- [pass] generator pytest (compatibility with the real mchp_regfile.h): py -m pytest tests/generator -q: 82 passed

## Deviations
- mchp_regfile.h adds the following after the verbatim §5.1 block, which is kept unchanged and so exceeds 80 columns: mchp_regfile_lookup(); the inline helpers mchp_regfile_elem_offset/get/set; VMSTATE_MCHP_REGFILE(field, state); and #includes of exec/hwaddr.h, exec/memattrs.h and migration/vmstate.h.
- Regfile logging details beyond §5.1:
- Zero bytes written to reserved offsets next to a register are not logged.
- Reserved reads are logged only when no byte of the access hits a register, and never for debug accesses.
- An alias write calls post_write when either the alias or its target has HOOK_POST_WRITE.
- Dedicated models subclass mchp-regblock and fill hooks in the shared MchpRegBlockClass (realize, pre_read, post_write, is_enabled, irq_pending, reset_hold, reset_exit, log_writes). They do not chain Resettable parent phases.
- Generic-block heuristics, documented in mchp_regblock.h:
- SWRST is bit 0 of a CTRLA/CTRL register at offset 0 whose bit 0 is rdz; writing it resets the whole block.
- Enable protection uses bit 1 of that register.
- LOG_UNIMP is emitted for every write, except to INTENSET/INTENCLR and to registers with no writable bits.
- The system models live in two files, hw/misc/mchp_clock.c and hw/misc/mchp_sysctrl.c, with the header include/hw/misc/mchp_sysctrl.h. They are compiled under CONFIG_PIC32CM_JH_SOC (as hw/misc does for NPCM7XX), so there is no new Kconfig symbol. MCHP_SERCOM and MCHP_PORT are left to part B.
- The spec says init-svtor 0. I set armv7m 'init-nsvtor'=0 instead, because cortex-m0 has no Security Extension and armv7m only forwards init-svtor to CPUs that have one. Both default to 0.
- CPUID: set through the board's mc->compat_props global {cortex-m0-arm-cpu, midr, 0x410CC601}. The value is PIC32CM_JH_CPUID in the SoC header.
- SUPC: STATUS.BVDDSRDY always reads 1, because synchronization is instant; Harmony waits on it even when disabling the BOD. At reset, INTFLAG is set to the STATUS ready bits.
- RSTC: in addition to POR, SYST and WDT, any reset that no source announced (for example monitor system_reset) reports EXT (0x10). Reset requests use the named GPIO array 'reset-cause'[8], where line n = RCAUSE bit n. The WDT is wired to line 5 and SYSRESETREQ to line 6.
- WDT: a CLEAR value other than 0xA5 while the watchdog is enabled resets the system with RCAUSE=WDT (DS 22.6.8), even though WDT time-out itself is out of scope. ALWAYSON makes CTRLA read-only. CONFIG and EWCTRL are not locked, because the generated table gives them no hook.
- PAC follows DS 12.5.2.6. Double set, double clear, clearing a locked peripheral, SETLCK on a protected peripheral, a bad key, an unknown PERID, and a non-word WRCTRL write all set INTFLAGA.PAC. NVMCTRL: SPRM/CPRM set and clear STATUS.PRM, PBC/INVALL are no-ops, a missing CMDEX key sets PROGE and ERROR and drops the write, and the remaining commands are LOG_UNIMP.
- Reset-scope simplifications:
- XOSC32K and OSC32K drive their 32 kHz outputs only with EN32K set (DS 16.6.2/16.6.4).
- GCLK ignores the DIV high byte for every generator except generator 1 (DS Table 14-3).
- WRTLOCK is enforced on OSC32KCTRL XOSC32K/OSC32K and GCLK PCHCTRL, but any reset clears it (silicon keeps it until POR).
- Board: LED0 and SW0 are not added in part A, since §6.6 assigns them to part B. Serial wiring is pic32cm_jh_soc_connect_serial() plus the 'chardev' property of the SERCOM model. Until a SERCOM model with that property exists, the SoC logs LOG_UNIMP 'sercomN: no UART model; serial port not connected'.
- Installed docutils 0.23 with 'py -m pip install --user docutils' to validate the rst.

## Open issues
- MPU: MPU_TYPE reads 0 on QEMU cortex-m0, and CONTROL.nPRIV is not implemented. Firmware that needs the MPU (Zephyr CONFIG_ARM_MPU on this board, whose z_arm_mpu_init checks MPU_TYPE) needs a target/arm change, e.g. a cortex-m0plus CPU type with Armv6-M PMSA regions. target/arm is not in my paths.
- As expected for part A, hello.elf hangs at the SERCOM4 INTFLAG.DRE poll (console_putc, 0x480). Part B's mchp-sercom fixes this.
- Calibration row mismatch with the firmware README. The generated NVM image has 0xFFFFFF00_1103F924; the README transcript expects 0x0000010011040249.
- Both give CAL48M 0x00200220.
- They differ in OSC32K CAL (0x3F vs 0x40) and ADC bias.
- The README marks the calrow line as variable (regex), so an e2e golden test must use the regex, or the generator owner should align the values.
- DSU CRC32 and chip erase are LOG_UNIMP only, and STATUSA.DONE is never set, so firmware polling DONE (Harmony DSU_CRCCalculate) will hang.
- qom-test and test-hmp need QTEST_QEMU_ARGS=-L <pp-qemu>/pc-bios on this Windows build, because pc-bios is not installed into build/qemu-bundle (the symlinks fail). This is an environment issue.
- With -d unimp, generic blocks log every non-bookkeeping write, which can be noisy for firmware that polls by writing. Flattened PORT names ('port.GROUP[1].PINCFG[10]') are grouped by tools/pp/unimp.py under register 'GROUP[1]'. PORT becomes a dedicated model in part B.
- Not modelled (spec §7): WDT time-out, PAC enforcement, flash programming, sleep modes, and every other peripheral (DMAC, EIC, TC/TCC, RTC, ADC, DAC, AC, CAN, ...).

## Commands
- Incremental build (ninja -k 0 qemu-system-arm.exe; prints only compiler diagnostics): `$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/build.sh`
- Build and run qtests (default pic32cm-jh-test; pass names for others, e.g. microbit-test stm32l4x5_rcc-test qom-test test-hmp): `$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/qtest.sh pic32cm-jh-test microbit-test stm32l4x5_exti-test stm32l4x5_syscfg-test stm32l4x5_rcc-test stm32l4x5_gpio-test stm32l4x5_usart-test`
- Run hello.elf for 10 s with -d unimp,guest_errors (log in scratch run10.log): `$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/run10.sh`
- gdb-multiarch batch verification against -s -S (break at the DRE poll, register dump, info qtree, info mtree -f; output in gdbcheck.out): `$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/gdbcheck.sh`
- Raw run of the machine (from pp-qemu/build in a UCRT64 shell): `./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel <path>/hello.elf -display none -serial stdio -d unimp,guest_errors -trace 'mchp_clk_update'`
- Validate the board rst with docutils (docs are disabled in this build): `py C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/rstcheck.py C:/Users/C19257/Desktop/Code/pp-qemu/docs/system/arm/pic32cm-jh01-cpro.rst`

## Files
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/misc/mchp_regfile.h: §5.1 contract verbatim, plus: mchp_regfile_lookup(), the inline helpers mchp_regfile_elem_offset/get/set, and VMSTATE_MCHP_REGFILE()
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/mchp_regfile.c: Byte-lane dispatcher. Handles split reads and writes, aliases, W1C, rdz, enable protection, pre_read with debug-safe side effects, and post_write. Table sanity asserts. Traces mchp_regfile_read/write
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/misc/mchp_regblock.h: TYPE_MCHP_REGBLOCK ('mchp-regblock') state and class. Class hooks: realize, pre_read, post_write, is_enabled, irq_pending, reset_hold, reset_exit, log_writes. Also mchp_regblock_set_info(), mchp_regblock_update_irq(), mchp_regblock_set_clock() and vmstate_mchp_regblock
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/mchp_regblock.c: Generic peripheral built from one MchpInstanceInfo. MMIO 0 = the slot, MMIO 1 = the alias view. IRQ = INTFLAG & INTENSET. Clock input 'gclk'. Register views follow the mode selector; SWRST (CTRLA/CTRL bit 0) resets the block. Fuse resets are read via rom_ptr_for_as first. LOG_UNIMP plus trace mchp_regblock_unimp_write for unmodelled writes; mchp_clk_update trace
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/misc/mchp_sysctrl.h: Type names and wiring names for the system controllers: clock ports, the 'reset-cause' and 'system-reset' GPIOs, and the DSU 'revision' property
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/mchp_clock.c: mchp-oscctrl, mchp-osc32kctrl, mchp-gclk and mchp-mclk. Ready bits and their flag edges. QEMU Clock tree XOSC/OSC48M/FDPLL96M → GCLK gen[n]/pch[m] → MCLK 'cpu', with a guard against clock loops; WRTLOCK; CPUDIV validation; CKRDY
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/mchp_sysctrl.c: mchp-nvmctrl, mchp-supc, mchp-rstc, mchp-wdt, mchp-pm, mchp-pac, mchp-dsu and mchp-divas
- C:/Users/C19257/Desktop/Code/pp-qemu/include/hw/arm/pic32cm_jh_soc.h: Pic32cmJhSocState, TYPE_PIC32CM5164JH01100_SOC ('pic32cm5164jh01100-soc'), PIC32CM_JH_CPUID and pic32cm_jh_soc_connect_serial()
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/arm/pic32cm_jh_soc.c: Table-driven SoC. Factory pic32cm_jh_models[] (module → QOM type). Memories (flash and dataflash 0xFF, NVM rows from the generated fill, SRAM), HPB backgrounds, clock wiring, armv7m, OR gates, peripherals, reset wiring. Properties silicon-rev and serial-number
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/arm/pic32cm_jh01_cpro.c: Machine pic32cm-jh01-cpro: 32 MHz/32.768 kHz crystal clocks, serial_hd(0..4) → SERCOM4/2/3/6/0, armv7m_load_kernel, and a compat_props midr=0x410CC601 on cortex-m0
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/arm/Kconfig: PIC32CM_JH01_CPRO (default y) and PIC32CM_JH_SOC (selects ARM_V7M, MCHP_REGFILE, OR_IRQ, UNIMP)
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/arm/meson.build: pic32cm_jh_soc.c and pic32cm_jh01_cpro.c
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/Kconfig: config MCHP_REGFILE
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/meson.build: Regfile and regblock under CONFIG_MCHP_REGFILE; mchp_clock.c and mchp_sysctrl.c under CONFIG_PIC32CM_JH_SOC
- C:/Users/C19257/Desktop/Code/pp-qemu/hw/misc/trace-events: mchp_regfile_read/write, mchp_regblock_unimp_write, mchp_clk_update, mchp_rstc_reset_request
- C:/Users/C19257/Desktop/Code/pp-qemu/docs/system/arm/pic32cm-jh01-cpro.rst: Board documentation: supported and missing devices, properties, boot, debugging
- C:/Users/C19257/Desktop/Code/pp-qemu/docs/system/target-arm.rst: Toctree entry, after arm/kzm by title
- C:/Users/C19257/Desktop/Code/pp-qemu/tests/qtest/pic32cm-jh-test.c: 12 qtests: reset values, ready bits, RCAUSE, user-row resets (default and from a HEX image, across 2 resets), regfile semantics, views/SWRST, clock tree, shared IRQ, PAC, DIVAS, SoC properties
- C:/Users/C19257/Desktop/Code/pp-qemu/tests/qtest/meson.build: pic32cm-jh-test registered in qtests_arm under CONFIG_PIC32CM_JH01_CPRO
- C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/progress.md: Progress log, design decisions and working commands
- C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/gdbcheck.out: Evidence from the gdb batch session: registers at reset and at the DRE poll, full info qtree and info mtree -f
