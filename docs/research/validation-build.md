# validation-build: validating a QEMU peripheral twin of PIC32CM JH (Cortex-M0+) against silicon, its fidelity limits, building/shipping from Windows, and upstreaming

## Summary
Treat the DFP as a set of hypotheses and the chip as the source of truth. The local PIC32CM5164JH01100 ATDF (DFP 1.3.221) has enough metadata to generate checks for 1,487 register instances: initval on 506 of 553 definitions, set/clear and W1C atomic-op attributes on 102, WSYNC/RWSYNC on 95, access-size on 75, and per-mode views on 106. However, the ATDF, EDC and SVD files contain provable errors (SCB.CCR 0x204 against bitfields that imply 0x208, a DID mismatch on PIC32CM3204JH00032, TCC PER width). They also do not encode which bits inside RW registers are read-only, and they leave out values loaded from fuses or calibration, so golden values have to be read from a Curiosity Pro (EV81X90A) using the DFP's own reset-and-halt sequence. QEMU's qtest works on Windows hosts and is the natural runner for the generated tests. Differential testing should drive QEMU's gdbstub and pyOCD's gdbserver with the same GDB/MI script. It should compare UART output, GPIO activity and register state by event order and value, not by absolute time, and use -d unimp/guest_errors, the memory_region_ops_* trace events and the hwprofile/execlog plugins to find behavior the model is missing. Snapshot side effects are a trap. The gdbstub and HMP 'x' set attrs.debug=1 on Arm, but 'xp' and pmemsave use MEMTXATTRS_UNSPECIFIED, so reads trigger MMIO side effects. Halting QEMU also freezes all virtual time, while silicon peripherals keep running or stop according to each one's DBGCTRL.DBGRUN. For hardware access, pyOCD (EDBG CMSIS-DAP plus the CMSIS pack) and J-Link support the part and are the flash/debug runners on Zephyr's new pic32cm_jh01_cpro board (MDB/MPLAB IPE are the Microchip-tool alternatives). OpenOCD's at91samd driver does not recognize DID series 6. Avatar2 is stale (QEMU 6.2 fork, Linux-only, ARMv7-M-only IRQ forwarding), so forwarding MMIO to a real chip means building a custom proxy device on top of pyOCD. QEMU is instruction-accurate, not cycle-accurate, and its cortex-m0 model differs from an M0+: CPUID 0x410CC200, MPU only if mpu-ns-regions is set, SCR treated as a bad offset (RAZ/WI), SLEEPDEEP/SLEEPONEXIT ignored. Analog behavior, wait states, DMA timing and power can only be injected or approximated. For Windows, QEMU's docs still point to MSYS2 MINGW64, but MSYS2 deprecated MINGW64 in March 2026 and upstream CI only cross-compiles Windows system emulators in a Fedora container. This workstation already has MSYS2 UCRT64 with most dependencies, but QEMU's configure step needs Developer Mode or the symlink privilege, which this account lacks, and WSL is not installed. Upstream still accepts new MCU boards (MAX78000 in 2025, AST1040 in 2026) but marks them orphaned when nobody maintains them. Upstream rejects code believed to be AI-generated, and checkpatch rejects Apache-2.0 SPDX headers. Microchip's current QEMU footprint is PolarFire SoC, co-maintained since May 2026 by Conor Dooley of Microchip.

## Findings

1. (high) [LOAD-BEARING] [Test generation from ATDF (scope)] The PIC32CM5164JH01100 ATDF has enough machine-readable metadata to generate most of a register reset/access suite: 37 module definitions, 64 register-group mappings, 553 register definitions expanding to 1,487 register instances. RW/R/W = 424/118/11. initval is present on 506 (57 non-zero). atomic-op set/clear/W1C attributes appear on 102 registers, access=WSYNC/RWSYNC (SYNCBUSY-synchronized) on 74/21, access-size=4 on 75, and mode-dependent views on 106 (12 mode groups). Per-instance params such as TCC SIZE are also present.
   detail: A generator can emit, for each register instance and permitted access width: (1) a reset read compared to initval; (2) an implemented-bits check (from EDC impl masks); (3) set/clear alias semantics (INTENSET/INTENCLR, OUTSET/OUTCLR); (4) W1C on INTFLAG; (5) SYNCBUSY polling after writes to WSYNC registers; (6) per-mode views by programming CTRLA.MODE first. The same test specification should also be executable against silicon (see the next findings).
   - ev: Profiled with C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-validation-build/atdf_profile.py and atdf_details.py against PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf (DFP 1.3.221)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1633 (AC CTRLA access='WSYNC'), :1659 atomic-op='clear:INTENCLR', :1667 atomic-op='set:INTENSET', :1675 atomic-op='clear:INTFLAG', :6574 CPUID access-size='4'
   - ev: Mode groups: RTC MODE0/1/2; SERCOM I2CM/I2CS/SPIS/SPIM/USART_EXT/USART_INT; TC COUNT8/16/32. Per module: SERCOM has 66 regs, all mode-dependent, 16 synchronized; TC 15 synchronized; RTC 13 synchronized, 29 mode-dependent
   - ev: TCC instance params SIZE=24/24/16 for TCC0/TCC1/TCC2 (instances at PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1305, :1354, :1387)

2. (high) [LOAD-BEARING] [DFP data is not ground truth] Several DFP reset values are internally inconsistent or depend on the instance, so expectations generated from the DFP must be checked against silicon before they become golden. SCB.CCR is 0x204 in ATDF, SVD and EDC, but its own bitfields (UNALIGN_TRP=0x8, STKALIGN=0x200) and the EDC impl mask (0x208) imply 0x208, which is also what QEMU's ARMv6-M model resets to. PIC32CM3204JH00032's ATDF signature DSU_DID=0x1106001D disagrees with its DSU.DID initval 0x1100050D. TCC PER/PERBUF use a module-level 0xFFFFFFFF even though the counters are 24/24/16-bit. Seven registers have initval bits outside any documented bitfield (for example PAC.STATUSC bit 24). 47 registers have no initval. The local DFP (1.3.221) is four releases behind 1.7.296, and those releases changed the ATDF and SVD files.
   detail: It is not verified here which CCR value silicon actually returns. The point is that a generated test must be arbitrated by a silicon readback, and the generator should pin the DFP version and diff its outputs whenever the DFP is upgraded. Note that a test derived from the same data as the model cannot catch errors in that data.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6606-6608 (CCR initval 0x00000204; UNALIGN_TRP mask 0x8; STKALIGN mask 0x200)
   - ev: PIC32CM-JH01/svd/PIC32CM5164JH01100.svd:24997-25002 (CCR resetValue 0x00000204); PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:15388 (CCR impl 0x208, por bits 0x204)
   - ev: target/arm/cpu.c:530-540 (QEMU: STKALIGN plus, for non-M_MAIN cores, UNALIGN_TRP, giving 0x208)
   - ev: PIC32CM-JH00/atdf/PIC32CM3204JH00032.atdf:1055 (DSU_DID 0x1106001D) vs :1841 (DID initval 0x1100050D); its DSU_DID__SERIES value-group at :1916-1919 lists only 0/1
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5132 PAC.STATUSC initval 0x09000000 with no bitfield at 0x1000000; also MCLK.AHBMASK bit 11, TCC.PER/PERBUF bits 31:24, SystemControl.CCR bit 2 (computed with the work-validation-build check)
   - ev: Registers without initval include RSTC.RCAUSE (:5477), OSC32KCTRL.OSCULP32K (:4798), MPU, MTB, DMAC/ICM descriptors, SysTick RVR/CVR
   - ev: Microchip.PIC32CM-JH_DFP.pdsc:11 (1.3.221, 2025-01-17); https://www.keil.arm.com/packs/pic32cm-jh_dfp-microchip/versions/ (latest 1.7.296, 2026-06-08: 'Updated ATDF files...'; 1.6.279: 'Updated SVD files for improved CMSIS compliance')

3. (medium) [LOAD-BEARING] [Writable masks, side effects and fuse-loaded values need silicon probing] Neither the ATDF nor the EDC encodes read-only bits inside RW registers, clear-on-read, or FIFO read side effects. Some reset values are loaded from the NVM user row or calibration area (WDT and BODVDD configuration; OSCCTRL.CAL48M is likely calibration-loaded). Probing on silicon also has to enable APB clocks first: MCLK.APBCMASK resets to 0x08000000, so SERCOM, TC, ADC and others are clock-gated. It also has to respect PAC write protection, since PAC.STATUSB/STATUSC are non-zero at reset.
   detail: Recommended procedure: enable the MCLK APB mask and the GCLK channel, then write all-ones and all-zeros patterns per register and read back to learn the writable mask. Use an exclusion list (SWRST/ENABLE, WDT, NVMCTRL CMD, PAC WRCTRL, oscillator/GCLK source switches, DSU) and parameterize expected values by the user-row contents. Whether reading a clock-gated or PAC-protected peripheral on silicon returns 0 or a bus error is not verified here; measure it, because the twin must reproduce it.
   - ev: ATDF bitfield attributes across the whole file are only name/caption/mask/values/modes (atdf_profile.py output); no per-field access attribute
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC: SERCOM0 INTFLAG access 'n-----nn' impl 0x83 (W1C not encoded); SERCOM0 CTRLA impl 0x7BF1009F vs 0x4BB1009F per mode; peripheral SFRs use one access kind per register (954 n, 156 r, 21 w); POR differs from MCLR only for core registers R0-R12/SP/MSP/PSP/PC
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:4641-4656 (USER_WORD_0/1: WDT_ENABLE/ALWAYSON/PER/WINDOW/EWOFFSET/WEN, BODVDD fields, NVMCTRL_REGION_LOCKS)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:4931-4935 (CAL48M initval 0x00200220, FCAL/FRANGE/TCAL fields)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:4185 (APBCMASK initval 0x08000000; SERCOM0 bit 0x2, TC0 0x1000, ADC0 0x20000 clear at reset)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5120-5158 (PAC STATUSB initval 0x2 = DSU; STATUSC 0x09000000)

