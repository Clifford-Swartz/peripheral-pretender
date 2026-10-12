# prior-art: ecosystem and research relevant to a QEMU-based digital twin of Microchip MCU peripherals

## Summary
Upstream QEMU (11.1.50, checked 2026-10-09) has no model of any Microchip Arm MCU (SAM D/E/V, PIC32CM/CK/CZ). It has only three Microchip-related machines. The PolarFire SoC Icicle Kit is RISC-V, has many stubbed devices, is marked "Odd Fixes", and its own docs say HSS no longer boots. The SmartFusion2 M2S010 SOM is a Cortex-M3 with 5 modeled devices. The Arduino AVR boards model only the USART and the 16-bit timer. Outside upstream, Microchip MCU work on QEMU is small and either unmaintained or very new. Serge Vakulenko's PIC32MX/MZ fork is based on QEMU 2.3.50 (2015), and DICE reused it. Quarkslab's SAMD21 board is QEMU 8.2 with GPIO and SERCOM only, and is frozen. A 2026 PIC32MK board on QEMU 10.2.50 ships a pytest harness and a CLAUDE.md file, which points to AI-assisted development; its fidelity has not been validated. Renode (MIT) is the richest open model base for Microchip silicon: PolarFire SoC/Icicle/BeagleV-Fire, Mi-V, SAM4S, SAM E70, SAMD21 and SAMD51. Its Zephyr-derived dts2repl overlays for PIC32CM JH, PIC32CX, PIC32CZ and SAME54 work by faking clock-register values so firmware boots. The PIC32CM overlay labels OSCCTRL.STATUS, OSCCTRL.DPLLSTATUS and OSC32KCTRL.STATUS as "UNK1-3", and it mislabels OSC32KCTRL.XOSC32K; the DFP ATDF identifies all four exactly. Microchip's own official emulation path for PolarFire SoC and Mi-V is Renode bundled with SoftConsole (an Antmicro collaboration since 2018). MPLAB SIM is a discrete-event simulator that supports only some peripherals per device, and in 2021 it showed "partial support ... beta" warnings for the SAMD21. The local Simulator.emu maps PIC32CM JH, SAMD21 and SAME54 to the 'pic32sim' engine. Mechanisms worth borrowing:
- Renode's ApplySVD returns the SVD reset value and logs the register name for unmodeled registers; QEMU's unimp device returns 0 and logs only an offset.
- Renode's Tag and Python stubs mirror Pretender's storage, pattern and increasing register models.
- Code generators: PeakRDL-renode, dts2repl, and xPack's old SVD->JSON->QEMU tool (xsvd).
- simavr builds its cores from the avr-libc device headers.
- Intel DML is a register-bank DSL that compiles to C.

Rehosting research has already generated QEMU models for Microchip SAM parts:
- Perry (USENIX Security 2024) turns ASF drivers plus SVD into QEMU 7.2 models via KLEE, with a 74% unit-test pass rate.
- FlexEmu (CCS 2025) uses an LLM to extract model parameters from driver source and generates QEMU emulators, with a 98.5% pass rate.
- SEmu used NLP to derive rules from the SAM3X and SAM R21 manuals.
- HALucinator ships Atmel ASF v3 handlers.
- RPM/Bluezz (2026) builds an SVD skeleton and adds agent-assisted event-condition-action rules taken from reference manuals, with human review. These tools aim for firmware that runs well enough to fuzz, not for timing or bit accuracy. None of them validates against silicon, a gap a vendor with boards and RTL can close using Avatar2/Pretender/Conware-style hardware recording. Among peer vendors:
- Espressif ships its own QEMU fork through `idf.py qemu`.
- Nordic publishes Apache-2.0 peripheral models for Zephyr's bsim targets.
- Silicon Labs submits Renode pull requests itself.
- Infineon and Synopsys showed a RISC-V MCU virtual prototype in March 2025 that Infineon plans to grow into a "full digital twin".
- Arm Virtual Hardware, now run by Corellium, hosts NXP, ST and TI boards but no Microchip board.

The commercial and tool landscape has shifted. Arm stopped shipping the standalone FVP library after 11.28; from 11.29 FVPs come only with Arm Development Studio. xPack dropped its SVD-driven qemu-system-gnuarmeclipse binary in 9.2.4-1 (October 2025). For SystemC/RTL co-simulation, the mature options are:
- QBox (BSD-3; built on a Qualcomm libqemu fork based on QEMU 11.0)
- AMD/Xilinx remote-port with libsystemctlm-soc (MIT), available only in Xilinx's QEMU fork
- Renode's Verilator/DPI and SystemC bridges

Upstream QEMU's own out-of-process device support works only for PCI devices.

## Findings

