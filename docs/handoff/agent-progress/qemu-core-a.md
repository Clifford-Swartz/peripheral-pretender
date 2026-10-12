# qemu-core-a progress log

## Status
- [x] read spec, qemu-internals, boot-path, generated headers
- [x] include/hw/misc/mchp_regfile.h (spec 5.1 verbatim + lookup/get/set/VMSTATE_MCHP_REGFILE)
- [x] hw/misc/mchp_regfile.c
- [x] include/hw/misc/mchp_regblock.h + hw/misc/mchp_regblock.c (generic block, views, SWRST, fuses, unimp log)
- [x] trace-events / Kconfig / meson (hw/misc, hw/arm)
- [x] system devices (written: hw/misc/mchp_clock.c, mchp_sysctrl.c, include/hw/misc/mchp_sysctrl.h): hw/misc/mchp_oscctrl.c (OSCCTRL+OSC32KCTRL), mchp_gclk.c (GCLK+MCLK),
      mchp_nvmctrl.c, mchp_sysctrl.c (SUPC, RSTC, WDT, PM), mchp_pac.c, mchp_dsu.c, mchp_divas.c;
      header include/hw/misc/mchp_sysctrl.h (TYPE names + port names)
- [x] SoC hw/arm/pic32cm_jh_soc.c + include/hw/arm/pic32cm_jh_soc.h; board hw/arm/pic32cm_jh01_cpro.c
- [ ] build, gdb verification (48 MHz, DRE poll), MPU_TYPE, CPUID, SYSRESETREQ
- [ ] qtest tests/qtest/pic32cm-jh-test.c; rerun microbit + stm32l4x5 qtests
- [ ] docs/system/arm/pic32cm-jh01-cpro.rst + toctree

## Design decisions
- children created in SoC instance_init via factory table module->type (default mchp-regblock)
- serial: pic32cm_jh_soc_set_serial() stores Chardev; realize sets "chardev" prop on SERCOM child if it has one (part B)
- CPUID via board compat_props global cortex-m0-arm-cpu.midr=0x410cc601
- MPU: expected NOT to work (pmsav7-dregion only exists for V7 cores) -> verify + document
- RSTC: gpio-in "reset-cause"[8], line n = RCAUSE bit; SYSRESETREQ->6, WDT->5; latch survives reset; EXT after

## Commands that work
- build: PowerShell: $env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/.../scratchpad/qemu-core-a/build.sh   (log in build.log; first build after meson change ~3 min)
- first full build OK at milestone "SoC+board written" (no warnings in our files)
- [x] MILESTONE: hello.elf runs, no abort; gdbcheck.sh (gdb-multiarch batch vs -s -S) shows break at DRE poll 0x480,
      GENCTRL0=0x00010106, PCHCTRL22=0x40, OSC48MDIV=0, cpuclk 48 MHz (info qtree), trace 4 MHz->48 MHz,
      CPUID=0x410cc601 (compat_props works), MPU_TYPE=0 (mpu-ns-regions ignored on cortex-m0: CONFIRMED), RCAUSE=1 at reset
- fixed: OR-gate DEVICE() cast before nsources check segfaulted
- next: qtest tests/qtest/pic32cm-jh-test.c; SYSRESETREQ test; docs rst; rerun microbit/stm32l4x5 qtests
- [x] MILESTONE: tests/qtest/pic32cm-jh-test.c 12/12 pass (qtest.sh); registered in tests/qtest/meson.build
- next: regressions (microbit, stm32l4x5 x5, qom-test, device-introspect-test, test-hmp), docs rst, code review pass
- [x] regressions: microbit 6/6, stm32l4x5 exti 9/9 syscfg 10/10 rcc 5/5 gpio 14/14 usart 7/7, device-introspect 6/6,
      qom-test 79/79 (needs QTEST_QEMU_ARGS=-L <src>/pc-bios: npcm bootrom not installed on Windows)
- [x] docs/system/arm/pic32cm-jh01-cpro.rst + toctree (after arm/kzm)
- pending: test-hmp result; rebuild after edits (soc init tidy, line wraps, user-row test 2nd reset); final qtest rerun
- [x] test-hmp 79/79; final rebuild OK; pic32cm-jh-test 12/12 (incl. 2nd-reset fuse check); run10.sh: 10 s no abort;
      gdbcheck.sh re-run OK on final binary. RST validated with docutils (rstcheck.py; docutils installed --user)
- remaining: final review + StructuredOutput report
- [x] DONE. Final tree: build clean, pic32cm-jh-test 12/12, microbit 6/6, stm32l4x5 45/45, device-introspect 6/6,
      qom-test 79/79, test-hmp 79/79 (last two with QTEST_QEMU_ARGS=-L <src>/pc-bios).
- scripts: build.sh, qtest.sh [tests...], run10.sh, gdbcheck.sh (uses gdbcheck.gdb from mkgdb.py), rstcheck.py