4. (high) [LOAD-BEARING] [qtest as the test runner] QEMU's qtest protocol (read/write at any width, clock_step/clock_set on the virtual clock, irq_intercept_in/out, set_irq_in) is the upstream mechanism for device-model tests. It runs on Windows hosts, and the STM32L4x5 tests are a direct template for generated reset and GPIO tests. There is also a Python qtest client for generator-driven tests.
   detail: qtest runs the machine with -accel qtest, so no guest CPU executes. That makes it right for register semantics, and firmware-level behavior is covered by the differential and functional tests. Upstream's own test admits a deliberate model-vs-board difference, which shows why silicon goldens and an explicit known-deviation list are needed. Whether python/qemu/machine/qtest.py, which uses socketpair fd passing, works on Windows is unverified.
   - ev: system/qtest.c:69-258 (QTest protocol documentation)
   - ev: docs/devel/testing/qtest.rst:25-27 ('Consider adding a new QTest case when you are introducing a new virtual hardware') and :37-54 (register in tests/qtest/meson.build)
   - ev: docs/devel/testing/main.rst:139 ('Both unit tests and qtests can run on POSIX hosts as well as Windows hosts.'); tests/qtest/libqtest.c:382-412 (CreateProcess path on _WIN32)
   - ev: tests/qtest/meson.build:245-250, 268 (qtests_stm32l4x5 enabled by CONFIG_STM32L4X5_SOC)
   - ev: tests/qtest/stm32l4x5_gpio-test.c:177-179 ('Since AF and analog modes aren't implemented, IDR reset values aren't the same as with a real board.'), :209-219, :575 (qtest_start('-machine b-l475e-iot01a'))
   - ev: python/qemu/machine/qtest.py:35-163 QEMUQtestProtocol/QEMUQtestMachine (python/ is not in the sparse checkout; read via https://github.com/qemu/qemu at the same commit 602e4f2da1)

5. (high) [LOAD-BEARING] [Hardware snapshot paths for PIC32CM JH] pyOCD, J-Link and MDB/MDBCore can all read registers on the Curiosity Pro, and pyOCD and J-Link are the runners Zephyr uses for this board. pyOCD gets the target from the CMSIS-Pack index ('pyocd pack install pic32cm', --target=pic32cm5164jh01100) and can also use an unzipped DFP directory via --pack. MDB command files and the MDBCore Groovy API provide scripted reads on Microchip tools (EDBG, and SIM for MPLAB SIM). OpenOCD's at91samd flash driver only knows SAM C series 0/1, so PIC32CM JH (DID series 6) is not recognized. The DFP's J-Link and DAP .py files are scripts hosted by MPLAB X, not standalone tools.
   detail: Not verified: OpenOCD's at91samdXX.cfg can probably still halt the core and read memory over CMSIS-DAP, with only flash probing failing (the error would be 'Couldn't find part corresponding to DID'). MDB/MDBCore syntax is taken from a Microchip MASTERs lab repo for an ATSAMD21. The official MDB guide on onlinedocs.microchip.com returned HTTP 503/403 during this research.
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/board.cmake:4-6 (pyocd --target=pic32cm5164jh01100; jlink --device=PIC32CM5164JH; mplab_ipe --tool EDBG)
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic32c/pic32cm_jh01_cpro/doc/index.html ('pyocd pack install pic32cm'; pyocd is the default runner via the on-board EDBG)
   - ev: pyOCD pyocd/target/pack/cmsis_pack.py:131-171 (accepts 'the root directory of an expanded pack'); pyocd/core/options.py:102-104 ('pack' option); docs/debug_probes.md:146,172-175 (Microchip EDBG = CMSIS-DAP v1); docs/command_reference.md:868-871 (reset halt), :950-954 (read32), :971-974 (savemem) - https://github.com/pyocd/pyOCD (v0.45.1, 2026-07-21)
   - ev: https://www.segger.com/supported-devices/microchip/pic32cm_jh/pic32cm5164jh/ (supported); https://kb.segger.com/J-Link_Commander (Mem32, SaveBin, -CommandFile)
   - ev: https://github.com/stefanluethin-microchip/mu_dev11_mdbcore dev11Labs_mdbScripts/dev11mod3Lab_mdbbat-1b-cmd.txt:29-49,70-77 (device/hwtool EDBG/program/break/reset/run/wait/halt/print/write /p); dev11mod4Lab_mdbcs-scr.groovy:87-139 (Debugger(dev, tool, true), connect, program, run, halt, getSymbolAddress, readFileRegisters, writeFileRegisters)
   - ev: OpenOCD src/flash/nor/at91samd.c (master, head 2026-10-04; file last changed 2024-10-05): SAMD_FAMILY_C families cover only SAMD_SERIES_20/21 (samc20_parts/samc21_parts); GitHub code search 'pic32cm repo:openocd-org/openocd' = 0 hits; tcl/target/at91samdXX.cfg:40-46 (at91samd dsu_reset_deassert), :80 (flash bank at91samd)
   - ev: scripts/j-link_cortex-m0plus.py:1-23 (autogenerated; DSU 0x41002100/0x41002101; NVMCTRL 0x41004000), :93-117 (uses ide.addBooleanProperty, settings.getNumeric: host API provided by MPLAB X)

6. (high) [Capturing true post-reset state on silicon] The DFP already contains the reset-and-halt sequence needed to snapshot registers before any instruction runs: halt, set DEMCR=0x01000001 (TRCENA|VC_CORERESET), write AIRCR=0x05FA0004 (SYSRESETREQ), wait for S_RESET_ST, re-halt and clear DSU STATUSA.CRSTEXT (write 2 to 0x41002101). There is also a pin-level reset_extension (RESET and SWCLK held low). A pyOCD or J-Link harness can replicate this.
   detail: A SYSRESETREQ reset and a power-on reset can leave different state (RSTC.RCAUSE, retained domains). Record the reset type alongside each golden snapshot, and model reset sources explicitly in the twin (see the reset/power finding).
   - ev: scripts/dap_cortex-m0plus.py:462-485 (reset_and_halt)
   - ev: scripts/dap_cortex-m0plus.py:861-886 (reset_extension toggling RESET/SWCLK via dev.Pins)
   - ev: scripts/dap_cortex-m0plus.py:349-350 (dsu_statusa = 0x41002101, dsu_ctrl = 0x41002100)