1. (high) [LOAD-BEARING] [Upstream QEMU: Microchip coverage] Upstream QEMU master contains no model of any Microchip Arm Cortex-M MCU (SAM D/E/V, SAM3/4, PIC32CM/CK/CZ/CX) and no PIC32/PIC64 model. Its only Microchip-related machines are the PolarFire SoC Icicle Kit (RISC-V), the Microsemi SmartFusion2 M2S010 SOM (Cortex-M3) and the Arduino AVR boards (ATmega168/328P/1280/2560).
   detail: A PIC32CM JH / SAM-class twin in QEMU therefore starts from scratch on QEMU's generic armv7m/NVIC/SysTick plus whatever reusable IP models exist. No existing Microchip SoC file in upstream can be extended.
   - ev: grep -i 'atsam|samd2|same5|samv7|sam3x|pic32|pic64' over hw/ include/ docs/ configs/ returned no matches (local tree 11.1.50, commit 602e4f2da1)
   - ev: hw/riscv/microchip_pfsoc.c:1-9; MAINTAINERS:1795-1807
   - ev: hw/arm/msf2-soc.c:137 (cpu-type cortex-m3); MAINTAINERS:1240-1263
   - ev: hw/avr/arduino.c:81,96,111,126 (ATmega168/328P/1280/2560 boards); MAINTAINERS:1315-1334
   - ev: patchew/qemu-devel search for SAMD21/SAME70/ATSAM patch series 2025-26 found nothing (https://patchew.org/QEMU/)

2. (high) [LOAD-BEARING] [Upstream QEMU: PolarFire SoC model quality] The upstream microchip-icicle-kit machine was written by Wind River in 2020, not Microchip. Most MSS peripherals are unimplemented stubs, its status is 'Odd Fixes', and its own docs say the Hart Software Services firmware no longer runs because clock and memory-controller support is missing. No Microchip GitHub org uses this machine.
   detail: This is a cautionary example of drift. When vendor firmware moves on and nobody owns the model, the twin stops booting real boot code. Renode's PolarFire model, which Microchip uses officially, models the peripherals QEMU stubs (see the Renode PolarFire finding).
   - ev: hw/riscv/microchip_pfsoc.c:4-7 (Copyright 2020 Wind River, Bin Meng)
   - ev: hw/riscv/microchip_pfsoc.c:371-449 (create_unimplemented_device for watchdog0-4, spi0/1, i2c0/1, can0/1, usb, gpio0-2)
   - ev: docs/system/riscv/microchip-icicle-kit.rst:102-109
   - ev: MAINTAINERS:1795-1799 (S: Odd Fixes)
   - ev: GitHub code search '"microchip-icicle-kit"' in orgs polarfire-soc, linux4microchip and '"qemu-system-riscv64"' in pic64gx: 0 hits

3. (high) [Upstream QEMU: AVR (Microchip/Atmel) model scope] QEMU's ATmega model covers only the USART and 16-bit timers, which is enough for FreeRTOS demos. The 8-bit timers, TWI, ADC, external memory controller, watchdog, SPI and EEPROM are create_unimplemented_device stubs, and there is no GPIO model.
   detail: Upstream AVR support is a CPU-centric model with minimal peripherals, very different from a peripheral 'digital twin'.
   - ev: docs/system/target-avr.rst:11-15
   - ev: hw/avr/atmega.c:346-352 (avr-timer8 unimplemented)
   - ev: hw/avr/atmega.c:376-381
   - ev: ls hw/gpio shows no avr file

4. (high) [Upstream QEMU: SmartFusion2] The SmartFusion2 SoC (Microsemi, now Microchip) models the system registers, MSS timer, two SPI controllers, UART (via serial_mm) and EMAC. I2C, DMA, watchdog, GPIO, HS-DMA, CAN, RTC, APB config and USB are unimplemented stubs. The model was contributed by an individual (2017-2020) and is listed as Maintained.
   - ev: hw/arm/msf2-soc.c:66-76
   - ev: hw/arm/msf2-soc.c:213-222
   - ev: hw/arm/msf2-soc.c:3-4 (Copyright 2017-2020 Subbaraya Sundeep, MIT-style license text)
   - ev: docs/system/arm/emcraft-sf2.rst:9-15 (doc says Cortex-M4, but code uses cortex-m3 at msf2-soc.c:137)
   - ev: MAINTAINERS:1240-1263

5. (high) [LOAD-BEARING] [Upstream QEMU: who maintains MCU models] Upstream Cortex-M MCU models are mostly community- or academic-contributed and can become orphaned. STM32L4x5 came from Télécom Paris students (2023-24). The MAX78000 (2025) came from an individual and is already listed as 'Orphan'.
   detail: Relevant to the choice between upstreaming and a vendor fork. Upstream acceptance implies a long-term maintainer commitment, otherwise the model rots (see the Icicle drift above).
   - ev: hw/arm/stm32l4x5_soc.c:1-12
   - ev: hw/arm/max78000_soc.c:1-9
   - ev: MAINTAINERS:904-910 (MAX78000FTHR, S: Orphan)
   - ev: MAINTAINERS:1091-1100 (STM32L4x5, Maintained by Samuel Tardieu)

6. (high) [PIC32 QEMU fork (Serge Vakulenko)] sergev/qemu ('QEMU for PIC32') is a fork of QEMU 2.3.50. It has PIC32MX7 (Explorer-16, chipKIT Max32, Maximite) and PIC32MZ (Explorer-16, MEB-II, WiFire) machines with UART, SPI/SD card, Ethernet and GPIO models and a hex loader. It was posted to qemu-devel as a v3 series in July 2015 but never merged. The last commits (Feb 2025) only add RetroBSD syscall tracing.
   detail: The only historical Microchip-MCU QEMU board family. It is too old to rebase cheaply, but it shows a register-header-per-family approach (pic32mx.h / pic32mz.h) and that research tools (DICE) reused it.
   - ev: https://raw.githubusercontent.com/sergev/qemu/master/VERSION -> 2.3.50
   - ev: https://api.github.com/repos/sergev/qemu/contents/hw/mips (mips_pic32mx7.c, mips_pic32mz.c, pic32_uart.c, pic32_spi.c, pic32_sdcard.c, pic32_ethernet.c, pic32_gpio.c, pic32_load_hex.c, pic32mx.h, pic32mz.h)
   - ev: https://lists.gnu.org/archive/html/qemu-devel/2015-07/msg01184.html ([PATCH pic32 v3 09/16])
   - ev: https://github.com/sergev/qemu/wiki (machine list)
   - ev: GitHub API: last commits 2025-02-07..10; license NOASSERTION (QEMU GPL); 34 stars

7. (medium) [PIC32MK QEMU board (2026, community, AI-assisted)] ericsonj/qemu-pic32mk (April-May 2026) adds a PIC32MK1024MCM100 board to mainline QEMU 10.2.50. It claims models for EVIC, UART x6, SPI x6, timers, NVM, data EEPROM, GPIO, OC/IC x16, CAN FD x4 (virtual bus), ADCHS, USB CDC to PTY, WDT and CRU. I2C and DMA are 'register stub + LOG_UNIMP'. A companion pytest plugin provides GPIO/ADC injection fixtures and a declarative 'qemu_state' marker. The repo root has a CLAUDE.md ('guidance to Claude Code'), and its reference was the 783-page datasheet.
   detail: Peripheral coverage is self-reported; I found no evidence of validation against silicon. It shows that an LLM-agent-assisted engineer can produce broad QEMU MCU coverage from a datasheet in weeks. It also shows the twin-plus-test-harness pattern (socket and chardev injection, pytest fixtures) that a Microchip twin would want.
   - ev: https://github.com/ericsonj/qemu-pic32mk (README sections 2 and 8; VERSION 10.2.50; CLAUDE.md)
   - ev: https://github.com/ericsonj/pytest-qemu-pic32mk (README)
   - ev: GitHub API: created 2026-04-29, pushed 2026-05-17, license NOASSERTION, 0 stars

8. (high) [SAMD21 QEMU board (Quarkslab, SSTIC 2024)] Quarkslab built a custom SAMD21G18A board on QEMU 8.2 stable with GPIO (PORT) and SERCOM models (samd21_gpio.c, samd21_sercom.c), plus off-the-shelf SPI flash. The repo has no license, and its README says it will not be updated for future QEMU versions. The slides conclude that adding a custom board 'can require days of dev' and that pushing a board upstream 'is complex'.
   detail: The only public QEMU SAMD21 work found. GitHub repo search for 'qemu samd21/same54/samv71/same70/atsam/sam3x' returned nothing else.
   - ev: https://github.com/quarkslab/sstic-tame-the-qemu (README; GitHub API: created 2024-05-31, pushed 2024-06-10, no license)
   - ev: https://sstic.org/media/SSTIC2024/SSTIC-actes/tame_the_qemu_debug_firmware_on_custom_emulated_bo/SSTIC2024-Slides-tame_the_qemu_debug_firmware_on_custom_emulated_board-cauquil.pdf (slides 6-7, 30-33, 39-40)

9. (high) [LOAD-BEARING] [Renode: Microchip platforms and models] Renode ships Microchip/Atmel platforms and models:
- Platforms (renode/renode): polarfire-soc, mpfs-icicle-kit, beaglev-fire, miv, miv_rv32, miv-board, sam4s/sam4s8b/sam4s16c, sam4s_xplained, sam_e70, atsamd21j17d-aft, atsamd51g19a.
- PolarFire models (renode-infrastructure): MPFS_{CAN, PDMA, GPIO, I2C, eNVM, DDRMock, Sysreg, SystemServices, PCIe, SDController, QSPI, SPI, RTC, Timer, Watchdog, USB}.
- Mi-V models: MiV_{CoreGPIO, CoreTimer, CoreUART, CoreLevelInterruptor}.
- SAM models: SAM4S_{ADC, CRCCU, PIO, TWI, DACC, EEFC, RSTC, WDT}, SAM_{PDC, SPI, TC, TRNG, USART}, SAMD21_{GPIO, I2C, RTC, Timer}, SAMD20_UART, SAMD5_UART.
All are MIT-licensed and Antmicro-copyrighted. MPFS_DDRMock.cs also carries 'Copyright (c) 2020-2023 Microchip'.
   detail: Model depth varies: SAMD5_UART.cs is 181 lines (DATA/INTFLAG/INTEN only, with tagged CTSIC/RXBRK/ERROR bits), SAMD21_Timer.cs 500 lines, SAM_USART.cs 815 lines. The code is C# and can't be dropped into QEMU, but it is MIT-licensed reference material for behavior.
   - ev: git ls-tree of https://github.com/renode/renode (HEAD 150e30d3, 2026-10-09): platforms/cpus/*.repl, platforms/boards/*.repl
   - ev: git ls-tree of https://github.com/renode/renode-infrastructure (HEAD 515cc63e, 2026-10-09): src/Emulator/Peripherals/Peripherals/**
   - ev: renode-infrastructure src/Emulator/Peripherals/Peripherals/Miscellaneous/MPFS_DDRMock.cs:1-6
   - ev: renode LICENSE:1-4 (MIT); latest release v1.17.0 (2026-09-07)

10. (high) [Renode vs QEMU: PolarFire SoC fidelity] Renode's polarfire-soc.repl instantiates real models for the blocks QEMU stubs:
- 5 MPFS_Watchdog, 2 MPFS_SPI, 2 MPFS_I2C, 2 MPFS_CAN, MPFS_USB and 3 MPFS_GPIO
- MPFS_PCIe, MPFS_eNVM, MPFS_Sysreg, MPFS_PDMA and a crypto block (AthenaX5200)
   - ev: renode platforms/cpus/polarfire-soc.repl (type counts via git cat-file at HEAD 150e30d3)
   - ev: QEMU hw/riscv/microchip_pfsoc.c:371-449 (the same blocks as unimplemented stubs)

11. (high) [LOAD-BEARING] [Renode modeling mechanisms worth borrowing] Renode offers layered fidelity:
(1) ApplySVD: reads of unimplemented registers return the SVD reset value, and reads and writes are logged with peripheral and register names.
(2) 'Tag <addr size> "NAME" value': named constant registers.
(3) Python peripherals (repeater/flipflop/counter scripts or inline scripts).
(4) C# register-framework models (Define/WithFlag/WithValueField/WithTaggedFlag/value-provider and write callbacks).
(5) HDL co-simulation. A DPI-based method serves Verilator, Questa and VCS over TCP sockets (AHB, APB3, AXI4, AXI4-Lite), and a direct Verilator C++ method covers APB3, AXI4, AXI4-Lite, Wishbone and CFU.
(6) A SystemC TLM bridge (SystemCPeripheral proxy plus a renode_bridge module over sockets, 2024).
Models must ship with Robot Framework tests to be upstreamed.
   detail: A QEMU twin can borrow the ApplySVD idea directly: generate from the ATDF a 'named-unimplemented-register' device that returns ATDF initval and logs NAME.FIELD. Upstream QEMU's unimp device returns 0 and logs only an offset (hw/misc/unimp.c:21-29). Reset-value-correct, self-describing stubs are a cheap first fidelity tier for every peripheral not yet modeled.
   - ev: renode-infrastructure src/Emulator/Main/Peripherals/Bus/SVDParser.cs:85-109 and :320-335 (MIT)
   - ev: renode platforms/cpus/sam_e70.repl:41-49 (PythonPeripheral flipflop + Tag CIDR + ApplySVD ATSAME70Q21.svd)
   - ev: renode scripts/pydev/{repeater,flipflop,counter}.py
   - ev: https://renode.readthedocs.io/en/latest/advanced/writing-peripherals.html
   - ev: https://renode.readthedocs.io/en/latest/advanced/co-simulating-with-an-hdl-simulator.html
   - ev: https://antmicro.com/blog/2024/07/systemc-co-simulation-in-renode/

12. (high) [Renode code generators: PeakRDL-renode, dts2repl] Antmicro generates parts of models instead of hand-writing them. PeakRDL-renode (in renode/tools) turns SystemRDL into a C# partial class holding the register map, and behavior is written in a separate file. dts2repl (Apache-2.0) converts Zephyr devicetree into Renode .repl platforms plus per-compatible overlays. Antmicro credits dts2repl and model work for raising the Renode Zephyr dashboard from about 300 to more than 470 passing boards (June 2024).
   detail: This is the same split a Microchip twin would want. Machine-generated register skeletons come from authoritative data (ATDF/SVD, or Microchip's internal register DB), and hand-written behavior lives alongside them so regeneration never clobbers it.
   - ev: renode tools/PeakRDL-renode/README.md (Copyright 2024 Antmicro)
   - ev: https://antmicro.com/blog/2025/02/systemrdl-support-for-renode-model-generation
   - ev: https://github.com/antmicro/dts2repl (README; Apache-2.0; pushed 2026-10-07)
   - ev: https://zephyrproject.org/improving-data-sources-and-tests-in-the-renode-zephyr-dashboard-and-renodepedia/

13. (high) [LOAD-BEARING] [Renode PIC32CM overlay vs ATDF ground truth] dts2repl's overlay 'microchip,pic32cm.repl' applies to Zephyr's pic32cm_jh01_cpro board, whose root compatible includes 'microchip,pic32cm'. It forces clock-ready values through Tags named UNK1/UNK2/UNK3 and one mislabeled 'OSCCTRL_STATUS', plus Python 'repeater' stubs. The PIC32CM JH DFP ATDF resolves each one:
- 0x4000100C = OSCCTRL.STATUS; 0x11 sets XOSCRDY|OSC48MRDY
- 0x40001030 = OSCCTRL.DPLLSTATUS; 0x03 sets LOCK|CLKRDY
- 0x4000140C = OSC32KCTRL.STATUS; 0x03 sets XOSC32KRDY|OSC32KRDY
- 0x40001414 is not OSCCTRL.STATUS but OSC32KCTRL.XOSC32K, a 16-bit read-write control register with reset value 0x0080, now frozen as the constant 0x3
   detail: This is the clearest concrete case for a vendor-built twin. Community models of Microchip parts are 'boot-enough spoofs' (Antmicro's own word) with guessed register identities. The authoritative ATDF/EDC data the user already maintains turns these into named, reset-correct, field-aware models. dts2repl has similar overlays for pic32cx, pic32cz, pic32cm_gc00/sg00, same54/sam_e54_cult, samv71, same70, samd21, samr21/34, saml21, mec1501/mec172x, mpfs and miv-soc.
   - ev: https://github.com/antmicro/dts2repl/blob/main/dts2repl/overlay/microchip,pic32cm.repl lines 1-16; commit history: 2026-04-01 '`pic32cm` bringup', 2026-08-03 'Fix {G,M}CLK and OSCCTRL spoofs for `pic32c{x,m}`'
   - ev: dts2repl/dts2repl.py:1183-1202 (overlays = root + /soc compatibles)
   - ev: zephyr boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.dts:17 (compatible = "pic32cmjh01,cpro", "microchip,pic32cm5164jh01100", "microchip,pic32cm")
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:597 (OSCCTRL base 0x40001000), :584 (OSC32KCTRL base 0x40001400), :4860 (OSCCTRL STATUS offset 0xC), :4927 (DPLLSTATUS offset 0x30), :4761 (OSC32KCTRL STATUS offset 0xC), :4770 (XOSC32K offset 0x14, RW, initval 0x0080)

14. (high) [LOAD-BEARING] [Microchip-Antmicro collaboration / Microchip's official emulation] Microchip's public emulation path for PolarFire SoC and Mi-V is Renode. Antmicro added PolarFire SoC in Renode 1.6 (Dec 2018), bundled with SoftConsole. Antmicro's Dec 2022 post says 'Antmicro and Microchip have been working hand in hand', notes pre-silicon use, and says SAM E70-class Cortex-M parts 'can also be simulated'. Microchip's own polarfire-soc example repos (active 2026-09) ship 'renode-emulation-platform' launch configs and state 'Renode is integrated with the SoftConsole Tool'. No public Microchip QEMU model or announcement was found.
   detail: Microchip already has an institutional Renode relationship. The QEMU direction the user asked about should be framed against this: either complement it (QEMU for MCUs and MPLAB integration) or reuse the relationship (Renode for MCUs too).
   - ev: https://antmicro.com/blog/2022/12/microchip-riscv-support-in-renode
   - ev: https://antmicro.com/blog/2018/12/renode-release-1-6
   - ev: https://github.com/polarfire-soc/polarfire-soc-bare-metal-examples/blob/main/driver-examples/mss/mss-usb/mpfs-usb-host-hid/Working_with_Renode.md (repo pushed 2026-09-01)
   - ev: renode-infrastructure MPFS_DDRMock.cs:2-3 (Microchip co-copyright)
   - ev: SoftConsole latest listed release v2022.2 per microchip.com search snippet (microchip.com pages returned 403/503 to direct fetch)

15. (medium) [PIC64GX / PIC64 emulation] No QEMU or Renode model of PIC64GX or PIC64-HPSC was found. Zephyr has a pic64gx_curiosity_kit board, and Microchip's pic64gx and linux4microchip GitHub orgs contain only upstream QEMU references (Zephyr and U-Boot forks), not PIC64 models.
   detail: This is an absence finding; Microchip-internal work would not be visible.
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic64gx_curiosity_kit/doc/index.html
   - ev: GitHub code search 'qemu org:pic64gx' -> pic64gx-zephyr/boards/qemu/* (upstream Zephyr QEMU boards only)
   - ev: renode platforms listing has no pic64* file
   - ev: Web searches 'PIC64GX Renode OR QEMU' returned no emulator

16. (high) [Wokwi] Wokwi simulates three Microchip AVR parts: ATmega328P (Uno/Nano), ATmega2560 (Mega) and ATtiny85. No SAM or PIC parts are supported. Its AVR core avr8js (MIT, active 2026) has one TypeScript module per peripheral, each with a unit-test spec: adc, clock, eeprom, gpio, spi, timer, timer-attiny, twi, usart, usi, watchdog. The Custom Chips API (beta) builds chips in C (or Rust/AssemblyScript) to WebAssembly, with experimental Verilog, and exposes GPIO, analog, timer, UART, I2C, SPI and framebuffer APIs. Wokwi CI runs simulations in the cloud with tiered minute quotas, and its CLI has experimental MCP support for AI agents.
   detail: Ideas to borrow: a per-peripheral unit-test spec next to each model; a stable, sandboxed external-part API so board-level parts (sensors, displays) are user-extensible without touching the MCU model; CI and agent (MCP) access.
   - ev: https://docs.wokwi.com/getting-started/supported-hardware
   - ev: https://docs.wokwi.com/chips-api/getting-started
   - ev: https://github.com/wokwi/avr8js (MIT; GitHub API pushed 2026-08-31); https://api.github.com/repos/wokwi/avr8js/contents/src/peripherals
   - ev: https://docs.wokwi.com/wokwi-ci/getting-started

17. (high) [simavr] simavr (GPL-3.0, active 2026-09) instantiates each AVR core by including avr-libc's per-device register header (e.g., avr/iom328p.h) into a generic family template, so register addresses come from the toolchain's vendor-derived definitions. It models EEPROM, watchdog, IO ports with pin interrupts, 8/16-bit timers, UART, SPI, I2C, external interrupts, ADC and self-programming. It also has an IRQ-based 'parts' layer for external components, VCD tracing and gdb.
   detail: This is a direct precedent for spec-driven model instantiation from vendor data. The Microchip equivalent is generating QEMU SoC and peripheral instances from DFP headers or ATDF, with shared family templates (e.g., one SERCOM model parameterized per device).
   - ev: https://github.com/buserror/simavr (README; GitHub API license GPL-3.0, pushed 2026-09-26)
   - ev: https://github.com/buserror/simavr/blob/master/simavr/cores/sim_mega328.c lines 22-42

18. (medium) [gpsim and Proteus VSM] gpsim (GPL-2.0+, PIC10/12/16/18 with pin stimuli and loadable modules) last released 0.32.1 on 2023-11-03. Proteus VSM (Labcenter, commercial) claims more than 750 supported CPU variants whose models 'fully simulate I/O ports, interrupts, timers, USARTs and all other peripherals'. Its packages cover PIC10/12/16/18/24/dsPIC33 and AVR, and a Cortex-M3 package lists ATSAM3N parts.
   detail: Proteus's Microchip coverage beyond PIC and AVR is not well documented publicly. Labcenter's microprocessor list page returned 404.
   - ev: https://gpsim.sourceforge.net/
   - ev: https://sourceforge.net/projects/gpsim/files/gpsim/ (0.32.0/0.32.1, 2023-11-03)
   - ev: https://www.labcenter.com/simulation/
   - ev: https://robotshop.com/products/proteus-vsm-software-arm-cortex-m3 (ATSAM3N list; reseller page)

19. (medium) [LOAD-BEARING] [MPLAB SIM (public documentation) and local engine map] Microchip documents the MPLAB X simulator as discrete-event. Stimuli are evaluated only at instruction boundaries, so interrupt latency may be inaccurate. Peripheral coverage is listed per device ('Simulator Peripheral Support by Device' under Help > Release Notes > Debug Features Support, also a v5.40 PDF/CSV grid), and stimulus can be scripted in SCL. A 2021 instructor report on ATSAMD21J18A shows 'W0106-SIM: This device only has partial support for PORT_GROUP0 peripheral. ** beta mode **' (also PORT_GROUP1 and TC7) and 'W0111-SIM ... We don't simulate the clock selection'. Locally, Simulator.emu maps 1013 devices to 'pic32sim', including PIC32CM JH, ATSAMD21J18A, ATSAME54P20A and PIC32CK. That engine advertises no trace, no software or runtime breakpoints, no data capture, and only watchdog as a non-address break.
   detail: The current per-device support list for PIC32CM JH could not be retrieved (microchip.com blocked automated fetch). The user can check it in MPLAB X or the internal device database. A QEMU twin would sit next to, not replace, MPLAB SIM's debug-feature contract that Simulator.emu describes.
   - ev: https://onlinedocs.microchip.com/oxy/GUID-240F27AA-C615-4705-B68C-6E434B126B47-en-US-1/GUID-D986B842-29C1-4D29-9740-F01D2A3E67DF.html and .../GUID-3B43938F-8AA4-4598-9FF1-2603D4CA60D3.html (known from search snippets only; direct fetch returned HTTP 503)
   - ev: https://www.microchip.com/content/dam/mchp/documents/mplab-x-ide-documents/SimPeripheralSupport_v5.40.pdf (HTTP 403 to fetch; described in search snippet)
   - ev: https://www.yorku.ca/professor/drsmith/2021/04/15/simulating-micros-from-command-line/
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu:105 (ATmega328P sim), :304 (ATSAMD21J18A pic32sim), :329 (ATSAME54P20A pic32sim), :2926 (PIC32CK pic32sim), :3005-3009 (PIC32CM...JH... pic32sim), :3705-3719 (pic32sim engine capabilities); engine counts pic32sim 1013, dssim 986, sim 760, pic18sim 511, pic16esim 367

20. (high) [xPack QEMU Arm / GNU MCU Eclipse QEMU] The legacy qemu-system-gnuarmeclipse was based on QEMU 2.8.0, removed the application-class Arm devices and added Cortex-M devices and graphical STM32 boards. It was declared deprecated in xPack v7.2.0-1 (2023-01-31), last shipped in 8.2.6-1 (2024-09-12), and removed in 9.2.4-1 (2025-10-09). The 9.2.4-1 release is upstream QEMU 9.2.4 with 'no major changes'. Its peripheral descriptions came from vendor SVD via the xsvd tool: convert SVD XML to JSON, patch it with mandatory info, then generate the QEMU support code (MIT, Liviu Ionescu).
   detail: Two lessons. (1) An SVD->JSON->generated-QEMU-code pipeline has been done before and is a reasonable template. (2) A fork that diverges from upstream (2.8.0) eventually dies when the platform moves (Apple Silicon). Generated code should target current upstream APIs and be regenerated, not hand-forked.
   - ev: https://xpack-dev-tools.github.io/qemu-arm-xpack/blog/2023/01/31/qemu-arm-v7-2-0-1-released/
   - ev: https://xpack-dev-tools.github.io/qemu-arm-xpack/blog/2025/10/09/qemu-arm-v9-2-4-1-released/
   - ev: https://cdn.jsdelivr.net/npm/xsvd@0.3.0/README.md
   - ev: https://lists.nongnu.org/archive/html/qemu-arm/2023-02/msg01555.html (user stuck migrating from gnuarmeclipse to qemu-system-arm)

21. (high) [LOAD-BEARING] [Espressif: vendor-maintained QEMU fork shipped with the SDK] Espressif maintains github.com/espressif/qemu (branch esp-develop; releases 'esp-develop-9.2.2-20260417', i.e., QEMU 9.2.2-based). It has machines for ESP32, ESP32-S3, ESP32-C3 and ESP32-C6 and many SoC-specific peripheral models (AES/SHA/RSA/HMAC/DS/XTS-AES, RTC_CNTL, cache, LEDC, RNG, JTAG, interrupt matrices, clocks). It ships prebuilt binaries integrated in ESP-IDF ('idf.py qemu monitor/gdb', eFuse and flash image files, secure boot v2). It even adds a virtual framebuffer that 'does not exist on real hardware'.
   detail: This is the closest peer precedent for what the user describes: a silicon vendor owning a QEMU fork tracking upstream, with shared IP-block models reused across chips. That maps naturally to Microchip IP reuse (SERCOM/TC/TCC/EIC/DMAC across SAM D/E/C and PIC32CM/CK/CZ). The twin is surfaced through the vendor IDE/SDK (for Microchip, MPLAB X/VS Code extensions and Harmony/MCC).
   - ev: https://github.com/espressif/qemu (GitHub API: default branch esp-develop, pushed 2026-06-09; releases 2025-08-18, 2026-04-19)
   - ev: GitHub API listing espressif/qemu hw/xtensa (esp32.c, esp32s3.c), hw/riscv (esp32c3.c, esp32c6.c), hw/misc (esp32*_*.c, shared esp_aes.c/esp_sha.c/esp_rsa.c/esp_hmac.c/esp_ds.c)
   - ev: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html

22. (high) [Nordic: vendor-authored peripheral models (native, not ISS)] Nordic publishes C models of nRF52833, nRF5340 and nRF54L15/LM20/LS05 peripherals (BabbleSim/ext_nRF_hw_models, Apache-2.0, 'Copyright Nordic Semiconductor ASA'). Zephyr's nrf52_bsim/nrf5340bsim boards use them through the native simulator: firmware is compiled to run natively on the host against the models, with deterministic simulated time. Modeled peripherals include RADIO, TIMER, AAR, CCM/ECB, CLOCK, EGU, FICR, GPIO/GPIOTE, NVMC, PPI/DPPI, RNG, RTC, TEMP, UART/UARTE and UICR.
   detail: This is an alternative architecture to QEMU: a fast, deterministic peripheral twin that requires recompiling firmware for the host and replacing some HAL functions. QEMU runs the unmodified Cortex-M binary. A Microchip program could offer both, sharing the same peripheral behavior code.
   - ev: https://github.com/BabbleSim/ext_nRF_hw_models (README; src/HW_models/NHW_EGU.c:1-5 SPDX Apache-2.0; pushed 2026-09-24)
   - ev: https://docs.zephyrproject.org/latest/boards/native/nrf_bsim/doc/nrf52_bsim.html

23. (medium) [Other MCU vendors' virtual-prototype practice] Peer practice by vendor:
- Silicon Labs works with Antmicro on Renode Series 2 (RADIO, LDMA, GPIO, AES, GPCRC, RNG, SYSRTC, TIMER, EUSART, USART) and itself submitted the PR adding SiXG301 (Series 3).
- NXP and Renesas are Antmicro partners; Renode added i.MX RT700, i.MX 8M Plus and S32K388 (NXP) and RZ/G2L, RA8M1 and DA14592 (Renesas).
- Infineon and Synopsys have an AURIX TC4xx VDK (2020). In March 2025 they showed a virtual prototype of Infineon's first automotive RISC-V MCU, whose SDK IAR, Elektrobit, Green Hills, HighTec, Lauterbach, PLS and TASKING were already using; Infineon plans to expand it into 'a full digital twin'.
- Upstream QEMU's TriCore TriBoard and RX gdbsim boards are community- or supplier-written (EFS-auto, Y. Sato).
   detail: Peers either partner with a simulator house (Antmicro, Synopsys) or own a fork (Espressif) or a model library (Nordic). The Infineon pre-silicon SDK for toolchain partners is a model for using a twin to enable the ecosystem before silicon.
   - ev: https://antmicro.com/blog/2025/08/silabs-series-2-in-renode
   - ev: https://antmicro.com/about/partners/
   - ev: https://antmicro.com/blog/2026/03/renode-support-for-the-nxp-i-mx-rt700-mcu
   - ev: https://news.synopsys.com/2020-10-27-Synopsys-Expands-Portfolio-of-Automotive-VDKs-with-Support-for-Infineons-AURIX-TC4xx-32-bit-Microcontroller-Family
   - ev: https://www.eenewseurope.com/en/infineon-synopsys-to-show-virtual-prototype-of-first-automotive-risc-v-microcontroller/
   - ev: QEMU hw/tricore/triboard.c:1-5,74; hw/rx/rx-gdbsim.c:175,185

24. (medium) [Arm Fast Models / FVP / Arm Virtual Hardware status] Arm FVPs were available as a standalone library up to 11.28. From 11.29 (Development Studio 2025.0, July 2025) they are 'provided solely as part of Arm Development Studio'; 11.30 shipped with DS 2025.1. The AVH cloud runs on Corellium, which offers FVPs (Corstone-300/310/315/320, Cortex-M0..M85) and third-party boards (NXP S32K family, STM32U5 IoT Discovery, TI AM62P, i.MX 8M Plus, i.MX 93, Raspberry Pi 4/5) but no Microchip boards. ARM-software/AVH-GetStarted is archived, while ARM-software/AVH (FVP usage docs) is still active.
   detail: Arm's own route is core and reference-subsystem models (no vendor peripherals). Vendor-chip models on AVH come from Corellium partnerships, and Microchip has none there. Corellium trial and pricing terms could not be fetched from Arm's support site.
   - ev: https://learn.arm.com/install-guides/fm_fvp/fvp
   - ev: https://developer.arm.com/community/arm-community-blogs/b/tools-software-ides-blog/posts/developmentstudio20250
   - ev: https://support.avh.corellium.com/devices/ and https://support.avh.corellium.com/devices/fvps/
   - ev: GitHub API: ARM-software/AVH-GetStarted archived=true; ARM-software/AVH pushed 2026-09-21
   - ev: https://veecle.ai/blog/arm-fvp-vs-qemu-renode (2026-08-13; secondary: FVPs are programmer's-view, 'cannot be used to measure software performance')

25. (medium) [Synopsys / Siemens / Cadence commercial digital twins] The commercial SoC-level 'digital twin' offerings in 2025 focus on automotive and Arm CSS, not MCU peripherals. Siemens PAVE360 (June 2025: cloud access to Arm Zena CSS virtual models; Innexis ANA/VSI, with VSI using the Digital Twin Description Language) and Cadence Helium Virtual and Hybrid Studio (Arm Zena CSS, 2025; Fast Model library, virtual-to-RTL 'gearshift') are examples. Synopsys Virtualizer VDKs exist for Infineon AURIX and Renesas RH850 through Centers of Excellence. No Microchip MCU VDK or virtual prototype announcement was found.
   detail: Whether 'Siemens Vista' was renamed into the Innexis line could not be verified.
   - ev: https://blogs.sw.siemens.com/pave360/2025/06/04/siemens-and-arm-work-together-to-build-digital-twin-solutions-for-ai-defined-vehicle/
   - ev: https://www.siemens.com/en-gb/products/ic/hav/innexis/virtual-system-interconnect/
   - ev: https://community.cadence.com/cadence_blogs_8/b/corporate-news/posts/cadence-arm-collaborate-on-zena-css-based-socs-for-automotive-semiconductors
   - ev: https://us.design-reuse.com/news/37455/synopsys-renesas-rh850-virtualizer-development-kit.html
   - ev: Web search 'Microchip virtual prototype OR digital twin ... Synopsys OR Siemens OR MachineWare OR Lauterbach' -> nothing Microchip-specific (Siemens PartQuest at MASTERs 2025 is a design-enablement tool, not a virtual MCU)

26. (medium) [Simics / DML, Lauterbach, TASKING] Intel's Simics Simulator Public Release is free under an end-user license to defined user groups. Wind River sells commercial Simics (Simics 7 solution brief, April 2025). DML (MPL-2.0, active 2026) is a register-bank, field, event and interface DSL compiled to C, and today only targets Simics. Lauterbach's TRACE32 instruction-set simulator has an API for adding user peripheral simulation models (timers, communication interfaces, memory-mapped peripherals). TASKING's TriCore simulator is built on Infineon's TSIM and supports the AURIX TC4xx Virtual Prototype.
   detail: Ideas to borrow: a register DSL that compiles to simulator C (DML-like) is a cleaner target for ATDF-driven generation than raw QEMU C. The Infineon/TASKING/Lauterbach pattern shows a silicon vendor supplying its simulator core to third-party debuggers, analogous to exposing a Microchip twin through MPLAB and through partners.
   - ev: https://www.intel.com/content/www/us/en/developer/articles/tool/simics-simulator.html
   - ev: https://www.windriver.com/resource/intel-simics-7-release-solution-brief
   - ev: https://github.com/intel/device-modeling-language (README; SPDX MPL-2.0)
   - ev: https://www.lauterbach.com/sim.html; https://repo.lauterbach.com/publications/lauterbach_apis.pdf
   - ev: https://www.tasking.com/support/tricore/readme_v5.0r2.html

27. (high) [LOAD-BEARING] [SystemC/TLM and RTL co-simulation bridges] Four bridge options exist, plus PCI-only support upstream:
- QBox (Qualcomm/GreenSocs; BSD-3-Clause; v7.4.0, 2026-10-03) embeds QEMU into SystemC TLM-2.0 via libqemu-cxx. It builds against Qualcomm's QEMU fork (tag libqemu-v11.0-v0.16), supports Cortex-M7/M55 and ships a cortex-m55-remote platform.
- AMD/Xilinx libsystemctlm-soc (MIT) connects to QEMU through libremote-port, a socket protocol that serializes QEMU transactions and TLM payloads, and to RTL through Verilator. remote-port exists only in Xilinx's QEMU fork (based on 10.2.4; remote-port-memory-master/slave, gpio, stream, net, PCI).
- MachineWare VCML and MINRES SystemC-Components are Apache-2.0 SystemC/TLM model libraries.
- Renode has a SystemC bridge (2024).
- Upstream QEMU's out-of-process device options (multi-process x-pci-proxy-dev, vfio-user client) are PCI-oriented.
   detail: If Microchip has internal SystemC/TLM or Verilog models of its peripherals (e.g., from IP verification), these bridges are the path to a high-fidelity 'golden' mode. The cost is a QEMU fork (remote-port, libqemu) and a much slower simulation. A small MMIO-forwarding sysbus device would be needed to do the same on upstream QEMU.
   - ev: https://github.com/qualcomm/qbox (README; LICENSE BSD-3; CMakeLists.txt:55 LIBQEMU_GIT=https://github.com/qualcomm/qemu.git; package-lock.cmake GIT_TAG libqemu-v11.0-v0.16)
   - ev: https://github.com/Xilinx/libsystemctlm-soc (README; LICENSE 'All files are distributed under the MIT license')
   - ev: https://github.com/Xilinx/qemu hw/core/remote-port*.c; VERSION 10.2.4
   - ev: https://github.com/machineware-gmbh/vcml (Apache-2.0); https://github.com/Minres/SystemC-Components (Apache-2.0)
   - ev: QEMU docs/system/multi-process.rst:46-66; hw/vfio-user/pci.c present; grep for remote-port in hw/ include/ docs/ finds only Xen event-channel hits

28. (high) [LOAD-BEARING] [Hardware-in-the-loop and trace-based model learning (Avatar2, Pretender, Conware)] Three record-and-learn systems:
- Avatar2 (BAR 2018, Apache-2.0, last push 2025-03) orchestrates QEMU/PANDA/Unicorn with real hardware via OpenOCD, GDB, J-Link or Inception, forwarding MMIO to the physical board.
- Pretender (RAID 2019, Apache-2.0) records MMIO and interrupt traces through avatar2 on STM32 Nucleo L152RE/F072RB and Maxim MAX32600 boards. It clusters accesses into peripherals with DBSCAN (epsilon 0x100) and fits per-register models (Simple Storage, Pattern, Increasing via linear regression, Write-only) plus interrupt-timing models, targeting 'survivable execution'.
- Conware (AsiaCCS 2021) records firmware-peripheral exchanges from vendor example code on dev boards, builds mergeable automata, and emulated an uninstrumented binary using merged models of six peripherals.
   detail: Pretender's register classes are the same idea as Renode's stub scripts: repeater ~ storage, flipflop ~ pattern, counter ~ increasing. For Microchip the borrowable piece is the loop itself: run MCC/Harmony example projects on real boards (or RTL simulation), record register traces, and replay them as differential regression tests against the QEMU models. The aim is validation rather than learning models from scratch.
   - ev: https://github.com/avatartwo/avatar2 (README publications list line 98; avatar2/targets: gdb, inception, jlink, openocd, panda, pypanda, qemu, unicorn)
   - ev: https://www.s3.eurecom.fr/docs/raid19_pretender.pdf (Sections 3.2-3.3; code https://github.com/ucsb-seclab/pretender)
   - ev: https://www.usenix.org/conference/raid2019/presentation/gustafson
   - ev: https://seclab.cs.ucsb.edu/publications/2021spenskyconware/

29. (high) [HAL-level rehosting with Microchip ASF support (HALucinator)] HALucinator (USENIX Security 2020; GPL-3.0, Sandia; maintained repo halucinator/halucinator pushed 2026-08) replaces vendor HAL functions with Python handlers inside QEMU via avatar2. It ships Atmel ASF v3 handler modules (usart, timers, ext_interrupt, rf233 radio, sd_mmc, KSZ8851 ethernet, edbg stub, contiki) and SAMR21 test configurations.
   detail: Idea to borrow: an optional fast 'HAL twin' mode for Harmony/MCC driver APIs, useful for application-level CI. It is not a substitute for register-level fidelity.
   - ev: https://github.com/halucinator/halucinator (src/halucinator/bp_handlers/atmel_asf_v3/*.py; test/SAMR21/*; LICENSE GPL-3.0 with Sandia notice)
   - ev: https://github.com/embedded-sec/halucinator (original repo, last push 2021-10)
   - ev: avatar2 README publications list (sec20summer_clements_prepub.pdf)

30. (high) [Access-pattern and symbolic MMIO inference (P2IM, uEmu, Jetset, Fuzzware, Hoedur)] Five systems infer MMIO behavior automatically:
- P2IM (USENIX Sec 2020, Apache-2.0) categorizes registers by access pattern and feeds fuzzer data to data registers. It is built on a QEMU 2.3.50 tree with a 'gnuarmeclipse' directory and ran 79% of 70 samples without help.
- uEmu (USENIX Sec 2021) symbolically executes firmware in S2E to build a knowledge base of responses per access point (93% of unit tests).
- Jetset (USENIX Sec 2021, GPL-2.0) uses symbolic execution to reach a target and generates C device models for a modified QEMU.
- Fuzzware (USENIX Sec 2022, Apache-2.0; Unicorn plus angr) derives per-access Passthrough/Constant/Set/Bitextract models across 77 firmware images on 19 platforms.
- Hoedur (USENIX Sec 2023, AGPL-3.0) fuzzes with multi-stream inputs on QEMU built as a library through Rust bindings (qemu-sys/qemu-rs), reusing Fuzzware models.
   detail: These systems aim at firmware coverage. They answer 'what value makes this read path proceed', not 'what would silicon do'. For a vendor twin they are most useful as gap detectors: run customer-like firmware, flag MMIO reads whose consumed bits the model doesn't produce, and reuse P2IM/Fuzzware unit-test corpora as smoke tests.
   - ev: https://www.usenix.org/conference/usenixsecurity20/presentation/feng; https://github.com/RiS3-Lab/p2im (qemu/src/qemu.git/VERSION 2.3.50; gnuarmeclipse/ dir; LICENSE Apache-2.0)
   - ev: https://usenix.org/conference/usenixsecurity21/presentation/zhou; https://github.com/MCUSec/uEmu (README: S2E 2.0)
   - ev: https://www.usenix.org/conference/usenixsecurity21/presentation/johnson; https://github.com/aerosec/jetset (README lines 11, 104)
   - ev: https://www.usenix.org/conference/usenixsecurity22/presentation/scharnowski; https://github.com/fuzzware-fuzzer/fuzzware modeling/fuzzware_modeling/model_detection.py:13,46,130,231
   - ev: https://github.com/fuzzware-fuzzer/hoedur (README crate list); hoedur-experiments citation (USENIX Security 23)

31. (high) [DMA rehosting and PIC32 (DICE)] DICE (IEEE S&P 2021) identifies DMA input channels from the source and destination pointers firmware writes into DMA controllers. It was integrated into P2IM (Cortex-M) and into a PIC32 emulator that is the sergev/qemu fork (MIPS M4K/M-class), and it detected 33 of 37 DMA input channels with no false positives across 9 DMA controllers from 5 vendors.
   detail: DMA, event-system and peripheral-to-peripheral triggers (EVSYS/DMAC on PIC32CM JH) are where register-only stubs fail. A twin needs explicit memory-side and event-side modeling.
   - ev: https://github.com/RiS3-Lab/DICE-DMA-Emulation (README abstract; booktitle 42nd IEEE S&P 2021)
   - ev: DICE .gitmodules: mips-emulator -> https://github.com/sergev/qemu.git

32. (high) [Specification-guided models from manual text (SEmu)] SEmu (ACM CCS 2022, DOI 10.1145/3548606.3559386) extracts tables from reference-manual PDFs with tabula and parses register descriptions with the Stanford tagger and parser into condition-action rules. The rules are chained at run time, and an S2E-based RuleDoctor localizes wrong or missing rules for manual fixing. Its manuals include Atmel SAM3X/SAM3A and SAM R21 datasheets, and the SAM3X rule file has 5,544 lines. Example rule: 'B,#R0x40008008,*,<=,0->0x40008010,0,=,0'.
   detail: Microchip doesn't need NLP over PDFs to get register structure; it owns the structured sources (ATDF/EDC/SVD and likely internal register databases). The remaining hard part SEmu tackles is behavioral text (e.g., 'TXC is set when...'). That is where LLM- or agent-assisted rule drafting plus human review fits.
   - ev: https://arxiv.org/abs/2208.07833; DOI per https://api.openalex.org/works/doi:10.1145%2F3548606.3559386
   - ev: https://github.com/MCUSec/SEmu (README; Manual/ list incl. Atmel-11057 SAM3X-SAM3A datasheet and sam-r21_datasheet.pdf; RuleExtraction/extractedRules/sam3x.txt 5544 lines)

33. (high) [LOAD-BEARING] [Peripheral models synthesized from vendor drivers incl. Microchip ASF (Perry)] Perry ('A Friend's Eye is A Good Mirror', USENIX Security 2024; repo perry-emu/perry) combines CMSIS-SVD metadata bundled with driver libraries and RAM/ROM ranges with driver source compiled to LLVM bitcode. KLEE and Z3 turn the drivers' 'software beliefs' into hardware behavior, and the result is emitted as QEMU (v7.2) peripheral and board models. It was evaluated on 10 driver libraries including Microchip ASF v3.52.0 for SAM4L-EK, SAM4E Xplained Pro, SAM4S Xplained, SAM E70 Xplained, SAM V71 Xplained Ultra and SAM3X8E (PMC, UART, USART, PIO, SPI, TWI, TC...), with a 74.24% unit-test pass rate.
   detail: This is direct prior art: QEMU models of Microchip SAM peripherals generated from Microchip's own drivers. A vendor can do better by cross-checking driver-derived behavior against the ATDF and against silicon traces. Driver-derived models inherit driver assumptions and miss behavior the driver never exercises.
   - ev: https://usenix.org/system/files/usenixsecurity24-lei.pdf (Table 2; Sections 4-5; ~19k LOC; Clang/LLVM, KLEE, Z3)
   - ev: https://github.com/perry-emu/perry (README: QEMU v7.2; '-a: Output everything within a single file so that you can easily integrate the model into QEMU'; pushed 2026-06-11)

34. (high) [LOAD-BEARING] [LLM-assisted peripheral model generation (FlexEmu, CCS 2025)] FlexEmu (Lei et al., accepted at ACM CCS 2025) defines 9 structural primitives (Reg, RegField, RegFieldState, RegFieldMap, Swt, Upd, Evt, MemField, MemFieldState) and one semantic model per peripheral class, covering 12 classes: ADC, DAC, DMA, Ethernet, GPIO, RNG, SDHC, SDIO, SPI, I2C, timer and UART. An LLM (Gemini 1.5 Flash) reads driver source and returns JSON primitive instances, and a backend generates QEMU emulators. It reports a 98.48% unit-test pass rate on 90 firmware samples across 15 MCUs from ST, NXP and Microchip; the Microchip boards were SAM4L-EK, SAM4E/4S Xplained, SAM E70, SAM V71 and SAM3X8E. It also found 10 bugs fuzzing three RTOSes.
   detail: Ideas to borrow: (a) a small primitive vocabulary between 'spec' and 'QEMU C' makes LLM output checkable (JSON against a schema, not free-form C); (b) per-class semantic templates (one UART template parameterized per IP) match Microchip's IP reuse. The paper reports no hallucination or error analysis or silicon validation in the parts read.
   - ev: https://arxiv.org/abs/2509.07615 (v1 2025-09-09; 'Accepted to appear at the 32nd ACM CCS')
   - ev: https://arxiv.org/html/2509.07615v1 (Sections on primitives and frontend; Table 2; Availability: https://github.com/FlexEmu/flexemu)
   - ev: GitHub API FlexEmu/flexemu: pushed 2025-10-23, no license, 2 stars

35. (high) [LOAD-BEARING] [Reactive peripheral modeling with SVD + agent-assisted rules (RPM/Bluezz, 2026)] RPM (arXiv 2608.29737, 2026-08-30; EPFL HexHive et al.) parses CMSIS-SVD for register layout and enriches that skeleton with event-condition-action rules derived from the reference manual. The rules capture interrupt, MMIO and DMA interactions and implicit state changes. Bluezz, built on Fuzzware/Unicorn, generated more than 10K lines of model code for the nRF52840 from about 2K lines of hand-written rules (91 rules for 9 peripherals, about 12.5 h). An agent that drafts rules from documentation, with a human reviewing and validating, cut this to about 2 h. It reached about 2.6x the basic-block coverage of prior rehosting and found 5 post-connection BLE bugs.
   detail: This is the most directly reusable methodology for a vendor: generated register skeleton, a behavior rule DSL, LLM-drafted rules, then human review. The paper notes reduced effectiveness for complex, timing-sensitive, interdependent peripheral semantics.
   - ev: https://arxiv.org/abs/2608.29737
   - ev: https://arxiv.org/html/2608.29737v1 (Table 5; implementation section: 'We implement Bluezz on top of Fuzzware'; quotes on SVD parsing and the agent-assisted workflow)

36. (medium) [LOAD-BEARING] [Fidelity ceiling of automated approaches] Published automated peripheral-modeling systems measure success as firmware making progress: unit-test pass rates (P2IM 79% of samples, uEmu 93%, Perry 74.24%, FlexEmu 98.48%), coverage, and bugs found. None reports cycle or timing accuracy or bit-exact agreement with silicon. MPLAB SIM is likewise discrete-event and timing-approximate, and Arm FVPs are programmer's-view models 'not performance accurate'.
   detail: This is a synthesis across sources. A Microchip 'digital twin' should state its fidelity level explicitly (register-accurate functional vs time-approximate vs cycle-accurate). It should also budget for silicon or RTL differential validation, which none of the prior art provides.
   - ev: Pretender paper Section 2 ('survivable execution')
   - ev: Perry, FlexEmu, P2IM and uEmu evaluation figures (URLs above)
   - ev: MPLAB SIM general limitations (onlinedocs URL above, via search snippet)
   - ev: https://veecle.ai/blog/arm-fvp-vs-qemu-renode (quotes Arm on PV models)

37. (high) [Renode relation to QEMU] Renode's CPU emulation library tlib is 'based on libqemu code and is LGPL licensed unless stated otherwise' (LGPL-2.1, credits Fabrice Bellard), while Renode itself is MIT and its peripherals are C#. Renode peripheral models therefore can't be used inside QEMU without rewriting. Since they are MIT-licensed, translating them into GPL-2.0-or-later QEMU code is license-compatible (standard MIT-to-GPL compatibility; not legal advice).
   - ev: https://github.com/antmicro/tlib (README; LICENSE LGPL 2.1)
   - ev: renode LICENSE:1-4 (MIT)
   - ev: renode-infrastructure file headers 'licensed under the MIT License'

## Table: Prior art
| name | type | Microchip relevance | input/approach | license | status 2025-26 | URL |
| QEMU microchip-icicle-kit | Upstream QEMU machine (RISC-V) | PolarFire SoC MSS (MMUART, sysreg, IOSCB, DMC; reuses SiFive PDMA, Cadence GEM/SDHCI) | Hand-written C; watchdog/SPI/I2C/CAN/USB/GPIO are unimp stubs | GPL-2.0-or-later | 'Odd Fixes'; HSS no longer boots (docs) | https://qemu.readthedocs.io/en/master/system/riscv/microchip-icicle-kit.html |
| QEMU emcraft-sf2 (msf2-soc) | Upstream QEMU machine (Cortex-M3) | Microsemi/Microchip SmartFusion2 M2S010 | Hand-written C: sysreg, timer, SPI, UART, EMAC; rest stubs | MIT-style file / QEMU GPL | Maintained | https://qemu.readthedocs.io/en/master/system/arm/emcraft-sf2.html |
| QEMU AVR / Arduino boards | Upstream QEMU machine (AVR) | ATmega168/328P/1280/2560 | CPU + USART + 16-bit timer only; TWI/ADC/SPI/WDT/EEPROM stubs | GPL-2.0-or-later | Maintained | https://qemu.readthedocs.io/en/master/system/target-avr.html |
| sergev/qemu (PIC32) | QEMU fork | PIC32MX7, PIC32MZ boards (Explorer-16, MEB-II, WiFire, Max32) | Hand-written C: UART, SPI/SD, Ethernet, GPIO, hex loader | QEMU GPL (NOASSERTION on GitHub) | Based on QEMU 2.3.50; not upstreamed (2015 v3 series); sporadic 2025 commits | https://github.com/sergev/qemu |
| ericsonj/qemu-pic32mk | QEMU board (community, 2026) | PIC32MK1024MCM100 | Datasheet-driven C models (UART/SPI/I2C stub/CAN FD/ADC/USB CDC...); pytest plugin; CLAUDE.md shows AI assistance | QEMU GPL (NOASSERTION) | Created Apr 2026 on QEMU 10.2.50; unvalidated | https://github.com/ericsonj/qemu-pic32mk |
| quarkslab/sstic-tame-the-qemu | QEMU board (custom) | ATSAMD21G18A | Hand-written C: GPIO (PORT) + SERCOM | none stated | QEMU 8.2; frozen since 2024-06 | https://github.com/quarkslab/sstic-tame-the-qemu |
| Renode (+ renode-infrastructure) | Simulation framework (tlib from libqemu; C# models) | PolarFire SoC/Icicle/BeagleV-Fire, Mi-V, SAM4S, SAM E70, SAMD21, SAMD51 (+ ~25 Microchip dts2repl overlays incl. PIC32CM JH, PIC32CZ, SAME54, SAMV71) | C# register framework, Python stubs, Tags, ApplySVD, .repl; Verilator/DPI and SystemC co-sim; PeakRDL-renode and dts2repl generators | MIT (tlib LGPL-2.1) | v1.17.0 (2026-09-07); very active; official for PolarFire via SoftConsole | https://github.com/renode/renode |
| Wokwi (avr8js, Custom Chips, CI) | Commercial online simulator + MIT core | ATmega328P, ATmega2560, ATtiny85 only | JS/TS per-peripheral models with unit specs; Custom Chips in C/Rust to WASM (Verilog experimental) | avr8js MIT; service commercial | Active (avr8js pushed 2026-08); CI quotas; MCP in CLI | https://docs.wokwi.com |
| simavr | Instruction-level AVR simulator | Most megaAVR/tinyAVR | Cores from avr-libc device headers; IRQ 'parts'; VCD; gdb | GPL-3.0 | Active (pushed 2026-09) | https://github.com/buserror/simavr |
| gpsim | PIC simulator | PIC10/12/16/18 | C++ models + stimuli + loadable modules | GPL-2.0+ (some LGPL) | Last release 0.32.1 (2023-11) | https://gpsim.sourceforge.net/ |
| Proteus VSM | Commercial schematic + MCU co-simulation | PIC10-PIC24/dsPIC33, AVR, some SAM3N | Vendor-written CPU + peripheral models with SPICE-level board | Commercial | Active product | https://www.labcenter.com/simulation/ |
| MPLAB SIM (MPLAB X simulator) | Microchip's own simulator | PIC, AVR, SAM / PIC32C (pic32sim engine for PIC32CM JH, SAMD21, SAME54, PIC32CK) | Discrete-event; per-device peripheral support list; SCL stimulus | Proprietary (free with MPLAB X) | Shipping; SAM support partial ('beta' warnings in 2021 report) | https://developerhelp.microchip.com/xwiki/bin/view/software-tools/x/debugging/simulator/ |
| xPack QEMU Arm / GNU MCU Eclipse QEMU | QEMU distribution / legacy fork | None (STM32 boards); template for SVD-driven peripherals | Legacy: SVD->JSON->generated QEMU code (xsvd) + graphical boards | QEMU GPL; xPack scripts MIT | Legacy fork (QEMU 2.8.0) removed in 9.2.4-1 (2025-10-09) | https://xpack-dev-tools.github.io/qemu-arm-xpack/ |
| Espressif QEMU fork | Vendor-maintained QEMU fork | Peer precedent (ESP32/S3/C3/C6) | Hand-written C SoC models, shared IP models; integrated in ESP-IDF (idf.py qemu) | QEMU GPL | Active; 9.2.2-based releases (2026-04) | https://github.com/espressif/qemu |
| Nordic nRF HW models + BabbleSim | Vendor-authored native peripheral models | Peer precedent (nRF52833/5340/54L) | C models linked with firmware compiled for host (Zephyr *_bsim) | Apache-2.0 | Active (2026-09) | https://github.com/BabbleSim/ext_nRF_hw_models |
| Arm FVP / Arm Virtual Hardware (Corellium) | Commercial/free FVPs + cloud boards | None (no Microchip boards on AVH) | PV-level Fast Models; vendor boards by Corellium | Arm EULA / commercial | FVPs DS-only since 11.29 (2025); AVH via Corellium | https://learn.arm.com/install-guides/fm_fvp/fvp |
| Synopsys Virtualizer VDKs | Commercial virtual prototypes | None found | SystemC/TLM VDKs via vendor Centers of Excellence (Infineon, Renesas) | Commercial | Infineon RISC-V MCU prototype shown Mar 2025 | https://www.eenewseurope.com/en/infineon-synopsys-to-show-virtual-prototype-of-first-automotive-risc-v-microcontroller/ |
| Siemens PAVE360 / Innexis; Cadence Helium | Commercial SoC/system digital twins | None found | Multi-fidelity virtual/hybrid/RTL platforms (Arm Zena CSS) | Commercial | 2025 announcements | https://blogs.sw.siemens.com/pave360/2025/06/04/siemens-and-arm-work-together-to-build-digital-twin-solutions-for-ai-defined-vehicle/ |
| Intel/Wind River Simics + DML | Full-system simulator + device DSL | None found | DML register-bank DSL compiled to C (Simics backend only) | Simics: free-under-EULA / commercial; DML MPL-2.0 | Simics 7 (2025); DML active 2026 | https://github.com/intel/device-modeling-language |
| Lauterbach TRACE32 ISS / TASKING simulator | Commercial debugger simulators | Generic | User peripheral-model API (TRACE32); TASKING wraps Infineon TSIM | Commercial | Active | https://www.lauterbach.com/sim.html |
| QBox (Qualcomm/GreenSocs) | SystemC TLM-2.0 + QEMU co-sim | Generic (Cortex-M7/M55 supported) | libqemu-cxx over Qualcomm QEMU fork (libqemu-v11.0) | BSD-3-Clause | v7.4.0 (2026-10-03) | https://github.com/qualcomm/qbox |
| AMD/Xilinx libsystemctlm-soc + remote-port | SystemC/TLM/RTL co-sim with QEMU | Generic | Socket-based remote-port in Xilinx QEMU fork; Verilator RTL bridges | MIT (lib); QEMU fork GPL | Active (fork at 10.2.4) | https://github.com/Xilinx/libsystemctlm-soc |
| MachineWare VCML; MINRES SCC/TGC-VP | SystemC/TLM model libraries | Generic | Reusable TLM peripherals, register components | Apache-2.0 | Active (2026-10) | https://github.com/machineware-gmbh/vcml |

## Table: Rehosting research
| system | venue/year | model source | fidelity | borrowable idea |
| Avatar2 | BAR (NDSS workshop) 2018 | Live hardware via OpenOCD/GDB/J-Link/Inception; QEMU/PANDA/Unicorn | Real silicon behavior, slow, needs board | Hardware-in-the-loop differential testing of QEMU models vs real PIC32CM/SAM boards; golden trace capture |
| Pretender | RAID 2019 | MMIO+IRQ traces recorded via avatar2 (STM32, Maxim boards); DBSCAN clustering; storage/pattern/increasing/write-only register models; IRQ timing | 'Survivable execution', approximate | Auto-classify unmodeled registers; trace-replay regression tests |
| Conware | AsiaCCS 2021 | Recordings of firmware-peripheral exchanges from vendor example code; automata; mergeable | Higher-fidelity automata for recorded behaviors only | Use Microchip's own MCC/Harmony examples on boards to record and validate model state machines |
| HALucinator | USENIX Security 2020 | HAL function interception with Python handlers (QEMU via avatar2); Atmel ASF v3 + SAMR21 support | HAL-level only (no registers) | Optional fast 'HAL twin' mode for Harmony/MCC APIs in application CI |
| P2IM | USENIX Security 2020 | Register categories from access patterns; fuzzer feeds data registers (QEMU 2.3.50 / GNU MCU Eclipse) | Enough to run 79% of 70 samples | Reuse P2IM unit-test corpus as smoke tests |
| uEmu | USENIX Security 2021 | S2E symbolic execution builds per-access-point knowledge base | 93% unit tests; path-oriented | Flag reads whose returned values the model cannot justify |
| Jetset | USENIX Security 2021 | Symbolic execution to reach a target; emits C device models for QEMU | Targeted (boot-to-point) | Auto-generated QEMU C for boot-critical registers; targeted bring-up |
| DICE | IEEE S&P 2021 | DMA channel identification from DMA controller pointer writes; P2IM + sergev PIC32 QEMU | 33/37 DMA channels, 0 FP | Model DMA/event paths explicitly (DMAC, EVSYS) |
| Fuzzware | USENIX Security 2022 | angr per-MMIO-access models (Passthrough/Constant/Set/Bitextract) on Unicorn | Coverage-oriented; 77 images / 19 platforms | Measure which register bits firmware actually consumes to prioritize model fidelity |
| SEmu | ACM CCS 2022 | NLP over reference-manual tables/text into condition-action rules; S2E RuleDoctor; SAM3X/SAM R21 manuals | Spec-guided; manual rule fixes needed | Behavior rules from Microchip's structured datasheet sources rather than PDF NLP |
| Hoedur | USENIX Security 2023 | Fuzzware models + multi-stream inputs; QEMU as library via Rust | Fuzzing-grade | Build QEMU as a library (libqemu-style) for test-harness control |
| Perry | USENIX Security 2024 | Driver source (LLVM bitcode) + SVD; KLEE/Z3; emits QEMU 7.2 models; Microchip ASF SAM4L/4E/4S/E70/V71/3X | 74.24% unit tests | Cross-check driver-derived behavior from Harmony/ASF against ATDF and silicon |
| FlexEmu | ACM CCS 2025 | LLM (Gemini 1.5 Flash) extracts 9-primitive JSON instances from driver source; per-class semantic models; QEMU backend; incl. Microchip SAM | 98.48% unit tests (15 MCUs) | Primitive DSL + JSON-schema-constrained LLM extraction; one template per IP class |
| RPM / Bluezz | arXiv 2026 (2608.29737) | SVD skeleton + ECA rules from reference manual; agent drafts rules, human validates; Fuzzware/Unicorn | Reactive/stateful (interrupt, DMA, inter-peripheral); weaker for timing-sensitive | Generated register skeleton + reviewed behavior-rule DSL; agent-assisted rule authoring (12.5 h -> ~2 h) |

## Table: Microchip parts: where public models exist today
| Microchip part/family | upstream QEMU | QEMU forks / boards | Renode | research / other |
| PIC32CM JH (Cortex-M0+) | none | none found | dts2repl overlay 'microchip,pic32cm' (clock spoofs, UNK tags) for Zephyr pic32cm_jh01_cpro | MPLAB SIM (pic32sim engine) |
| SAM D21 | none | Quarkslab SAMD21G18A (QEMU 8.2; GPIO+SERCOM) | atsamd21j17d-aft.repl (SAMD21_RTC/GPIO/Timer/I2C, SAMD5_UART) + Tags | MPLAB SIM (partial, 2021 beta warnings) |
| SAM D5x/E5x | none | none found | atsamd51g19a.repl (SAMD5_UART + Python stubs); same54 overlays with ApplySVD | MPLAB SIM (pic32sim) |
| SAM E70 / V71 / S70 | none | none found | sam_e70.repl (SAM_USART, CadenceGEM, SAM_TRNG, ApplySVD); samv71 overlays | Perry & FlexEmu generated QEMU models (ASF) |
| SAM4S / 4E / 4L / SAM3X | none | none found | sam4s*.repl (SAM4S_* models) | Perry/FlexEmu (QEMU); SEmu rules (SAM3X); HALucinator ASF (SAMR21) |
| PIC32CK / PIC32CZ / PIC32CX | none | none found | dts2repl overlays (pic32cz, pic32cx, pic32ck via Zephyr) | MPLAB SIM (pic32sim) |
| PIC32MX / PIC32MZ / PIC32MK (MIPS) | none | sergev/qemu (MX7/MZ, QEMU 2.3.50); qemu-pic32mk (MK, QEMU 10.2.50, 2026) | none | DICE used sergev fork |
| AVR ATmega/ATtiny | ATmega168/328P/1280/2560 (USART+timer16) | n/a | none relevant | Wokwi/avr8js, simavr, MPLAB SIM 'sim' engine |
| PolarFire SoC / Mi-V / SmartFusion2 | Icicle Kit (partial, HSS broken); emcraft-sf2 (partial) | n/a | polarfire-soc, mpfs-icicle-kit, beaglev-fire, miv (official via SoftConsole) | Microchip example repos ship Renode launch configs |
| PIC64GX / PIC64-HPSC | none | none found | none found | Zephyr board only (pic64gx_curiosity_kit) |

## Code: Renode ApplySVD: unimplemented register reads return the SVD reset value (named logging) (https://github.com/renode/renode-infrastructure src/Emulator/Main/Peripherals/Bus/SVDParser.cs:85-109 (HEAD 515cc63e, MIT))
```csharp
        public bool TryReadAccess(ulong offset, out ulong value, SysbusAccessWidth width)
        {
            var bytesToRead = CheckAndGetWidth(width);
            var weHaveIt = false;
            value = 0;

            if(HitInTheRegisterDictionary(out var tmpRegister, offset, bytesToRead))
            {
                weHaveIt = true;
                if(tmpRegister.HasReadAccess)
                {
                    value = tmpRegister.ResetValue;
                    LogReadSuccess(value, tmpRegister.Peripheral.Name, tmpRegister.Name, offset, width);
                }
                else
                {
                    LogReadFail(tmpRegister.Peripheral.Name, tmpRegister.Name, offset, width);
                }
            }
            else
            {
                value = AssembleValueFromRegisters(offset, width, ref weHaveIt);
            }
            return weHaveIt;
        }
```

## Code: QEMU unimplemented device: returns 0, logs only the offset (qemu hw/misc/unimp.c:21-29)
```c
static uint64_t unimp_read(void *opaque, hwaddr offset, unsigned size)
{
    UnimplementedDeviceState *s = UNIMPLEMENTED_DEVICE(opaque);

    qemu_log_mask(LOG_UNIMP, "%s: unimplemented device read  "
                  "(size %d, offset 0x%0*" HWADDR_PRIx ")\n",
                  s->name, size, s->offset_fmt_width, offset);
    return 0;
}
```

## Code: Renode SAM E70 platform: Python stub + constant Tag + ApplySVD (https://github.com/renode/renode platforms/cpus/sam_e70.repl:41-49 (HEAD 150e30d3))
```text
PMC_SR: Python.PythonPeripheral @ sysbus 0x400E0668
    size: 0x4
    initable: true
    filename: "scripts/pydev/flipflop.py"

sysbus:
    init:
        Tag <0x400E0940 0x4> "CIDR" 0xa1020e00
        ApplySVD @https://dl.antmicro.com/projects/renode/svd/ATSAME70Q21.svd
```

## Code: dts2repl PIC32CM overlay (applies to Zephyr pic32cm_jh01_cpro): guessed 'UNK' clock registers (https://github.com/antmicro/dts2repl dts2repl/overlay/microchip,pic32cm.repl:1-16 (ATDF: 0x4000100C=OSCCTRL.STATUS, 0x40001030=OSCCTRL.DPLLSTATUS, 0x4000140C=OSC32KCTRL.STATUS, 0x40001414=OSC32KCTRL.XOSC32K))
```text
sysbus:
    init add:
        Tag <0x4000100C 0x4> "UNK1" 0x11
        Tag <0x40001030 0x4> "UNK2" 0x03
        Tag <0x4000140C 0x4> "UNK3" 0x03
        Tag <0x40001414 0x4> "OSCCTRL_STATUS" 0x3

GCLK_SYNCCLOK: Python.PythonPeripheral @ sysbus 0x40001C04
    size: 0x4
    initable: true
    filename: "scripts/pydev/repeater.py"

GCLK_GENCTRL: Python.PythonPeripheral @ sysbus 0x40001C20
    size: 0x8
    initable: true
    filename: "scripts/pydev/repeater.py"
```

## Code: Renode SAMD5x SERCOM USART INTFLAG in the register framework (tagged = unimplemented bits) (https://github.com/renode/renode-infrastructure src/Emulator/Peripherals/Peripherals/UART/SAMD5_UART.cs:89-99 (MIT))
```csharp
            Registers.InterruptFlag.Define(this)
                .WithFlag(0, FieldMode.Read, name: "DRE - Data Register Empty", valueProviderCallback: _ => Count == 0)
                .WithFlag(1, out transmitComplete, FieldMode.Read | FieldMode.WriteOneToClear, name: "TXC - Transmit Complete")
                .WithFlag(2, out receiveComplete, FieldMode.Read, name: "RXC - Receive Complete")
                .WithFlag(3, out receiveStart, FieldMode.Read | FieldMode.WriteOneToClear, name: "RXS - Receive Start")
                .WithTaggedFlag("CTSIC - Clear To Send Input Change", 4)
                .WithTaggedFlag("RXBRK - Receive Break", 5)
                .WithReservedBits(6, 1)
                .WithTaggedFlag("ERROR", 7)
                .WithWriteCallback((_, __) => UpdateInterrupt())
            ;
```

## Code: simavr: AVR core instantiated from the vendor-derived avr-libc device header (https://github.com/buserror/simavr simavr/cores/sim_mega328.c:22-42 (GPL-3.0))
```c
#include "sim_avr.h"

#define SIM_VECTOR_SIZE	4
#define SIM_MMCU		"atmega328"
#define SIM_CORENAME	mcu_mega328

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom328p.h"
// instantiate the new core
#include "sim_megax8.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t mega328 = {
	.names = { "atmega328", "atmega328p" },
	.make = make
};
```

## Code: Upstream QEMU ATmega: most peripherals are unimplemented stubs (qemu hw/avr/atmega.c:376-381)
```c
    create_unimplemented_device("avr-twi",          OFFSET_DATA + 0x0b8, 6);
    create_unimplemented_device("avr-adc",          OFFSET_DATA + 0x078, 8);
    create_unimplemented_device("avr-ext-mem-ctrl", OFFSET_DATA + 0x074, 2);
    create_unimplemented_device("avr-watchdog",     OFFSET_DATA + 0x060, 1);
    create_unimplemented_device("avr-spi",          OFFSET_DATA + 0x04c, 3);
    create_unimplemented_device("avr-eeprom",       OFFSET_DATA + 0x03f, 3);
```

## Code: Upstream QEMU Icicle Kit docs: model drift (HSS no longer boots) (qemu docs/system/riscv/microchip-icicle-kit.rst:102-109)
```rst
Running HSS
-----------

The machine ``microchip-icicle-kit`` used to run the Hart Software Services
(HSS_), however, the HSS development progressed and the QEMU machine
implementation lacks behind.  Currently, running the HSS no longer works.
There is missing support in the clock and memory controller devices.  In
particular, reading from the SD card does not work.
```

## Open questions
- Which peripherals does MPLAB SIM's pic32sim engine actually model for PIC32CM JH01/JH00 today? The public 'Simulator Peripheral Support by Device' list and v5.40 PDF/CSV could not be fetched (microchip.com returned 403/503), so this needs checking in MPLAB X or the internal device database.
- Can MPLAB SIM's internal peripheral models (pic32sim, sim, dssim engines) be wrapped or exported, e.g., as a QEMU MMIO backend over a socket? None of their architecture is public.
- Does Microchip have internal SystemC/TLM or RTL (Verilog) models of SERCOM, TC/TCC, EIC, DMAC, EVSYS or OSCCTRL that could serve as a golden reference through Verilator, remote-port or QBox?
- Strategic fit with the existing Antmicro/Renode relationship (PolarFire SoC, Mi-V, SoftConsole): should the MCU twin be QEMU (as requested), Renode, or a shared behavior core targeting both? No public statement on Microchip MCU emulation strategy was found.
- Upstream vs vendor fork: upstream QEMU MCU models tend to drift or orphan (Icicle 'Odd Fixes' with HSS broken; MAX78000 'Orphan'), while Espressif keeps a fork. Is Microchip prepared to commit named maintainers?
- Licensing of generated code: can ATDF/SVD-derived register tables be emitted into GPL-2.0-or-later QEMU sources, and can MIT Renode models be used as behavioral references? Needs legal review; MIT-to-GPL is normally compatible.
- What fidelity target defines 'digital twin' here: register-accurate functional, time-approximate (QEMU icount), or cycle-accurate? Prior-art fidelity claims are firmware-progress metrics, not silicon agreement.
- Validation plan: whether to use avatar2-style hardware-in-the-loop trace capture on Curiosity boards, Conware-style recording of MCC/Harmony examples, or RTL co-simulation to verify models. None of the prior art validates against silicon.
- How the community qemu-pic32mk (2026) and the Perry/FlexEmu-generated SAM models behave on real firmware. Their fidelity is self-reported and unverified.
- Status and coverage of the Renode Zephyr dashboard for Microchip boards (pic32cm_jh01_cpro/cnano, sam_e54_xpro, PIC32CZ/CK). The dashboard is JS-rendered and could not be read.