7. (high) [LOAD-BEARING] [QEMU-side snapshot paths and MMIO side effects] QEMU exposes state through the gdbstub (-s -S), HMP x/xp/'info mtree' (usable from QMP via human-monitor-command), and QMP pmemsave. Only the gdbstub and HMP 'x' path (cpu_memory_rw_debug) marks accesses as debug (attrs.debug=1 on Arm). HMP 'xp' and pmemsave use MEMTXATTRS_UNSPECIFIED, so device read callbacks and their side effects (FIFO pops, clear-on-read) run.
   detail: Recommendations: implement twin MemoryRegionOps with read_with_attrs and suppress side effects when attrs.debug is set (the NVIC already uses read_with_attrs/write_with_attrs, hw/intc/armv7m_nvic.c:2537-2538). Take snapshots through the gdbstub or 'x', not 'xp' or pmemsave. Exclude side-effect registers such as DATA from silicon-vs-QEMU diffs, because silicon debugger reads are generally not side-effect-free (an architectural expectation; verify per peripheral). Also parse 'info mtree -f' against ATDF register-group offsets as a cheap memory-map conformance test.
   - ev: docs/system/gdb.rst:13-19 (-s listens on TCP 1234; -S waits for gdb)
   - ev: hmp-commands.hx:467-485 (x = virtual, xp = physical memory dump); hmp-commands-info.hx:219-231 (info mtree -f/-d/-o/-D)
   - ev: monitor/hmp-cmds.c:650-663 (xp: address_space_read(..., MEMTXATTRS_UNSPECIFIED, ...); x: cpu_memory_rw_debug) - not in the sparse checkout, read from https://github.com/qemu/qemu at commit 602e4f2da1
   - ev: system/physmem.c:4081-4115 (cpu_memory_rw_debug uses the translation's attrs); target/arm/ptw.c:4113-4119 (result->attrs.debug = 1); include/exec/memattrs.h:47-48
   - ev: system/physmem-qmp-cmds.c:72-107 (qmp_pmemsave -> physical_memory_read); system/physmem.c:3531-3535 (MEMTXATTRS_UNSPECIFIED)
   - ev: docs/devel/testing/functional.rst:119-123 (human-monitor-command from QMP)

8. (high) [Debug-halt behavior differs between QEMU and silicon] When gdb stops the VM, QEMU's virtual clock stops, so every modeled peripheral freezes. On PIC32CM JH, peripherals with DBGCTRL.DBGRUN (AC, ADC, DAC, DMAC, PDEC, RTC, TC, TCC; SERCOM also has a DBGCTRL register) can keep running while the CPU is halted. Breakpoint-snapshot comparisons need DBGRUN=0 on silicon, or a DBGRUN model in the twin.
   - ev: include/qemu/timer.h:28-31 ('The virtual clock only runs during the emulation. It stops when the virtual machine is stopped.')
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf: 9 DBGCTRL registers; DBGRUN bitfields in modules AC, ADC, DAC, DMAC, PDEC, RTC, TC, TCC (e.g. :1698, :1882, :2972)

9. (medium) [LOAD-BEARING] [Differential harness design] A single GDB/MI driver can control both sides with the same ELF, breakpoints and register list: pygdbmi, which QEMU's own functional tests already use, talking to QEMU's gdbstub and to the pyOCD/J-Link GDB servers. UART output can be captured through QEMU chardevs available on Windows (stdio, console, socket, pipe, COM ports; no pty) against the EDBG virtual COM port, and GPIO activity through a logic analyzer against QEMU trace events or qtest irq_intercept_out. The Cortex-M0+ has no ITM/SWO/DWT trace blocks, so silicon cannot stream MMIO accesses non-intrusively.
   detail: Normalize timing: compare event order and values, and use tolerance windows only after calibrating -icount. Without a cycle counter (no DWT CYCCNT in core_cm0plus.h), calibrate instruction throughput on silicon with SysTick or a TC. Silicon-side observability is limited to halting snapshots, instrumented firmware (UART/RTT logs), the MTB branch buffer, or the forwarding approach in the Avatar2 finding.
   - ev: tests/functional/qemu_test/gdb.py:3-24 (pygdbmi GdbController wrapper); pythondeps.toml:49-50 (pygdbmi in functests)
   - ev: chardev/meson.build:1-28 (char-serial/socket/pipe/stdio everywhere; char-console/char-win-stdio/char-win on Windows; char-pty only on non-Windows)
   - ev: C:/Users/C19257/.mchp_packs/ARM/CMSIS/6.1.0/CMSIS/Core/Include/core_cm0plus.h:649-660 (only SCS, SysTick, NVIC, SCB, optional MPU; no ITM/TPI/DWT definitions)
   - ev: system/qtest.c:226-245 (irq_intercept_out emits 'IRQ raise/lower NUM')

10. (medium) [Avatar2-style MMIO forwarding] Avatar2 has a JLinkTarget (pylink-square), an OpenOCDTarget and an SVD mapper. Its last code commit was 2024-10-23, its QEMU endpoint is a fork whose default branch is dev/qemu-6.2, it depends on posix_ipc (Linux), and its interrupt-forwarding plugin targets ARMv7-M. Upstream multi-process QEMU only proxies PCI devices. For an ARMv6-M PIC32CM JH on Windows, the practical route is a custom QEMU MMIO-proxy sysbus device that makes blocking calls to a Python bridge using pyOCD or J-Link. Use it as a lab tool to record golden access traces, not for real-time runs.
   detail: Each forwarded access costs at least one debug-probe USB round trip (not measured here). Timing-driven peripherals such as UART baud and timers keep running in real time on silicon while the emulated CPU crawls, so use forwarding for init and configuration sequences. Record (pc, addr, size, R/W, value) tuples and replay them as qtest checks in CI.
   - ev: https://github.com/avatartwo/avatar2 (Apache-2.0; pushed 2025-03-31; last commit 2024-10-23 'Pin Unicorn version'); setup.py:23 posix_ipc, :31 pylink-square
   - ev: avatar2/targets/: jlink_target.py, openocd_target.py, gdb_target.py, qemu_target.py; avatar2/plugins/arm/armv7m_interrupts.py:5 ('from avatar2.archs import ARMV7M'); avatar2/plugins/arm/svd_mapper.py:10-17
   - ev: https://github.com/avatartwo/avatar-qemu (default branch dev/qemu-6.2; pushed 2025-03-29)
   - ev: docs/system/multi-process.rst:12 ('enabled by default for targets that enable KVM'), :59-66 (x-pci-proxy-dev)

11. (high) [LOAD-BEARING] [Discovering unmodeled behavior with tracing] QEMU already provides what is needed to find gaps. Map every ATDF peripheral instance either to a model or to create_unimplemented_device, then run firmware with -d unimp,guest_errors,invalid_mem (plus -D file). Use --trace 'memory_region_ops_*' for every MMIO access with value and region name. The hwprofile plugin gives per-offset access heatmaps, and execlog with afilter traces chosen registers. A small custom plugin using qemu_plugin_mem_get_value and qemu_plugin_hwaddr_device_name can emit (pc, device, offset, value) traces that diff against hardware logs.
   detail: In CI, fail a demo-app run if new 'unimplemented device' or guest_errors lines appear for peripherals declared as modeled. Upstream's cross-win64 CI build passes --disable-plugins, but the MSYS2 QEMU package ships plugin DLLs. A custom build gets plugins automatically when dlltool is present, and it is present on this workstation.
   - ev: util/log.c:513-517 (unimp, guest_errors), :532-533 (invalid_mem)
   - ev: system/trace-events:19-20 (memory_region_ops_read/write: cpu, mr, addr, value, size, name)
   - ev: docs/devel/tracing.rst:14-25 (quickstart uses memory_region_ops_*), :221-222 (default 'log' backend), :236-251 (log -> stderr; simple -> binary file)
   - ev: hw/misc/unimp.c:21-39 (LOG_UNIMP logging of reads/writes); hw/arm/stm32l4x5_soc.c:362-366 (create_unimplemented_device per peripheral)
   - ev: docs/about/emulation.rst:616-650 (hwprofile track/source/pattern), :657-688 (execlog with ifilter/afilter)
   - ev: include/plugins/qemu-plugin.h:684-690 (qemu_plugin_mem_get_value), :716-742 (hwaddr_is_io/phys_addr/device_name)
   - ev: configure:1032-1036 (Windows plugins need dlltool); plugins/win32_linker.c; meson.build:4536-4543 (qemu_plugin_api.lib shipped on Windows)

12. (high) [LOAD-BEARING] [Firmware corpora for CI] There are two ready-made firmware corpora for this exact board. Microchip's Harmony csp_apps_pic32cm_jh00_jh01 has 38 MPLAB X projects, all for pic32cm_jh01_curiosity_pro.X, across 23 peripheral folders, plus core, bootloader and ClassB repos. Zephyr added the pic32cm_jh01_cpro board in August 2025 (author at microchip.com), declaring adc, clock_control, comparator, counter, dac, dma, flash, gpio, hwinfo, i2c, pinctrl, pwm, reset, rtc, spi, uart and watchdog. The Harmony license limits execution to Microchip devices, which needs legal review before Harmony binaries are run in an emulator or used in public or upstream tests.
   detail: Several Harmony demos exercise behavior that is hard to emulate and should be tagged in the fidelity matrix: SleepWalking (adc_dmac_sleepwalking, ac_sleepwalk_singleshot), ECC/MBIST fault injection (mcramc_ecc_mbist_testing, nvmctrl_ecc_testing), reset cause (rstc_reset_cause) and CAN-FD. For upstream QEMU tests, prefer Apache-2.0 Zephyr samples or a purpose-written test firmware with public source.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01 (pushed 2026-10-08): apps ac, adc, can, ccl, clock, dac, divas, dmac, dsu, eic, evsys, freqm, mcramc, nvmctrl, pm, port, rstc, rtc, sercom (i2c/spi/usart incl. LIN), systick, tc, tcc, wdt; 38 firmware/pic32cm_jh01_curiosity_pro.X projects (git tree listing)
   - ev: Same repo License.md clause 4 ('must be used and execute solely and exclusively on or in combination with a device manufactured by or for Microchip') and clause 5 (no use that would subject the software to Open Source Software license terms)
   - ev: Other Harmony repos: core_apps_pic32cm_jh00_jh01, bootloader_apps_pic32cm_jh00_jh01, classb_pic32cm_jh (GitHub search, org Microchip-MPLAB-Harmony)
   - ev: https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/microchip/pic32c/pic32cm_jh01_cpro (pic32cm_jh01_cpro.yaml supported list; first commit 2025-08-17 by Arunprasath P <arunprasath.p@microchip.com>); drivers/serial/uart_mchp_sercom_g1.c:1-4 (Microchip copyright, Apache-2.0)
   - ev: MPLAB X CI/CD Wizard generates a Debian-based Dockerfile using prjMakefilesGenerator: https://onlinedocs.microchip.com/oxy/GUID-FDED4652-7F52-4069-A2F3-D654D6EF1FD3-en-US-4/GUID-FA5479EF-1B6A-4CD9-BCBC-0FD5CAA4A0E5.html (seen in search results; page not fetched)

13. (high) [Running Zephyr tests on the twin (including on Windows)] Zephyr's QEMU board pattern is small. qemu_cortex_m0 declares 'simulation: qemu' and passes '-cpu cortex-m0 -machine microbit', so a QEMU variant of pic32cm_jh01_cpro would point QEMU_BOARD_FLAGS at the new machine. Twister selects QEMUWinHandler on Windows, so twister-on-QEMU works on this host (QEMU_BIN_PATH is an optional override).
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/qemu/cortex_m0/board.cmake (SUPPORTED_EMU_PLATFORMS qemu; QEMU_CPU_TYPE cortex-m0; -machine microbit) and qemu_cortex_m0.yaml (type: qemu; simulation: - name: qemu)
   - ev: scripts/pylib/twister/twisterlib/testinstance.py:226-233 (QEMUHandler vs QEMUWinHandler when os.name == 'nt'), :264-274 (only QEMU simulators on Windows; QEMU_BIN_PATH validated if set)
   - ev: scripts/pylib/twister/twisterlib/handlers.py class QEMUWinHandler docstring ('Spawns a thread to monitor QEMU output from pipes on Windows OS')

14. (high) [Determinism and record/replay] -icount shift=N,sleep=off gives deterministic virtual time: one instruction every 2^N ns, and sleep=off cannot be combined with shift=auto or align. rr=record/replay plus rrfile adds deterministic replay of serial input and timers, with snapshots requiring an 'orphan' qcow2 drive on diskless machines. Device models must use QEMU_CLOCK_VIRTUAL for any guest-visible timer and support VMState for snapshots. Upstream only tests record/replay on A-profile Arm machines and reverse debugging on aarch64/ppc64/s390x/x86_64, so M-profile behavior must be validated by the team.
   - ev: qemu-options.hx:5333-5381 (-icount shift/align/sleep/rr; 'it does not provide cycle accurate emulation')
   - ev: docs/devel/tcg-icount.rst:10-22 (icount gives a degree of determinism; incompatible with MTTCG)
   - ev: docs/system/replay.rst:32-67, :146-157 (orphan qcow2 for diskless snapshots), :194-237 (reverse-stepi/continue need snapshots)
   - ev: docs/devel/replay.rst:118-149 ('All virtual devices should use virtual clock for timers that change the guest state')
   - ev: tests/functional/arm/test_replay.py:18-60 (virt, cubieboard, vexpress-a9 only); reverse-debug tests exist only in tests/functional/{aarch64,ppc64,s390x,x86_64}/test_reverse_debug.py

15. (medium) [IDE attachment to QEMU's gdbstub] VS Code Cortex-Debug supports a 'qemu' server type (with cpu and machine options) and an 'external' server type (gdbTarget host:port), so it can launch or attach to the twin. No documentation was found showing that MPLAB X IDE or the MPLAB VS Code extension's mplab-core-da debugger can attach to a generic GDB remote target. Zephyr's mplab-core-da page lists only Microchip tools.
   detail: This is absence of evidence, not proof. Since the user works on MPLAB X device support, an internal MPLAB 'QEMU tool' backend (for example, speaking the GDB remote protocol to QEMU) may be a question to raise with the MPLAB debugger team. A plain arm-none-eabi-gdb or gdb-multiarch client works; gdb-multiarch 18.1 is already installed in MSYS2 on this machine.
   - ev: https://github.com/Marus/cortex-debug/blob/master/debug_attributes.md lines 25 (cpu, for QEMU), 37 (gdbTarget for external), 52 (machine, for QEMU), 86 (servertype includes qemu and external)
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/common/mplab-core-da-debugging.html (launch.json keys: type mplab-core-da, program, tool, device; supported tools: PICkit, SNAP, ICD, on-board debuggers; no mention of GDB)
   - ev: Web searches for MPLAB X or MPLAB VS Code plus 'GDB server'/QEMU found no supporting documentation

16. (medium) [MPLAB SIM as a third reference] MPLAB SIM's device map sends PIC32CM JH (and SAM D21/E54) to the 'pic32sim' engine, and the DFP declares a simulator profile. The same MDB command file can therefore run with 'hwtool SIM' and 'hwtool EDBG', giving a three-way comparison (pic32sim, silicon, QEMU) for register-level scripts.
   detail: pic32sim's peripheral coverage for PIC32CM JH is not visible from these files. The user's team owns this knowledge and should confirm whether pic32sim is a useful oracle.
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu:3005-3033 (PIC32CM...JH... -> emulengine 'pic32sim'), :3705-3719 (pic32sim engine: stopwatch available, trace allowed=false, data breakpoints)
   - ev: PIC32CM-JH01/hwtools/sdm/sdmSimulator.xml (profile 'sim': runtimewatch, trace, analyzer, registertrace streams)
   - ev: Microchip MDB guide 'Running a Command File Method' and the Hwtool SIM example (onlinedocs.microchip.com, seen in search results; fetch returned 503)

17. (high) [LOAD-BEARING] [Fidelity: timing] QEMU/TCG is instruction-accurate, not cycle-accurate. icount maps instructions to virtual time in powers of two (2^N ns), and no CPI, flash wait states (NVMCTRL CTRLB.RWS), bus contention or AHB arbitration are modeled. Interrupts are taken at translation-block exits, and device DMA through dma_memory_rw completes synchronously unless the model adds delays. Peripheral timing therefore exists only where a model schedules QEMU_CLOCK_VIRTUAL timers from its modeled clocks.
   detail: Busy-wait delays will diverge from timer-based timing unless -icount shift is calibrated against silicon. For a 48 MHz M0+, shift 4 or 5 (16 or 32 ns per instruction) brackets typical throughput; that is a heuristic and should be measured.
   - ev: docs/devel/tcg-icount.rst:10-15 ('QEMU does not attempt to emulate how long an instruction would take on real hardware')
   - ev: qemu-options.hx:5341-5350 (shift semantics; 'does not provide cycle accurate emulation')
   - ev: docs/devel/tcg.rst:36-50 (cpu_exec_interrupt() re-evaluated when exiting a TB)
   - ev: include/system/dma.h:146-152 (dma_memory_read is a direct dma_memory_rw call)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf: NVMCTRL.CTRLB initval 0x00000080; scripts/j-link_cortex-m0plus.py:74-79 (programming sets 15 wait states)

18. (high) [LOAD-BEARING] [Fidelity: CPU core mismatches (QEMU cortex-m0 vs a real M0+)] QEMU has no Cortex-M0+ CPU type. The cortex-m0 model reports MIDR/CPUID 0x410CC200 (silicon per ATDF: 0x410CC601) and has no MPU regions unless the SoC sets armv7m 'mpu-ns-regions' (QEMU's PMSAv7 state also models the v6-M MPU layout). It treats SCR (0xE000ED10) as a bad offset on non-v7 cores, so reads return 0, writes are ignored and a guest_errors line is logged. It does not implement SLEEPDEEP, and it ignores SLEEPONEXIT; WFI simply halts until there is work. The SoC must also set num-irq=32 and num-prio-bits=2, and the IOBUS (0x60000000) only exists as an alias with no single-cycle timing.
   detail: These differences will appear in register-snapshot diffs (SCB.CPUID, SCB.SCR, MPU.TYPE) and in -d guest_errors output. Fixing them likely needs Arm target or NVIC changes, which upstream would review separately from the SoC model.
   - ev: target/arm/tcg/cpu-v7m.c:45-52 (cortex-m0: ARM_FEATURE_V6|M, midr 0x410cc200; no pmsav7_dregion) vs :79-87 (cortex-m3 sets pmsav7_dregion = 8); no 'cortex-m0plus' string in target/arm or hw/arm
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6574 (CPUID initval 0x410CC601); :51-54 (NUM_IRQ 32, __MPU_PRESENT 1, __NVIC_PRIO_BITS 2)
   - ev: target/arm/cpu.c:1667-1676 ('QEMU's PMSAv7 state also models the Armv6-M MPU register layout'), :2339-2345; hw/arm/armv7m.c:349-355, :565 (mpu-ns-regions), :259-262 (num-irq, num-prio-bits)
   - ev: hw/intc/armv7m_nvic.c:1126-1130 and :1688-1699 (SCR requires ARM_FEATURE_V7; deep sleep RAZ/WI; SLEEPONEXIT ignored); :1582-1584 and :2218-2220 (bad offset: guest_errors, RAZ/WI); :2533 (returns MEMTX_OK, no fault)
   - ev: target/arm/tcg/op_helper.c:366-404 (WFI sets halted)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:656 (PORT_IOBUS 0x60000000), :253 (DIVAS_IOBUS 0x60000200)

19. (high) [Fidelity: analog, touch and CAN] Analog behavior can only be injected. Upstream's npcm7xx ADC exposes per-channel 'adci[n]' and 'vref' QOM properties that its qtest sets through QMP qom-set, and that is a pattern the PIC32CM ADC, DAC, AC and temperature sensor models can copy. PTC has an empty register group in the ATDF, so it can only be a stub without internal IP documentation. There is no Bosch M_CAN model upstream, and host CAN bridging (SocketCAN) is built only on Linux hosts.
   - ev: hw/adc/npcm7xx_adc.c:281-288 (adci[%u] and vref properties); tests/qtest/npcm7xx_adc-test.c:119-146 (qom-set from the test)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5472-5474 (PTC module with an empty register-group) and :756-758 (instance at 0x42005400)
   - ev: net/can/meson.build:1-6 (can_socketcan.c only if host_os == 'linux'); hw/net/can/ contains sja1000, ctucan, flexcan and xlnx models only
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf: CAN.CREL initval 0x32300000, 64 CAN registers

20. (medium) [Fidelity: reset sources, power modes and errata] QEMU's reset framework only has COLD, SNAPSHOT_LOAD, WAKEUP and S390-specific types. The twin must model PIC32CM JH reset sources (POR, BOD, external, WDT, system reset), RSTC.RCAUSE, and fuse and calibration loading itself. Sleep modes (IDLE/STANDBY), RUNSTDBY/ONDEMAND clock behavior and SleepWalking must be modeled explicitly, current draw is not modeled at all, and errata exist only if modeled, for example keyed off DSU.DID.REVISION.
   - ev: docs/devel/reset.rst:32-56; include/hw/core/resettable.h:38-42
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5477 (RSTC.RCAUSE, no initval); EDC POR equals MCLR for every peripheral register (C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221/PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC, scanned)
   - ev: Harmony demos needing these features: pm_wakeup_eic, pm_wakeup_rtc, adc_dmac_sleepwalking, ac_sleepwalk_singleshot, rstc_reset_cause (csp_apps_pic32cm_jh00_jh01)

21. (high) [LOAD-BEARING] [Windows build: QEMU docs vs current MSYS2] QEMU documents building with MinGW, either natively through MSYS2 or cross-compiled from Debian/Fedora. It supports 64-bit Windows only, and the build scripts need symlinks (Developer Mode, SeCreateSymbolicLinkPrivilege, or administrator). build-environment.rst recommends MINGW64, but MSYS2 announced the deprecation of MINGW64 on 2026-03-15 and builds its QEMU 11.1.2 package only for ucrt64, clang64 and clangarm64; mingw-w64-x86_64-qemu returns 404. UCRT64 is therefore the right environment, and QEMU's doc is stale on this point.
   - ev: docs/about/build-platforms.rst:148-168 (MinGW on Linux or MSYS2; os.symlink needs Developer Mode or SeCreateSymbolicLinkPrivilege or admin; 'Only 64-bit Windows is supported')
   - ev: docs/devel/build-environment.rst:102-152 (MSYS2 steps; :109 recommends MINGW64; :117 comment reads 'Replace -ucrt64 by -clangarm64 or -ucrt64', which is garbled)
   - ev: https://www.msys2.org/news/ (2026-03-15 'Deprecating the MINGW64 Environment': no new packages; switch to UCRT64 or CLANG64)
   - ev: https://packages.msys2.org/base/mingw-w64-qemu (11.1.2-1; ucrt64/clang64/clangarm64); https://packages.msys2.org/packages/mingw-w64-x86_64-qemu (HTTP 404)
   - ev: https://www.qemu.org/download/ (still lists the MINGW64 package mingw-w64-x86_64-qemu alongside UCRT64)

22. (high) [LOAD-BEARING] [Windows build: what upstream CI actually tests] Upstream's native Windows CI job (msys2-64bit, MINGW64) builds only tools: --disable-system --enable-tools ... --enable-rust. Windows system emulators and the NSIS installer are produced by cross-compiling in a Fedora 43 container (cross-win64-system, artifacts build/qemu-setup*.exe, with --disable-plugins). Native MSYS2 builds of qemu-system-arm are therefore not exercised upstream.
   - ev: .gitlab-ci.d/windows.yml:14-20 (MSYSTEM: MINGW64; CONFIGURE_ARGS: --disable-system --enable-tools ...), :94-95 (mingw-w64-x86_64-rust, rust-bindgen), :101 (MSYS=winsymlinks:native) - read via https://github.com/qemu/qemu at commit 602e4f2da1
   - ev: .gitlab-ci.d/crossbuilds.yml:92-106 (cross-win64-system; IMAGE fedora-win64-cross; --disable-plugins; artifacts build/qemu-setup*.exe)
   - ev: tests/docker/dockerfiles/fedora-win64-cross.docker:7 (fedora:43), :81-102 (mingw32-nsis, mingw64-gcc, mingw64-glib2, mingw64-pixman, rust-std-static-x86_64-pc-windows-gnu), :121 (ENABLE_RUST=1)
   - ev: docs/devel/testing/main.rst:678 (make docker-test-mingw@fedora-win64-cross); meson.build:4661-4684 (Windows 'installer' target via scripts/nsis.py and qemu.nsi)

23. (medium) [LOAD-BEARING] [Windows build: state of this workstation] MSYS2 is already installed with a UCRT64 toolchain and most of QEMU's minimum dependencies. However, this account lacks the symlink capability QEMU's configure needs (Developer Mode not set, no SeCreateSymbolicLinkPrivilege, not elevated), and WSL and Docker are not installed. A native build would likely stop at the meson post-configure script scripts/symlink-install-tree.py, which raises after printing 'Please enable Developer Mode...'.
   detail: The failure is inferred from the docs and the script; per instructions nothing was built. Practical plan: (1) ask IT to enable Developer Mode or grant SeCreateSymbolicLinkPrivilege; (2) in a UCRT64 shell run pacman -S mingw-w64-ucrt-x86_64-python-wheel and mingw-w64-ucrt-x86_64-nsis (optionally capstone and SDL2); (3) export MSYS=winsymlinks:native, configure with --target-list=arm-softmmu, build, run the arm qtests, then 'make installer'. Use MSYS2's Python, not the Windows 'py' launcher. For builds shipped to colleagues, use a Linux runner with the fedora-win64-cross container.
   - ev: Local read-only checks: C:/msys64 present; C:/msys64/ucrt64/bin/gcc.exe 16.2.0 (Rev4, Built by MSYS2 project); python.exe 3.14.8 with pip/ensurepip/setuptools/venv but no wheel; ninja 1.13.2; pkgconf; /usr/bin bison, flex, make; pacman -Q shows glib2 2.90.1, pixman 0.46.4; libfdt.pc; dlltool.exe; gdb-multiarch 18.1; arm-none-eabi-gcc 16.1.0 with a v6-m multilib and newlib 4.6.0.20260123; no qemu, makensis, capstone, SDL2 or rustc
   - ev: Local: HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock AllowDevelopmentWithoutDevLicense not set; whoami /priv does not list SeCreateSymbolicLinkPrivilege; token not in the Administrators role; wsl.exe: 'The Windows Subsystem for Linux is not installed.'; docker/podman not on PATH
   - ev: scripts/symlink-install-tree.py:28-36 (os.symlink; on Windows prints 'Please enable Developer Mode to support soft link without Administrator permission' then re-raises) - via https://github.com/qemu/qemu at commit 602e4f2da1; meson.build:19 (add_postconf_script)
   - ev: meson.build:1061 (glib >= 2.66.0); configs/targets/arm-softmmu.mak:4 (TARGET_NEED_FDT=y); docs/about/build-platforms.rst:102 (Python >= 3.9); pythondeps.toml:22, :42 (vendored meson 1.12.0; wheel >= 0.41.2)

24. (high) [Rust status in QEMU 11.x] Rust is optional and disabled by default in both the v11.1.0 release and current master. If enabled it needs rustc >= 1.83.0, bindgen >= 0.60 and Meson >= 1.12 (configure fetches the vendored meson-rust group). On Windows the Rust target is x86_64-pc-windows-gnu; MSVC is not supported. The Rust SysBusDevice, memory and timer bindings are marked stable, but only pl011 and hpet exist as Rust devices, so the twin should be written in C.
   - ev: meson_options.txt:388-389 (option('rust', type: 'feature', value: 'disabled')); configure:251 (rust="disabled"), :743-745 (--enable-rust/--disable-rust)
   - ev: v11.1.0 tag: meson_options.txt:384 'disabled' and configure:264 rust="disabled" (via https://github.com/qemu/qemu)
   - ev: meson.build:96-126 (rustc >= 1.83.0 check; bindgen required); docs/about/build-platforms.rst:111-122 ('QEMU requires Rust 1.83.0'; bindgen 0.60.x)
   - ev: configure:1152-1162 ('Rust needs Meson 1.12.0'); pythondeps.toml:25-27 (meson-rust group); configure:1232-1239 ('x86_64-pc-windows-gnu (MSVC not supported)')
   - ev: docs/devel/rust.rst:1 (|msrv| 1.83.0), :9-12 (focus on SysBusDevice in safe Rust), :112-115 (pl011 and hpet sample devices), :142-162 (module status table)

25. (medium) [Prebuilt Windows binaries] qemu.org lists two sources of Windows binaries: Stefan Weil's installers and MSYS2 packages. MSYS2 ucrt64 QEMU 11.1.2-1 (built 2026-09-29) installs TCG plugin DLLs, which makes it useful for prototyping the gdbstub, Cortex-Debug and plugin workflows on stock Cortex-M0 machines such as microbit. Neither source can contain the custom PIC32CM machine. From this network, qemu.weilnetz.de redirected to an OpenDNS URL, which suggests corporate DNS filtering (not verified further). QEMU 11.0.0 was tagged 2026-04-21, 11.1.0 on 2026-08-11, and the latest stable tag is v11.1.2.
   - ev: https://www.qemu.org/download/ (Windows: 'Stefan Weil provides binaries and installers...'; MSYS2 pacman instructions)
   - ev: https://packages.msys2.org/packages/mingw-w64-ucrt-x86_64-qemu (11.1.2-1, build date 2026-09-29)
   - ev: msys2/MINGW-packages mingw-w64-qemu/PKGBUILD:22 (mingw_arch ucrt64, clang64, clangarm64), :151 ('configure fails to create links, which can be ignored'), :255-258 (installs contrib and test plugin DLLs)
   - ev: WebFetch of https://qemu.weilnetz.de/w64/ returned a 302 to a '*.id.opendns.com/s/qemu.weilnetz.de/...' URL (not followed)
   - ev: GitHub API tag dates for qemu/qemu v11.0.0 / v11.1.0; tag list head v11.1.2

26. (medium) [Shipping a custom build (GPLv2; flag for legal review, not legal advice)] The QEMU emulator as a whole is GPLv2, and QEMU's NSIS installer shows COPYING as its license page and bundles MinGW DLLs that carry their own licenses. Per the FSF FAQ, copies made inside one organization are not 'distribution', but releasing a modified version publicly requires providing source. Communication over pipes and sockets is normally between separate programs, while in-process device models and plugins (qemu-plugin.h is GPL-2.0-or-later) are part of QEMU. Separately: the DFP's ATDF/SVD/headers are Apache-2.0, Harmony firmware has the execute-only-on-Microchip-devices clause, and 'QEMU' is a trademark.
   detail: Questions for legal: internal-only distribution versus shipping to customers (for example alongside MPLAB tooling), source-offer mechanics, notices for bundled DLLs, relicensing generated register tables from Apache-2.0 to GPL-2.0-or-later (Microchip owns the copyright), using Harmony binaries inside an emulator, and naming of the product.
   - ev: LICENSE:1-25 (GPLv2 as a whole; 'QEMU is a trademark of Fabrice Bellard')
   - ev: qemu.nsi:88 (MUI_PAGE_LICENSE COPYING), :119-120 (installs COPYING, COPYING.LIB); meson.build:4662-4668 (nsis.py receives the glib bindir to collect DLLs)
   - ev: https://www.gnu.org/licenses/gpl-faq.html#InternalDistribution, #GPLRequireSourcePostedPublic, #MereAggregation
   - ev: include/plugins/qemu-plugin.h:5 (SPDX GPL-2.0-or-later)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:2-18, PIC32CM-JH01/svd/PIC32CM5164JH01100.svd:2-18, PIC32CM-JH01/include/pic32cm5164jh01100.h:1-19 (Apache-2.0)
   - ev: csp_apps_pic32cm_jh00_jh01 License.md clauses 4-5

27. (high) [LOAD-BEARING] [Upstream contribution mechanics] Patches go as inline emails to qemu-devel, CC'ing the maintainers from MAINTAINERS or get_maintainer.pl. Every commit needs a Signed-off-by under the DCO, multi-patch series need a cover letter, and revisions need version tags; the docs encourage including tests. checkpatch.pl makes an SPDX-License-Identifier mandatory on new source files, accepts only GPL-2.0-or-later (preferred), GPL-2.0-only, LGPL-2.1-only/-or-later, BSD-2/3-Clause or MIT (anything else, Apache-2.0 included, is an ERROR), forbids license boilerplate text in new files, and warns when added files lack a MAINTAINERS update.
   - ev: docs/devel/submitting-a-patch.rst:15-26 (minimal checklist), :55-58 (checkpatch), :212-218 (include a testsuite addition), :225-236 (patches to qemu-devel; git send-email), :243-268 (b4), :320-331 (CC maintainers; get_maintainer.pl), :374-380 (Signed-off-by mandatory), :388-394 (cover letter), :532-547 (version tags)
   - ev: docs/devel/code-provenance.rst:9-21 (DCO via Signed-off-by), :83-91 (employer cases)
   - ev: scripts/checkpatch.pl:1377-1403 (preferred and valid license list; ERROR on others), :1504-1523 (new files need SPDX; no boilerplate), :1465-1481 ('does MAINTAINERS need updating?') - via https://github.com/qemu/qemu at commit 602e4f2da1
   - ev: Commit 26bab49db56a (max78000 docs): '[PMM: Moved doc to correct place in index; ...; added SPDX]'

28. (high) [LOAD-BEARING] [Upstream policy on AI-generated code] QEMU declines contributions believed to include or derive from AI-generated content (it names ChatGPT, Claude and Copilot). AI may still be used for researching APIs, static analysis and debugging when its output is not contributed, and the repository's AGENTS.md tells agents not to produce upstream code. Any twin code intended for upstream must be written by people.
   detail: This research falls under the permitted uses. Separate internal-only prototype code from code meant for upstream, and decide early which path the twin is on.
   - ev: docs/devel/code-provenance.rst:288-299 (TL;DR policy), :325-331 (decline if AI use is known or suspected), :336-349 (exceptions are discussed on qemu-devel)
   - ev: AGENTS.md:1-18 (agents permitted only for research, static analysis, debugging, local experiments not intended for upstream, trivial changes)

29. (high) [LOAD-BEARING] [New MCU boards are still accepted; maintenance is the real requirement] Recent precedents: B-L475E-IOT01A/STM32L4x5 was added in January 2024 by students and handed to their supervisor in September 2024 because they lacked 'time and access to the hardware'. MAX78000FTHR was added in July 2025 by an individual and marked Orphan in December 2025. ASPEED engineers added the Cortex-M4 AST1040 EVB in May 2026 and sit in MAINTAINERS. Boards that go unmaintained or lack test images get removed (PXA/OMAP in 9.2, highbank/midway in 11.0, sonorapass-bmc in 11.1). Expected deliverables are qtests, functional tests with SHA-256-pinned downloadable assets, and a docs/system/arm/<board>.rst listing supported and missing devices.
   detail: A silicon vendor upstreaming its own MCU is well placed (ASPEED is the model): it has hardware, datasheets and staff to act as M:. A board without a committed maintainer and public test images risks becoming orphaned and later removed.
   - ev: MAINTAINERS:834-840 (B-L475E-IOT01A: Samuel Tardieu, Maintained), :1091-1102 (STM32L4x5 incl. tests/qtest/stm32l4x5*), :904-910 (MAX78000FTHR: no M:, S: Orphan)
   - ev: Commits 41581f13619d (2024-01-08, Inès Varhol, telecom-paris.fr), e569d9593360 (maintainer handover message), bfb8ab8370c6 (Markus Armbruster: 'MAINTAINERS: Add MAX78000FTHR section as orphan'), 51eb283dd0e2 (2025-07-04, Jackson Donaldson), b56af8352b5e (2026-05-25, Jamin Lin <jamin_lin@aspeedtech.com>, 'for firmware development and validation')
   - ev: hw/arm/aspeed_ast10x0.c:484, :512 (cortex-m4); MAINTAINERS:1265-1271 (ASPEED reviewers @aspeedtech.com)
   - ev: docs/about/removed-features.rst:1144-1156, :1197-1201, :1203-1208
   - ev: docs/system/arm/max78000.rst:1-37; docs/system/arm/b-l475e-iot01a.rst:12-35; tests/functional/arm/test_max78000fthr.py:14-45
   - ev: docs/devel/testing/functional.rst:276-300 (downloading tests go in 'thorough'), :358-367 (QEMU_TEST_ALLOW_UNTRUSTED_CODE for blobs without public source)

30. (high) [Microchip's existing presence in QEMU] MAINTAINERS contains no microchip.com address. The Microchip PolarFire SoC Icicle Kit machine was written by Bin Meng (Wind River) in September 2020. After Bin left the RISC-V maintainers (January 2025), the entry was marked 'Odd Fixes' on 2026-05-13 with Conor Dooley (listed as conor@kernel.org, who acked from conor.dooley@microchip.com) and Sebastian Huber as M:. Conor has 12 QEMU commits under his Microchip address. Other Microchip-family models were written by the community: SmartFusion2 (Microsemi, Cortex-M3, emcraft-sf2) and the AVR ATmega MCUs. Microchip's own virtual-platform investment for PolarFire is Renode, which ships inside SoftConsole.
   - ev: MAINTAINERS:1795-1810 (Microchip PolarFire SoC Icicle Kit: M: Conor Dooley <conor@kernel.org>, M: Sebastian Huber, S: Odd Fixes); hw/riscv/microchip_pfsoc.c:1-8 (Copyright 2020 Wind River, Bin Meng)
   - ev: Commit 1d642d39cfd6 (2026-05-13, 'MAINTAINERS: Mark Microchip PolarFire SoC as Odd Fixes', Acked-by: Conor Dooley <conor.dooley@microchip.com>); commit 4a16a1a7adda (2025-01-28, 'MAINTAINERS: Remove Bin Meng from RISC-V maintainers'); first PolarFire commits 2020-09-01 by Bin Meng <bin.meng@windriver.com>
   - ev: GitHub commit search repo:qemu/qemu author-email:conor.dooley@microchip.com -> 12 commits (e.g. 2022-11-17 'hw/riscv: pfsoc: add missing FICs as unimplemented')
   - ev: MAINTAINERS:1240-1263 (SmartFusion2 and Emcraft M2S-FG484: Subbaraya Sundeep, Peter Maydell); hw/arm/msf2-soc.c:4, :137 (cortex-m3; note docs/system/arm/emcraft-sf2.rst:7 says Cortex-M4); MAINTAINERS:1315-1334 (AVR MCUs: Michael Rolnik); hw/avr/atmega.c:1-4
   - ev: https://antmicro.com/blog/2022/12/microchip-riscv-support-in-renode ('Renode now ships as an integrated part of SoftConsole', 2022-12-29)

31. (medium) [Prior art close to PIC32CM JH] Quarkslab's SSTIC 2024 'Tame the (Q)emu' repository adds an out-of-tree QEMU 8.2 board based on ATSAMD21G18A, with SAMD21 GPIO (PORT) and SERCOM models. SAM D21 shares much of its peripheral family with PIC32CM JH. The repository is explicitly unmaintained, and a separate WIP fork for SAM9X75 exists.
   detail: Use it as a reading reference for QOM and SERCOM/PORT structure only. Its license status and code quality were not reviewed, and the AI and provenance rules apply if anything is reused upstream.
   - ev: https://github.com/quarkslab/sstic-tame-the-qemu README.rst (files qemu/hw/arm/samd21.c, qemu/hw/gpio/samd21_gpio.c, qemu/hw/misc/samd21_sercom.c; 'will not be maintained'; QEMU 8.2)
   - ev: https://github.com/kmehltretter82/qemu-sam9x75 (WIP SAM9X75 fork, pushed 2026-10-05)

## Table: Validation techniques
| technique | what it catches | tooling | effort | automation in CI? |
| DFP-generated register reset/access tests (qtest) | Missing or misplaced registers; wrong reset values; implemented-bit masks; set/clear alias and W1C semantics; access-width rules; SYNCBUSY handling; per-mode register views | Python generator over ATDF (initval, atomic-op, access, access-size, modes) and EDC (impl masks), emitting C qtests in tests/qtest or Python QEMUQtestMachine tests; -accel qtest | Medium to build the generator; low per peripheral afterwards; ongoing exclusion and known-deviation lists | Yes. No hardware needed, and qtests run on Linux and Windows hosts |
| Silicon golden capture (same access spec on the real chip) | DFP errors (CCR 0x204/0x208, DID mismatch, TCC PER width); fuse- and calibration-loaded values; read-only bits inside RW registers; reserved bits that read 1; behavior of clock-gated and PAC-protected peripherals | pyOCD (EDBG CMSIS-DAP, target pic32cm5164jh01100), J-Link Commander or pylink, MDB/MDBCore; DFP reset_and_halt sequence; enable APB clocks via MCLK first | Medium; needs a safe exclusion list (SWRST, WDT, NVMCTRL CMD, PAC, clock switches) | Yes, on a hardware-in-the-loop runner with an EV81X90A attached; regenerate goldens per DFP and silicon revision |
| Differential firmware runs (same ELF) | Functional divergence: UART bytes, GPIO edge order, register state at breakpoints, ISR ordering, error paths | QEMU -serial file/socket vs EDBG VCOM; logic analyzer vs QEMU trace events or irq_intercept_out; GDB/MI (pygdbmi) against QEMU -s -S and pyOCD/J-Link GDB servers | High (harness plus timing normalization; DBGRUN and side-effect exclusions) | Partly. The QEMU side always runs; the hardware side runs on the HIL runner |
| Avatar-style MMIO forwarding and recording | Unmodeled side effects, required init sequences, real read values for given write sequences (golden traces) | Avatar2 (Linux only, QEMU 6.2 fork, ARMv7-M IRQ forwarding) or a custom QEMU MMIO-proxy sysbus device plus a pyOCD/J-Link Python bridge | High | No (lab tool); recorded traces become qtest replay checks in CI |
| MMIO tracing in QEMU | Accesses to unmodeled peripherals or registers, bad offsets and widths, register-access heatmaps to prioritize modeling | -d unimp,guest_errors,invalid_mem -D file; create_unimplemented_device per ATDF instance; --trace memory_region_ops_*; plugins hwprofile (pattern), execlog (afilter), custom plugin with qemu_plugin_mem_get_value | Low | Yes. Fail when a new unimp or guest_errors line appears for a peripheral declared as modeled |
| Firmware suites | End-to-end regressions in PLIBs and drivers | Harmony csp_apps_pic32cm_jh00_jh01 (38 Curiosity Pro projects) built with the MPLAB X CI/CD Dockerfile and prjMakefilesGenerator; Zephyr pic32cm_jh01_cpro tests through a QEMU board variant and twister (QEMUWinHandler on Windows) | Medium | Yes (check the Harmony license before any public use) |
| Determinism and record/replay | Flaky timing-dependent failures; reproducible bug reports; reverse debugging | -icount shift=N,sleep=off; -icount rr=record|replay,rrfile=...; orphan qcow2 for snapshots | Low to medium (all models must use QEMU_CLOCK_VIRTUAL and support VMState) | Yes for icount runs; replay on M-profile needs a validation spike |
| Three-way comparison with MPLAB SIM | Divergences among pic32sim, QEMU and silicon on the same register script | mdb.bat command files with hwtool SIM vs hwtool EDBG; GDB scripts for QEMU | Medium | Yes (SIM and QEMU need no hardware) |
| Memory-map conformance | Wrong peripheral base addresses or sizes, missing IOBUS aliases | HMP 'info mtree -f' through QMP human-monitor-command, diffed against ATDF register-group offsets | Low | Yes |
| Fuzzing device models | Crashes, assertions and hangs from guest-controlled MMIO or DMA | QEMU generic-fuzz (clang/libFuzzer, QEMU_FUZZ_OBJECTS) | Medium (Linux only) | Nightly on Linux |

## Table: Fidelity matrix template
| peripheral | register-accurate? | functional behavior | timing | interrupts/events | DMA | known gaps |
| LEGEND (proposed) | R0 unimplemented-device stub; R1 DFP-derived reset values and masks; R2 = R1 plus silicon-verified goldens | F0 none; F1 documented subset; F2 datasheet-complete for the modeled modes | T0 instantaneous; T1 virtual-time approximation from modeled clocks; T2 calibrated against silicon within a stated tolerance | I0 none; I1 NVIC lines; I2 = I1 plus EVSYS generators and users | D0 none; D1 DMAC triggers wired | Free text; cite datasheet section or errata ID; list each known deviation with a test reference |
| CPU core (QEMU cortex-m0 standing in for M0+) | SCB/NVIC: R1 with deviations (CPUID 0x410CC200 vs 0x410CC601; CCR 0x208 vs DFP 0x204) | Instruction-accurate ARMv6-M Thumb | No cycle timing; icount at 2^N ns per instruction | NVIC: set num-irq=32, num-prio-bits=2 | n/a | No cortex-m0plus type; MPU only if mpu-ns-regions is set; SCR is a bad offset on v6-M (RAZ/WI); SLEEPDEEP and SLEEPONEXIT ignored; no MTB |
| PORT (+ IOBUS alias at 0x60000000) | Target R2 | GPIO in/out, pulls, PMUX routing | T0 | EIC pin interrupts; PORT events | n/a | External pin stimulus needs a bridge (qtest set_irq_in or chardev); drive strength and analog pins not modeled; IOBUS single-cycle timing absent |
| SERCOM USART (USART_INT/EXT) | Target R2 (66 SERCOM registers, all mode-dependent; 16 synchronized) | TX/RX through a chardev (on Windows: stdio, socket, pipe, COM) | T1 character time from BAUD and GCLK, if implemented | DRE/TXC/RXC/RXS/CTSIC/RXBRK/ERROR | RX/TX triggers | LIN, IrDA, RS485 timing, collision detection; DATA read side effects in snapshots |
| SERCOM SPI / I2C | Target R2 | QEMU SSI and I2C buses to modeled slaves | T1 | Mode-specific flags | RX/TX triggers | Clock stretching, arbitration loss, SMBus timeouts, slave-select timing |
| TC (COUNT8/16/32) | Target R2 (23 regs, 15 synchronized, 11 mode-dependent) | Count, compare, capture, waveform output | T1 from GCLK and prescaler | OVF/ERR/MCx; event in/out | MC triggers | Capture inputs need stimulus; DBGRUN differs (QEMU freezes everything when halted) |
| TCC | Target R2 (PER/PERBUF width per instance: 24/24/16-bit) | PWM, dead time, fault, pattern | T1 | Faults and events | Yes | Dithering, fault recovery modes, output matrix |
| RTC (MODE0/1/2) | Target R2 (32 regs, 13 synchronized, 29 mode-dependent) | Counter and calendar | T1 at 32 kHz on QEMU_CLOCK_VIRTUAL | CMP/ALARM/PER/OVF | n/a | Retention across reset domains; host clock vs virtual clock choice affects determinism |
| WDT | Reset values come from the NVM user row (ENABLE, ALWAYSON, PER, WINDOW, EWOFFSET, WEN) | Timeout triggers system reset; early warning | T1 | EW interrupt | n/a | ALWAYSON locking; RCAUSE.WDT |
| EIC / EVSYS | Target R2 | Edge/level detection, filters; channel routing | T0 | EXTINT to NVIC or EVSYS; NMI | n/a | Filter and debounce timing; sync/resync/async path latency |
| DMAC | Descriptors live in SRAM (no initval) | Descriptor fetch and transfers | T0 (dma_memory_rw is synchronous) | TCMPL/TERR/SUSP | n/a | QoS/priority arbitration, beat timing, CRC engine |
| ADC / DAC / AC | Target R2 (ADC: 22 regs, 10 synchronized) | Results from injected inputs (QOM properties, as in npcm7xx ADC adci[n]) | T1 conversion time | RESRDY/WINMON/OVERRUN; COMP/WIN | RESRDY trigger | Calibration from NVM, INL/DNL, noise, internal temperature sensor (injected only) |
| PTC | R0 (empty register group in ATDF) | None | - | - | - | Needs internal IP spec or QTouch behavior; stub |
| CAN (Bosch M_CAN IP, CREL 0x32300000) | Target R1/R2 (64 regs) | Frames on a QEMU CAN bus | T1 bit timing optional | Interrupt lines | n/a | No M_CAN model upstream; host bridge only via SocketCAN on Linux; message RAM in system SRAM |
| NVMCTRL / FUSES / user row | Target R2 | Flash, data flash and user-row emulation backed by a file | T1 program/erase delays optional | READY/ERROR | n/a | Wait states (CTRLB.RWS) not timed; security bit; chip erase via DSU; ECC error injection |
| Clock system (OSCCTRL, OSC32KCTRL, GCLK, MCLK) | CAL48M and OSC32K calibration from NVM; MCLK.APBCMASK resets to 0x08000000 | QEMU Clock tree propagation and gating | T0 startup (T1 if startup times are modeled) | Ready/lock flags | n/a | Oscillator accuracy and jitter, clock-failure detection; FREQM measures model-derived values |
| PM / SUPC / RSTC | RCAUSE has no initval | Sleep modes, reset cause | T0 | BOD/VREG interrupts | n/a | No current or power model; standby and SleepWalking semantics; BOD only by injection |
| PAC / DSU | PAC STATUSB/STATUSC non-zero at reset (including undocumented STATUSC bit 24) | Write protection and error flags; DID and CRC32 | T0 | PAC ERR | n/a | Bus-error behavior on protected writes still to be measured on silicon |
| DIVAS (+ IOBUS alias 0x60000200), CCL, FREQM, PDEC, ICM, MCRAMC, MTB | R0 until needed | - | - | - | - | Start as create_unimplemented_device stubs so that -d unimp exposes use |

## Table: Windows build options
| option | steps | pros | cons | recommended? |
| Prebuilt MSYS2 package (mingw-w64-ucrt-x86_64-qemu 11.1.2) | In a UCRT64 shell: pacman -S mingw-w64-ucrt-x86_64-qemu | Instant; current release; includes TCG plugin DLLs | Stock QEMU only; cannot contain the PIC32CM machine | Only for prototyping workflows (gdbstub, Cortex-Debug, plugins) on stock Cortex-M0 machines such as microbit |
| Stefan Weil installer (qemu.weilnetz.de) | Download and run qemu-w64-setup-*.exe | Listed on qemu.org; NSIS installer | Stock QEMU only; the site was intercepted by OpenDNS from this workstation | No |
| Native MSYS2 UCRT64 build (existing C:/msys64) | IT enables Developer Mode or grants SeCreateSymbolicLinkPrivilege; pacman -S python-wheel and nsis (optionally capstone, SDL2); export MSYS=winsymlinks:native; configure --target-list=arm-softmmu; make; run the arm qtests; make installer | Fast local edit-build-debug loop; most dependencies already installed (gcc 16.2, glib2 2.90, pixman, libfdt, Python 3.14.8, ninja, dlltool, gdb-multiarch, arm-none-eabi-gcc); qtests run on Windows | Blocked today by the missing symlink privilege; upstream CI does not test native system-emulator builds; QEMU docs still say MINGW64 | Yes, for local development once the symlink issue is fixed |
| Native MSYS2 MINGW64 build | As documented in docs/devel/build-environment.rst with -mingw64 | Matches QEMU docs and the upstream msys2-64bit CI job | MSYS2 deprecated MINGW64 on 2026-03-15; no MINGW64 QEMU package; same symlink issue | No |
| WSL2 Linux build | IT installs WSL and a distro; apt build-dep qemu or lcitool deps; build Linux binaries, optionally cross-compile Windows with MinGW | Linux-first tooling: Avatar2 (posix_ipc), SocketCAN bridging, fuzzing, docker tests | WSL not installed and needs admin; Linux binaries are not shippable to Windows users; USB probe passthrough needs extra setup (unverified) | Optional, for Linux-only tools |
| Linux CI cross-compile (upstream method) | Fedora 43 fedora-win64-cross container; configure with --cross-prefix=x86_64-w64-mingw32-; make installer produces qemu-setup-*.exe | Same path upstream uses for Windows installers; reproducible; qtests and functional tests also run on Linux | Needs Linux runner and container infrastructure; Windows-only bugs surface only in smoke tests | Yes, for builds shipped to colleagues |
| Rust-enabled builds (any of the above) | Install rust and bindgen (MSYS2 mingw-w64-*-rust, rust-bindgen); --enable-rust; Meson 1.12 | Allows Rust device models | Disabled by default; rustc >= 1.83; MSVC unsupported; only pl011 and hpet sample devices exist | No; write the models in C |

## Table: DFP metadata usable for test generation (PIC32CM5164JH01100, DFP 1.3.221)
| metadata | value | use in generated tests | gap / caveat |
| Register definitions / instances | 553 definitions; 1,487 count-expanded instances; 64 register-group mappings; 37 modules | One reset and access check per instance and permitted width | Mode-dependent views and per-instance params (TCC SIZE) must be expanded |
| rw attribute | RW 424 / R 118 / W 11 | Read-only checks (writes ignored), write-only read-as checks | Read-only bits inside RW registers are not encoded |
| initval | 506 present (57 non-zero); 47 missing | Reset-value expectations | Errors (CCR), fuse/calibration-loaded values, instance-dependent widths; needs silicon arbitration |
| atomic-op | 102 (e.g. clear:INTFLAG, set:INTENSET, clear:INTENCLR) | W1C and set/clear alias semantics | Clear-on-read and FIFO side effects are not encoded |
| access (WSYNC / RWSYNC) | 74 / 21 | Poll SYNCBUSY after writes; check that the model sets and clears SYNCBUSY | Real synchronization latency (GCLK domain crossing) is untimed in QEMU |
| access-size | 75 registers with 4 | Narrow-access behavior checks | Silicon response to illegal widths must be measured |
| modes | 106 registers; 12 mode groups (RTC 3, SERCOM 6, TC 3) | Program CTRLA.MODE, then test that view | Order constraints (ENABLE=0, SWRST sync) must be scripted |
| EDC .PIC (per mode) | impl masks per mode (e.g. SERCOM0 CTRLA 0x7BF1009F vs 0x4BB1009F); access n/r/w per register; POR/MCLR | Reserved-bit read-as-zero checks | No per-bit RO-in-RW; POR equals MCLR for peripherals; CCR por 0x204 vs impl 0x208 |
| Instance parameters | e.g. TCC0/1 SIZE 24, TCC2 SIZE 16 | Instance-specific widths and reset values | Module-level initval ignores them (PER 0xFFFFFFFF) |
| Known inconsistencies | CCR 0x204; PIC32CM3204JH00032 DID; PAC.STATUSC bit 24; TCC PER width; local DFP 1.3.221 vs latest 1.7.296 | Seed the known-deviation list | Re-check after a DFP upgrade |

## Table: Debug and snapshot access paths
| path | target | PIC32CM JH status | scripting / snapshot commands | caveats |
| QEMU gdbstub | Twin | Built in | -s -S or -gdb tcp::PORT; GDB x/..., info registers; pygdbmi driver | Reads carry attrs.debug=1 on Arm; the VM stop freezes all virtual time |
| QEMU HMP/QMP | Twin | Built in | QMP human-monitor-command: 'x', 'xp', 'info mtree -f'; QMP pmemsave | 'xp' and pmemsave use MEMTXATTRS_UNSPECIFIED, so reads have MMIO side effects |
| pyOCD | Silicon via EDBG (CMSIS-DAP v1) | Supported via CMSIS pack (pyocd pack install pic32cm; target pic32cm5164jh01100); Zephyr's default runner | commander: reset halt, read8/16/32 ADDR LEN, savemem ADDR LEN FILE; Python API; gdbserver | Flash algorithms present only for 2532/5164 parts in DFP 1.3.221 |
| SEGGER J-Link | Silicon via an external J-Link on the Cortex debug header | PIC32CM5164JH listed as supported; Zephyr jlink runner | J-Link Commander -CommandFile: Mem32, SaveBin, R, H; pylink; J-Link GDB Server | Probe licensing and availability |
| MDB / MDBCore | Silicon via Microchip tools (EDBG/PKOB/ICD/PICkit) or MPLAB SIM | Native Microchip toolchain | mdb.bat command file (device, hwtool EDBG|SIM, program, break, run, wait, halt, print, write /p); Groovy Debugger.readFileRegisters | Cannot talk to QEMU; official MDB docs were unreachable (503/403) during this research |
| OpenOCD | Silicon via CMSIS-DAP | at91samd flash driver does not recognize DID series 6 | mdw/mdb via the cortex_m target (expected to work, unverified) | Flash programming fails part detection; no PIC32CM configs in the repo |
| Cortex-Debug (VS Code) | Twin or silicon | servertype qemu (cpu, machine), pyocd, jlink, openocd, external (gdbTarget host:port) | launch.json | MPLAB X and MPLAB VS Code (mplab-core-da): no documented generic GDB-remote support found |

## Code: ATDF encodes set/clear aliases and W1C flags (AC module) (C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221 : PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1667-1676)
```xml
        <register name="INTENSET" offset="0x5" rw="RW" size="1" atomic-op="set:INTENSET" initval="0x00" caption="Interrupt Enable Set">
          <bitfield name="COMP0" caption="Comparator 0 Interrupt Enable" mask="0x1"/>
          <bitfield name="COMP1" caption="Comparator 1 Interrupt Enable" mask="0x2"/>
          <bitfield name="COMP2" caption="Comparator 2 Interrupt Enable" mask="0x4"/>
          <bitfield name="COMP3" caption="Comparator 3 Interrupt Enable" mask="0x8"/>
          <bitfield name="WIN0" caption="Window 0 Interrupt Enable" mask="0x10"/>
          <bitfield name="WIN1" caption="Window 1 Interrupt Enable" mask="0x20"/>
        </register>
        <register name="INTFLAG" offset="0x6" rw="RW" size="1" atomic-op="clear:INTFLAG" initval="0x00" caption="Interrupt Flag Status and Clear">
          <bitfield name="COMP0" caption="Comparator 0" mask="0x1"/>
```

## Code: DFP reset value contradicts its own bitfields (SCB.CCR) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6606-6609)
```xml
        <register name="CCR" offset="0xD14" rw="R" size="4" access-size="4" initval="0x00000204" caption="Configuration and Control Register">
          <bitfield name="UNALIGN_TRP" caption="Unaligned accesses generates a Hard Fault" mask="0x8" values="SystemControl_CCR__UNALIGN_TRP"/>
          <bitfield name="STKALIGN" caption="Stack 8-byte aligned on exception entry" mask="0x200" values="SystemControl_CCR__STKALIGN"/>
        </register>
```

## Code: QEMU's M-profile CCR reset (v6-M gets STKALIGN|UNALIGN_TRP = 0x208) (qemu: target/arm/cpu.c:530-540)
```c
        env->v7m.ccr[M_REG_NS] = R_V7M_CCR_STKALIGN_MASK;
        env->v7m.ccr[M_REG_S] = R_V7M_CCR_STKALIGN_MASK;
        if (arm_feature(env, ARM_FEATURE_V8)) {
            /* in v8M the NONBASETHRDENA bit [0] is RES1 */
            env->v7m.ccr[M_REG_NS] |= R_V7M_CCR_NONBASETHRDENA_MASK;
            env->v7m.ccr[M_REG_S] |= R_V7M_CCR_NONBASETHRDENA_MASK;
        }
        if (!arm_feature(env, ARM_FEATURE_M_MAIN)) {
            env->v7m.ccr[M_REG_NS] |= R_V7M_CCR_UNALIGN_TRP_MASK;
            env->v7m.ccr[M_REG_S] |= R_V7M_CCR_UNALIGN_TRP_MASK;
        }
```

## Code: Upstream qtest reset-value pattern (template for generated tests) (qemu: tests/qtest/stm32l4x5_gpio-test.c:209-219)
```c
    qtest_system_reset(global_qtest);

    uint32_t moder = gpio_readl(GPIO_A, MODER);
    uint32_t odr = gpio_readl(GPIO_A, ODR);
    uint32_t otyper = gpio_readl(GPIO_A, OTYPER);
    uint32_t pupdr = gpio_readl(GPIO_A, PUPDR);
    uint32_t idr = gpio_readl(GPIO_A, IDR);
    /* 15: AF, 14: AF, 13: AF, 12: Analog ... */
    /* here AF is the same as Analog and Input mode */
    g_assert_cmphex(moder, ==, reset(GPIO_A, MODER));
    g_assert_cmphex(odr, ==, reset(GPIO_A, ODR));
```

## Code: DFP's silicon reset-and-halt sequence (capture post-reset state before any code runs) (C:/Users/C19257/.mchp_packs/Microchip/PIC32CM-JH_DFP/1.3.221 : scripts/dap_cortex-m0plus.py:462-485)
```python
def reset_and_halt():
    halt_target()
    dev.Write32(arm.DEMCR, 0x01000001) # TRCENA  | VC_CORERESET
    dev.Write32(arm.AIRCR, 0x05fa0004) # VECTKEY | SYSRESETREQ
    n = 0
    retries = 10
    seenReset = False
    while n<retries:
        dhcsr = dev.Read32(arm.DHCSR)
        #print_dhcsr_v("arm.DHCSR",dhcsr)
        if (dhcsr & 0x02000000): # wait for S_RESET_ST
            seenReset=True
            dev.Write32(arm.DHCSR, 0xa05f0003) # DBGKEY|C_HALT|C_DEBUGEN
            dev.Write8(dsu_statusa, 2) # release the cpu 0x41002101,0x2  STATUSA.CRSTEXT
            dhcsr = print_dhcsr("Cleared CRSTEXT")
        hasHalted = 0 != (dhcsr & 0x20000) # S_HALT
        if seenReset:
            if hasHalted: # wait for S_HALT
                break
        dev.Delay(100000)
        n=n+1
    dev.Write32(arm.DEMCR, 0x01000000) # TRCENA
    if n==retries:
        raise Exception("The device did not come out of reset")
```

## Code: HMP 'xp' reads MMIO with unspecified attributes (side effects), 'x' goes through the debug path (qemu: monitor/hmp-cmds.c:650-663 (not in sparse checkout; https://github.com/qemu/qemu at 602e4f2da1))
```c
        if (is_physical) {
            AddressSpace *as = cs ? cs->as : &address_space_memory;
            MemTxResult r = address_space_read(as, addr,
                                               MEMTXATTRS_UNSPECIFIED, buf, l);
            if (r != MEMTX_OK) {
                monitor_hmp_printf(hmp, " Cannot access memory\n");
                break;
            }
        } else {
            if (cpu_memory_rw_debug(cs, addr, buf, l, 0) < 0) {
                monitor_hmp_printf(hmp, " Cannot access memory\n");
                break;
            }
        }
```

## Code: Arm debug translations mark accesses with attrs.debug (gdbstub / HMP x) (qemu: target/arm/ptw.c:4113-4119)
```c
    if (ok) {
        /* translation succeeded */
        result->physaddr = res.f.phys_addr;
        result->attrs = res.f.attrs;
        result->attrs.debug = 1;
        result->lg_page_size = res.f.lg_page_size;
    }
```

## Code: SCR is not decoded for ARMv6-M in QEMU's NVIC; deep sleep RAZ/WI, SLEEPONEXIT ignored (qemu: hw/intc/armv7m_nvic.c:1688-1699)
```c
    case 0xd10: /* System Control.  */
        if (!arm_feature(&cpu->env, ARM_FEATURE_V7)) {
            goto bad_offset;
        }
        /* We don't implement deep-sleep so these bits are RAZ/WI.
         * The other bits in the register are banked.
         * QEMU's implementation ignores SLEEPONEXIT, which
         * is architecturally permitted.
         */
        value &= ~(R_V7M_SCR_SLEEPDEEP_MASK | R_V7M_SCR_SLEEPDEEPS_MASK);
        cpu->env.v7m.scr[attrs.secure] = value;
        break;
```

## Code: QEMU policy on AI-generated contributions (qemu: docs/devel/code-provenance.rst:291-299)
```rst
TL;DR:

  **Current QEMU project policy is to DECLINE any contributions which are
  believed to include or derive from AI generated content. This includes
  ChatGPT, Claude, Copilot, Llama and similar tools.**

  **This policy does not apply to other uses of AI, such as researching APIs
  or algorithms, static analysis, or debugging, provided their output is not
  included in contributions.**
```

## Code: checkpatch license gate (Apache-2.0 would be an ERROR) (qemu: scripts/checkpatch.pl:1379-1403 (not in sparse checkout; https://github.com/qemu/qemu at 602e4f2da1; $prefer = GPL-2.0-or-later at :1377))
```perl
    my @valid = qw(
	GPL-2.0-only
	LGPL-2.1-only
	LGPL-2.1-or-later
	BSD-2-Clause
	BSD-3-Clause
	MIT
	);

    my $nonpreferred = 0;
    my @unknown = ();
    foreach my $bit (@bits) {
	if ($bit eq $prefer) {
	    next;
	}
	if (grep /^$bit$/, @valid) {
	    $nonpreferred = 1;
	} else {
	    push @unknown, $bit;
	}
    }
    if (@unknown) {
	ERROR("Saw unacceptable licenses '" . join(',', @unknown) .
	      "', valid choices for QEMU are:\n" . join("\n", $prefer, @valid));
    }
```

## Code: Why a native Windows build fails without symlink rights (qemu: scripts/symlink-install-tree.py:28-36 (https://github.com/qemu/qemu at 602e4f2da1; run as meson postconf script, meson.build:19))
```python
    try:
        os.symlink(source, bundle_dest)
    except BaseException as e:
        if not isinstance(e, OSError) or e.errno != errno.EEXIST:
            if os.name == 'nt':
                print('Please enable Developer Mode to support soft link '
                      'without Administrator permission')
            print(f'error making symbolic link {dest}', file=sys.stderr)
            raise e
```

## Code: Upstream builds Windows system emulators by cross-compiling, not natively (qemu: .gitlab-ci.d/crossbuilds.yml:92-106 (https://github.com/qemu/qemu at 602e4f2da1))
```yaml
cross-win64-system:
  extends: .cross_system_build_job
  needs:
    - job: win64-fedora-cross-container
  variables:
    IMAGE: fedora-win64-cross
    EXTRA_CONFIGURE_OPTS: --enable-fdt=internal --disable-plugins --enable-slirp
    CROSS_SKIP_TARGETS: alpha-softmmu avr-softmmu hppa-softmmu
                        m68k-softmmu microblaze-softmmu
                        or1k-softmmu rx-softmmu sh4eb-softmmu sparc64-softmmu
                        tricore-softmmu xtensaeb-softmmu
  artifacts:
    when: on_success
    paths:
      - build/qemu-setup*.exe
```

## Code: Upstream native MSYS2 CI job builds tools only (qemu: .gitlab-ci.d/windows.yml:14-20 (https://github.com/qemu/qemu at 602e4f2da1))
```yaml
  variables:
    # Select the "64 bit, gcc and MSVCRT" MSYS2 environment
    MSYSTEM: MINGW64
    # This feature doesn't (currently) work with PowerShell, it stops
    # the echo'ing of commands being run and doesn't show any timing
    FF_SCRIPT_SECTIONS: 0
    CONFIGURE_ARGS: --disable-system --enable-tools -Ddebug=false -Doptimization=0 --enable-rust
```

## Code: Zephyr's PIC32CM JH01 Curiosity Pro runners (pyOCD target, J-Link device, MPLAB IPE) (https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/board.cmake:4-10)
```cmake
board_runner_args(pyocd "--target=pic32cm5164jh01100" "--frequency=4000")
board_runner_args(jlink "--device=PIC32CM5164JH" "--speed=4000")
board_runner_args(mplab_ipe "--tool" "EDBG" "--part" "32CM5164JH01100" "--erase" "--verify")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/mplab_ipe.board.cmake)
```

## Code: Twister uses a dedicated QEMU handler on Windows (https://github.com/zephyrproject-rtos/zephyr/blob/main/scripts/pylib/twister/twisterlib/testinstance.py:226-233)
```python
        elif simulator:
            if simulator.name == "qemu":
                if os.name != "nt":
                    handler = QEMUHandler(self, "qemu", *common_args)
                else:
                    handler = QEMUWinHandler(self, "qemu", *common_args)
                handler.args.append(f"QEMU_PIPE={handler.get_fifo()}")
                handler.ready = True
```

## Code: Upstream MCU functional test: pinned firmware asset + UART-driven assertions (qemu: tests/functional/arm/test_max78000fthr.py:14-30)
```python
    ASSET_FW = Asset(
        'https://github.com/JacksonDonaldson/max78000Test/raw/main/build/max78000.bin',
        '86940b4bf60931bc6a8aa5db4b9f7f3cf8f64dbbd7ac534647980e536cf3adf7')

    def test_fthr(self):
        self.set_machine('max78000fthr')
        fw_path = self.ASSET_FW.fetch()
        self.vm.set_console()
        self.vm.add_args('-kernel', fw_path)
        self.vm.add_args('-device', "loader,file=" + fw_path + ",addr=0x10000000")
        self.vm.launch()

        wait_for_console_pattern(self, 'started')

        # i -> prints instruction cache values
        exec_command_and_wait_for_pattern(self, 'i', 'CTRL: 00010001')

```

## Code: MDB command-file scripting (Microchip MASTERs lab, SAM D21 + EDBG) (https://github.com/stefanluethin-microchip/mu_dev11_mdbcore dev11Labs_mdbScripts/dev11mod3Lab_mdbbat-1b-cmd.txt:43-49)
```text
break testEnd
reset
run
#-in msec (don't put that command behind the cmd - mdbbat interprets it and will never stop!)
echo "start run for 4.500msec=4,5sec - stop at any time by pressing SW0"
wait 4500
halt
```

## Open questions
- Which reset value does PIC32CM JH silicon actually return for SCB.CCR (0x204 per DFP vs 0x208 per bitfields and QEMU), for DSU.DID on the 3204/6408 parts, and for TCC PER/PERBUF bits 31:24? Are these fixed in DFP 1.7.296?
- On silicon, what happens when a debugger or the CPU reads or writes a peripheral whose APB clock is masked (MCLK.APBCMASK resets to 0x08000000) or that PAC protects: a bus error/HardFault, or RAZ/WI? The twin must match it.
- Is CAL48M (and OSC32K/ADC calibration) loaded automatically from the NVM calibration area at reset on PIC32CM JH, and which registers are retained across non-POR resets? The EDC lists POR equal to MCLR for every peripheral register.
- Can MPLAB X or MDB, or the MPLAB VS Code mplab-core-da debugger, connect to a generic GDB remote target? If not, is an internal 'QEMU tool' backend for MPLAB feasible? This is for the user's MPLAB device-support organization to confirm.
- What is pic32sim's peripheral coverage for PIC32CM JH? Is it a useful oracle, or something the twin would replace?
- Will IT enable Developer Mode or grant SeCreateSymbolicLinkPrivilege, install WSL2, and provide Linux CI runners and a hardware-in-the-loop runner with an EV81X90A board?
- Is the twin intended for upstream QEMU (human-authored code only, GPL-2.0-or-later SPDX, a named Microchip maintainer, public test firmware) or for an internal fork (which allows AI-assisted code and Harmony test binaries, subject to legal review)?
- Legal: relicensing ATDF-derived tables from Apache-2.0 to GPL-2.0-or-later; whether running Harmony-built firmware on an emulator is compatible with License.md clause 4; obligations if a custom QEMU build ships to customers alongside MPLAB tooling; use of the 'QEMU' name.
- Do record/replay and reverse debugging work on M-profile machines? Upstream only tests A-profile Arm replay and reverse debugging on aarch64/ppc64/s390x/x86_64.
- What throughput does Avatar-style MMIO forwarding achieve over EDBG (CMSIS-DAP v1 HID) versus J-Link? This needs measurement before choosing a forwarding design.
- Does python/qemu/machine/qtest.py (socketpair fd passing) work on Windows hosts, or should generator-driven tests compile to C qtests?
- Could the MPLAB MCC/build MCP tools configured in this environment (mcc_generate_code, mplab_build_project, mplab_program_device, mplab_detect_connected_tools) automate test-firmware generation and programming for hardware-in-the-loop differential runs? They were not exercised in this research.
