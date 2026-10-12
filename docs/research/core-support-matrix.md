# core-support-matrix

## Summary
Checked against QEMU master 11.1.50 (commit 602e4f2da1, 2026-10-09). target/arm registers exactly seven M-profile CPUs: cortex-m0, m3, m4, m7, m33, m55 and m85. There is no cortex-m0plus and no cortex-m23. Every Microchip Cortex-M0+ part (SAM D1x/D2x/DA1, SAM C2x, SAM L2x, PIC32CM JH/MC/GV/PL10) therefore has to run on cortex-m0 as a stand-in. That fits well: the armv7m container and NVIC always provide VTOR, offer an optional Armv6-M MPU (the mpu-ns-regions property is passed through as pmsav7-dregion), always create SysTick, default to 2 priority bits and allow up to 32 IRQs. This matches the PIC32CM5164JH01100 DFP configuration exactly (NUM_IRQ 32, MPU, VTOR, 2 priority bits). Three small but real M0+ gaps remain: CPUID reads as a Cortex-M0, unprivileged Thread mode is missing (M_UNPRIV is only set together with the Main Extension), and nothing in QEMU models the single-cycle IOBUS. The JH's PORT_IOBUS (0x60000000) and DIVAS_IOBUS (0x60000200) can still be modelled as memory aliases. Cortex-M23 parts need a new CPU type, and the instruction-decode gating makes that non-trivial; these are PIC32CM LE00 (which is M23, not M0+), LS00/LS60 and SAM L10/L11. All M3/M4F/M7/M33 families are already covered at the CPU level: SAM3/4/G5x/D5x/E5x/E70/V71, PIC32CX, PIC32CZ, PIC32CK and SmartFusion2. The AVR target registers only avr5/avr51/avr6 and hard-codes the classic register-file-in-data-space layout, so AVR-Dx, tinyAVR 0/1/2, megaAVR 0 and XMEGA need CPU-target work. The MIPS target lacks M4K, microAptiv and M5150 models, the MCU ASE and shadow-register-set switching on interrupts, and has only basic EIC (external interrupt controller) support, so every PIC32 MIPS family needs CPU work. There is no PIC8, PIC24 or dsPIC target at all (QEMU has 19 target architectures). Upstream Microchip-lineage models are limited to: AVR ATmega Arduino machines (USART, 16-bit timer and power-reduction only; Maintained); SmartFusion2/emcraft-sf2 (Maintained); and the PolarFire SoC Icicle Kit (Odd Fixes; HSS no longer boots; GPIO/SPI/I2C/CAN/WDT/USB are stubs; no tests). Reusable off-chip parts exist (AT24C EEPROM, Atmel/SST/Microchip 25CSM04 SPI NOR, SMSC LAN9118/LAN91C111, EMC1413/1414), but nothing SAM- or PIC32-related is upstream. On the software side, Microchip itself upstreamed Zephyr support for PIC32CM JH (PR #95480, merged 2025-09-13) using "G1" driver bindings that the new Zephyr SAM E54 board shares, while SAM D21 has the largest hobby test corpus. Recommendation: start with PIC32CM JH01 (PIC32CM5164JH01100 on the Curiosity Pro EV81X90A), use SAM D21 as the community-facing second target and SAM E54/PIC32CK as the M4F/M33 step, and defer AVR-Dx, PIC32 MIPS and PolarFire.

## Findings

1. (high) [LOAD-BEARING] [M-profile CPU inventory] QEMU master registers exactly seven M-profile CPU types: cortex-m0, cortex-m3, cortex-m4, cortex-m7, cortex-m33, cortex-m55 and cortex-m85. No cortex-m0plus, cortex-m1 or cortex-m23 type exists anywhere in target/, hw/ or docs/, and no qemu-devel series adding M0+ or M23 was found.
   detail: Microchip's Cortex-M0+ parts must use cortex-m0. Cortex-M23 parts have no model.
   - ev: target/arm/tcg/cpu-v7m.c:325-340 (arm_v7m_cpus[] table)
   - ev: grep -rniE 'cortex-?m0plus|cortex-?m0\+|cortex-?m23|cortex_m23|m0plus' over the checkout: no matches
   - ev: https://patchew.org/QEMU/20180803144721.17036-1-stefanha@redhat.com (2018 series that added Cortex-M0 for micro:bit; web search found no M0+/M23 series)

2. (high) [LOAD-BEARING] [cortex-m0 as a Cortex-M0+ stand-in: VTOR] The NVIC implements VTOR (0xE000ED08) for every M-profile CPU with no feature gate, masked to 0xFFFFFF80. Reset loads SP/PC from vecbase, which comes from the CPU's init-nsvtor property, and that property exists on all M CPUs. QEMU's cortex-m0 therefore behaves like an M0+ built with VTOR; a real Cortex-M0 has no VTOR. PIC32CM JH sets __VTOR_PRESENT=1, and CMSIS core_cm0plus.h only exposes SCB->VTOR when that macro is 1, so JH firmware and bootloaders will use it.
   - ev: hw/intc/armv7m_nvic.c:1109-1110 (read)
   - ev: hw/intc/armv7m_nvic.c:1638-1640 (write)
   - ev: target/arm/cpu.c:552-580
   - ev: target/arm/cpu.c:1692-1700
   - ev: hw/arm/armv7m.c:319-323, 559
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:55
   - ev: DFP PIC32CM-JH01/include/pic32cm5164jh01100.h:241
   - ev: DFP PIC32CM-JH01/svd/PIC32CM5164JH01100.svd:33
   - ev: CMSIS 6.1.0 CMSIS/Core/Include/core_cm0plus.h:146-148, 360-361

3. (high) [LOAD-BEARING] [cortex-m0 as a Cortex-M0+ stand-in: MPU] cortex-m0 has no MPU by default because its initfn never sets pmsav7_dregion (cortex-m3 sets 8). However, every M CPU gets ARM_FEATURE_PMSA and the pmsav7-dregion property. A source comment says QEMU's PMSAv7 state also models the Armv6-M MPU layout. The NVIC masks RASR to the v6-M fields (no TEX) and hides the RBAR/RASR alias registers for v6-M. The armv7m container passes its mpu-ns-regions property through to pmsav7-dregion. An M0+ MPU (JH: __MPU_PRESENT=1) can therefore be enabled from SoC code without changing the CPU.
   detail: Based on reading the code, not on running it. An M0+ MPU has 8 regions when present (Arm TRM; the DFP only says it is present).
   - ev: target/arm/tcg/cpu-v7m.c:45-77 (no pmsav7_dregion) vs :87 (m3 = 8)
   - ev: target/arm/cpu.c:1466-1468, 1667-1677, 2342-2372
   - ev: hw/intc/armv7m_nvic.c:1385-1391, 1998-2004
   - ev: hw/arm/armv7m.c:349-355, 565-566
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:53
   - ev: DFP Microchip.PIC32CM-JH_DFP.pdsc:43-46 (Dmpu="MPU")

4. (high) [LOAD-BEARING] [cortex-m0: NVIC limits, SysTick, bit-band, alignment] For v6-M the NVIC rejects more than 32 external IRQs and defaults to 2 priority bits. armv7m always creates SysTick (cpuclk is mandatory, refclk optional) and leaves bit-banding off unless enable-bitband is set. CPUs without the Main Extension force CCR.UNALIGN_TRP. PIC32CM5164JH01100 has NUM_IRQ=32 (PERIPH_MAX_IRQn=31), 2 priority bits and SysTick, so it sits exactly at the v6-M NVIC limit.
   - ev: hw/intc/armv7m_nvic.c:2758-2761, 2768-2773
   - ev: hw/arm/armv7m.c:292-296, 463-473, 498-503, 560
   - ev: target/arm/cpu.c:537-540
   - ev: hw/arm/nrf51_soc.c:80-84 (TODO: a SoC without SysTick can't be modelled)
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:51, 54, 56, 1432
   - ev: DFP PIC32CM-JH01/include/pic32cm5164jh01100.h:127, 240

5. (medium) [LOAD-BEARING] [Remaining CPU-level gaps between QEMU cortex-m0 and a real M0+] (1) CPUID reads 0x410CC200 (Cortex-M0 r0p0), not an M0+ PARTNO 0xC60 value. The armv7m container does not pass the CPU's midr property through, because it creates the CPU inside its own realize step. Fixing this needs a patch, a new CPU type, or a -global/compat property (the -global route is untested). (2) ARM_FEATURE_M_UNPRIV is only implied by M_MAIN, so on cortex-m0 writes to CONTROL.nPRIV are ignored and there is no unprivileged Thread mode. That breaks MPU-based user/kernel isolation (e.g. Zephyr userspace, FreeRTOS-MPU), if the JH core implements the unprivileged extension.
   detail: The code facts are high confidence. Medium overall because the M0+ PARTNO and the unprivileged-extension option come from background Arm TRM knowledge, and the DFP does not say whether JH implements unprivileged mode.
   - ev: target/arm/tcg/cpu-v7m.c:52
   - ev: target/arm/cpu.c:2504 (midr property)
   - ev: hw/arm/armv7m.c:300-301, 553-567 (no midr alias)
   - ev: target/arm/cpu.c:1470-1472
   - ev: target/arm/tcg/m_helper.c:2778-2781
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:52 (__CM0P_REV 0x0001) vs PIC32CM-JH01/svd/PIC32CM5164JH01100.svd:30 (r0p0)

6. (medium) [LOAD-BEARING] [Cortex-M0+ single-cycle IOBUS] QEMU models no M0+ single-cycle I/O port (no IOBUS/SIO model and no RP2040). PIC32CM JH maps PORT at 0x41000000 and again as PORT_IOBUS at 0x60000000, and DIVAS at 0x48000000 and again as DIVAS_IOBUS at 0x60000200. QEMU does not model bus timing, so the IOBUS view can be a memory-region alias of the same device MMIO. Precedent: the msf2 eNVM alias. Ideally the alias is mapped only into the CPU's armv7m 'memory' container so DMA cannot reach it, as on real silicon.
   detail: That no IOBUS model exists is high confidence. The alias approach is analysis, not tested.
   - ev: grep -rwi 'iobus|io_bus|single-cycle' in hw/include/target/arm/docs: no relevant matches; grep 'rp2040|rp2350': none
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:252-253, 655-656
   - ev: DFP PIC32CM-JH01/include/pic32cm5164jh01100.h:475-476, 502-503
   - ev: hw/arm/msf2-soc.c:125-129
   - ev: hw/arm/armv7m.c:298, 555-556

7. (high) [How the only existing cortex-m0 board is built] The only cortex-m0 user is the nRF51 SoC behind the 'microbit' machine. It creates a TYPE_ARMV7M child named 'armv6m' with cpu-type cortex-m0 and num-irq 32, feeds a fixed 16 MHz sysclk into cpuclk and leaves refclk unconnected. Its peripherals have qtests (UART, GPIO, GPIO detect, NVMC, timer, I2C). The nRF51822 is a genuine Cortex-M0, so no upstream board exercises M0+-only options (v6-M MPU, IOBUS).
   - ev: hw/arm/nrf51_soc.c:186-191, 73-79
   - ev: hw/arm/microbit.c:40-67
   - ev: tests/qtest/microbit-test.c:545-550
   - ev: docs/system/arm/nrf.rst:21
   - ev: grep ARM_CPU_TYPE_NAME("cortex-m0") in hw/: only hw/arm/nrf51_soc.c:190

8. (medium) [LOAD-BEARING] [Cortex-M23 families (PIC32CM LE/LS, SAM L10/L11)] Microchip's Cortex-M23 parts have no QEMU CPU type. Keil lists PIC32CM LE00 as Cortex-M23 with an MPU and no TrustZone, so the task's premise that LE is M0+ is wrong. LS00/LS60 and SAM L11 are M23 with TrustZone. Adding cortex-m23 is more than writing an initfn: the ARMv6-M 32-bit Thumb whitelist only applies to M CPUs without ARM_FEATURE_V7, and any V8 M-profile CPU automatically gets V7 and THUMB2. A Baseline core would therefore execute Main-Extension Thumb-2 encodings, apart from those explicitly gated on M_MAIN such as IT, instead of faulting on them.
   detail: Correct v8-M Baseline firmware would most likely run. The difference shows up only in UNDEF/fault behaviour, but upstream acceptance would need correct gating.
   - ev: target/arm/tcg/translate.c:6263-6288
   - ev: target/arm/cpu.c:1474-1476, 1509-1516
   - ev: target/arm/tcg/translate.c:6082-6085
   - ev: hw/intc/armv7m_nvic.c:1389 ('not present for v6M or v8M without Main')
   - ev: https://www.keil.arm.com/devices/microchip-pic32cm2532le00100/processors
   - ev: https://www.keil.arm.com/devices/microchip-atsaml11e16a/processors/
   - ev: https://onlinedocs.microchip.com/oxy/GUID-51879300-6673-4E10-9215-46AA47325161-en-US-12/GUID-55F3998C-3BC4-44D3-A293-4CA417E01271.html (search snippet: LE00/LS00/LS60 implement Cortex-M23; the page itself returned HTTP 503)

9. (high) [LOAD-BEARING] [Arm Cortex-M3/M4/M7/M33 Microchip families] Existing CPU types cover all of these at the CPU level: SAM3 (M3), SAM4 (M4; SAM4S has no FPU), SAM G5x (M4F), SAM D5x/E5x (M4F, single-precision FPU), PIC32CX SG41/SG60/SG61, MT and BZ2/BZ3 (M4F; MTC/MTSH are dual-core), SAM E70/S70/V70/V71 and PIC32CZ CA70/MC70/CA80/CA90 (M7 with double-precision FPU), PIC32CK SG/GC (M33 with TrustZone, FPU and DSP), and SmartFusion2 (M3). QEMU's cortex-m4 has a single-precision FPU (MVFR0 0x10110021) and cortex-m7 a double-precision FPU (0x10110221). cortex-m33 has M_SECURITY, DSP, FPU, 16 MPU regions and 8 SAU regions. armv7m exposes vfp/dsp switches for parts without them.
   - ev: target/arm/tcg/cpu-v7m.c:105-200
   - ev: hw/arm/armv7m.c:325-334, 563-564
   - ev: https://www.keil.arm.com/devices/microchip-atsame54p20a/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamv71q21b/processors/
   - ev: https://www.keil.arm.com/devices/microchip-pic32ck2051sg01144/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamg55j19/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsam4s16c/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsam3x8e/processors/
   - ev: https://onlinedocs.microchip.com/oxy/GUID-62CB236E-26C5-4CE9-A182-B1D33FE1512C-en-US-3/GUID-B4415432-AE27-42AB-BAB1-48E787760125.html (PIC32CX SG: M4F)
   - ev: https://www.mouser.com/new/microchip/microchip-pic32cz-ca70-mc70-microcontrollers (M7, DP FPU, 300 MHz)
   - ev: https://www.microchip.com/en-us/solutions/industrial/smart-energy-metering/metering (PIC32CXMT)
   - ev: https://www.microchip.com/content/dam/mchp/documents/WSG/ProductDocuments/Brochures/PIC32-BZ2-and-PIC32-BZ3-Families-DS00006303.pdf
   - ev: https://www.microchip.com/content/dam/mchp/documents/FPGA/ProductDocuments/ProductBrief/SmartFusion2-SoC-FPGA-Product-Brief.pdf

10. (high) [LOAD-BEARING] [Cortex-M0+ Microchip families] The Cortex-M0+ families are: SAM D10/D11/D20/D21/DA1 (no MPU), SAM C20/C21 (C21 has an MPU), SAM L21 (no MPU) and L22 (MPU), PIC32CM JH00/JH01 (MPU, VTOR), PIC32CM MC00 (MPU), PIC32CM GV (M0+; one SEGGER device page says M0), and PIC32CM PL10 (M0+, 1.8-5.5 V, pin-compatible with AVR Dx, launched January 2026). Each can only be emulated via cortex-m0 plus SoC configuration.
   - ev: https://www.keil.arm.com/devices/microchip-atsamd11d14am/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamd20j18/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamd21j18a/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamda1j16b/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsamc21j18a/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsaml21j18b/processors/
   - ev: https://www.keil.arm.com/devices/microchip-atsaml22n18a/processors/
   - ev: https://www.keil.arm.com/devices/microchip-pic32cm1216mc00048/processors/
   - ev: DFP Microchip.PIC32CM-JH_DFP.pdsc:43-46; DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:26
   - ev: https://kb.segger.com/Microchip_PIC32CM_GV
   - ev: https://ir.microchip.com/news-events/press-releases/detail/1360/pic32cm-pl10-mcus-expand-microchips-arm-cortex-m0-portfolio

11. (medium) [Microchip ARM MPUs] SAM9X60/SAM9X7 (ARM926EJ-S) map to QEMU's existing arm926, and SAMA7G5/SAMA7D65 (Cortex-A7) to cortex-a7. SAMA5D2/D3/D4 (Cortex-A5) have no matching model because QEMU has no cortex-a5; the nearest are cortex-a7/a9. No Microchip MPU machine exists upstream.
   - ev: target/arm/tcg/cpu32.c:722-744
   - ev: grep '"cortex-a5"|cortex_a5_initfn' in target/arm and hw: no matches
   - ev: https://www.nasdaq.com/press-release/introducing-sama7d65-microprocessors-available-system-package-and-system-chip
   - ev: https://www.macnica.co.jp/en/business/semiconductor/manufacturers/microchip/products/142522/

12. (medium) [LOAD-BEARING] [AVR target capability (classic vs AVRxt/XMEGA)] target/avr registers only three CPU types: avr5, avr51 and avr6. docs/system/target-avr.rst lists 17 cores, so the docs are stale. The classic data-space layout is hard-coded: 32 bytes of register file at data 0x00, RAMPD/X/Y/Z, EIND, SP and SREG at data 0x58-0x5F, any store to 0-31 writes registers, and IN/OUT add 0x20 (NUMBER_OF_CPU_REGISTERS). AVR Dx, EA, EB and DU, tinyAVR 0/1/2, megaAVR 0 and XMEGA use the AVRxt/AVRxm cores: their register file is in a separate address space, I/O 0x00-0x3F maps 1:1 to data space, and flash is mapped into data space (AVR128DA SRAM sits at 0x4000-0x7FFF). Those parts therefore need new CPU types and a different data-space model, not just new peripherals. AVR_FEATURE_RMW/DES flags exist but no xmega CPU type uses them.
   detail: The QEMU code facts are high confidence. The AVR-Dx memory-map facts come from Microchip onlinedocs search snippets, because the pages returned 503.
   - ev: target/avr/cpu.c:417-419
   - ev: docs/system/target-avr.rst:7-9
   - ev: target/avr/cpu.c:144-155
   - ev: target/avr/helper.c:200-207, 281-284, 287-320
   - ev: target/avr/translate.c:1510-1519, 2135-2153
   - ev: target/avr/cpu.h:50-58, 70-77, 90-91
   - ev: https://onlinedocs.microchip.com/oxy/GUID-0B644D8F-67E7-49E6-82C9-1B2B9ABE6A0D-en-US-23/GUID-BA59618D-4850-490B-B176-0BCC3D9438A1.html (AVRxt instruction-set version)
   - ev: https://onlinedocs.microchip.com/oxy/GUID-BD668D46-A7E4-40CA-8B1D-4EB24BAA33C3-en-US-4/GUID-B0DFF21A-6168-4287-9E8A-3C574A09BD54.html (register file in a separate address space; search snippet, page returned 503)
   - ev: https://onlinedocs.microchip.com/oxy/GUID-3EE676DF-490E-41BC-98F0-5774B35DC989-en-US-19/GUID-0F7A9234-87AD-41E2-A6BD-4A570C291C94.html (AVR128DA SRAM 0x4000-0x7FFF)

13. (high) [LOAD-BEARING] [hw/avr inventory and maintenance] Upstream AVR machines are: an ATmega MCU container (ATmega168, 328, 1280 and 2560) and four Arduino machines (arduino-duemilanove, arduino-uno, arduino-mega, arduino-mega-2560-v3). Only three peripherals are real models: USART, the 16-bit timer (PWM, external clock and output-compare pins unsupported) and power reduction. GPIO ports, 8-bit timers, TWI, ADC, external-memory control, watchdog, SPI and EEPROM are all create_unimplemented_device stubs. MAINTAINERS: 'AVR TCG CPUs' and 'AVR MCUs' are Maintained by Michael Rolnik (gmail); 'Arduino' is Maintained by Philippe Mathieu-Daudé (oss.qualcomm.com). Functional tests cover the Uno and the Mega2560 (FreeRTOS).
   - ev: hw/avr/atmega.h:20-24
   - ev: hw/avr/atmega.c:308-318, 340-381, 399-455
   - ev: hw/avr/arduino.c:81-146
   - ev: hw/timer/avr_timer16.c:146, 218, 389
   - ev: MAINTAINERS:240-246, 1318-1328, 1330-1334
   - ev: tests/functional/avr/test_uno.py, tests/functional/avr/test_mega2560.py

14. (medium) [LOAD-BEARING] [MIPS CPU models vs PIC32 MIPS cores] target/mips defines 4Kc, 4Km, 4KEcR1, XBurstR1, 4KEmR1, 4KEc, 4KEm, 24Kc, 24KEc, 24Kf, 34Kf, 74Kf, XBurstR2, M14K, M14Kc, P5600, mips32r6-generic, I7200, R4000, VR5432, 5Kc, 5Kf, 20Kc, MIPS64R2-generic, 5KEc, 5KEf, I6400, I6500, Loongson-2E/2F/3A1000/3A4000, mips64dspr2 and Octeon68XX. M4K, microAptiv and M5150 are absent. The closest matches are 4KEm for M4K (r2, MIPS16, FMT MMU), M14K/M14Kc for microAptiv UC/MPU (r2 plus microMIPS, but no DSP), and P5600 as the only MIPS32r5 model (a different core). The MCU ASE is missing: IRET is only an enum and there is no ASE_MCU flag. EIC mode only feeds the vector number through Cause.IP times IntCtl.VS; there are no per-vector offsets and no automatic shadow-register-set switch on exception entry (SRSCtl is only used by RDPGPR/WRPGPR via PSS). mips_gic says 'EIC isn't supported'. Microchip sources: PIC32MX = M4K (MIPS32r2, MIPS16e, FMT); PIC32MZ EC = microAptiv; PIC32MZ EF = M5150 (MIPS32r5, FPU); PIC32MZ DA = microAptiv; PIC32MK = microAptiv with FPU; PIC32MM = microAptiv UC.
   detail: PIC32 interrupt handlers compiled with an IPLnSRS attribute rely on the hardware switching to a shadow register set. QEMU does not do that switch on exception entry, so such handlers would corrupt registers until it is implemented. This is an inference from the code and was not tested.
   - ev: target/mips/cpu-defs.c.inc:59-996 (.name entries)
   - ev: target/mips/cpu-defs.c.inc:182-200, 373-395, 397-420, 427-466
   - ev: target/mips/tcg/micromips_translate.c.inc:295
   - ev: target/mips/mips-defs.h:28-54
   - ev: target/mips/tcg/system/tlb_helper.c:1142-1172
   - ev: target/mips/tcg/translate.c:1243-1270
   - ev: target/mips/cpu-defs.c.inc:299 (only 34Kf sets SRSCtl.HSS)
   - ev: hw/intc/mips_gic.c:220
   - ev: https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/PIC32/mx-arch-cpu-overview/ (search snippet)
   - ev: https://skills.microchip.com/pic32mz-core-architecture/698794
   - ev: https://nuttx.apache.org/docs/latest/platforms/mips/pic32mz/index.html
   - ev: https://www.microchip.com/content/dam/mchp/documents/MCU32/ProductDocuments/DataSheets/PIC32MZ-DA-Family-DataSheet-DS60001565C.pdf
   - ev: https://www.microchip.com/content/dam/mchp/documents/MCU32/ProductDocuments/DataSheets/PIC32MK_GP_MC_Familly_Datasheet_60001402G.pdf

15. (high) [LOAD-BEARING] [No 8-bit or 16-bit PIC / dsPIC target] QEMU has 19 target architectures (alpha, arm, avr, hppa, i386, loongarch, m68k, microblaze, mips, or1k, ppc, riscv, rx, s390x, sh4, sparc, tricore, xtensa, hexagon). None covers PIC10/12/16/18, PIC24/dsPIC30/33 or the 32-bit dsPIC33A, which is a proprietary DSC core. A twin for those parts would need a brand-new TCG target; MPLAB SIM's dssim, pic18sim and pic16esim engines already cover them.
   - ev: target/Kconfig:1-19
   - ev: configs/targets/*-softmmu.mak (aarch64, alpha, arm, avr, hexagon, hppa, i386, loongarch64, m68k, microblaze, mips*, or1k, ppc*, riscv32/64, rx, s390x, sh4*, sparc*, tricore, x86_64, xtensa*)
   - ev: https://ir.microchip.com/news-events/press-releases/detail/1251/microchip-technology-introduces-a-new-core-in-the-dspic-digital-signal-controller-family-to-enhance-precision-and-execution-of-real-time-control
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu (986 dssim, 511 pic18sim, 367 pic16esim records)

16. (high) [SmartFusion2 model (Microsemi lineage)] msf2-soc.c models the SmartFusion2 MSS: cortex-m3 with 81 IRQs and bit-banding, eNVM ROM aliased at 0, eSRAM, two 16550 UARTs (serial_mm), the MSS timer, SYSREG, two MSS SPIs and the EMAC. It shows clock wiring: m3clk drives cpuclk, and refclk is fixed at /32 with a TODO for the SYSTICK_CR divider. I2C x2, DMA, WDT, GPIO, HS-DMA, CAN, RTC, APB config and USB are unimplemented stubs. The board is 'emcraft-sf2' (M2S010 SOM), tested by booting U-Boot. Status: Maintained by Subbaraya Sundeep (gmail) and Peter Maydell (Linaro). docs/system/arm/emcraft-sf2.rst wrongly calls the core a Cortex-M4; the code and Microchip both say Cortex-M3.
   - ev: hw/arm/msf2-soc.c:66-79, 106-140, 147-149, 213-222
   - ev: hw/arm/msf2-som.c:106
   - ev: tests/functional/arm/test_emcraft_sf2.py:25-44
   - ev: docs/system/arm/emcraft-sf2.rst:7
   - ev: MAINTAINERS:1240-1263
   - ev: https://www.microchip.com/content/dam/mchp/documents/FPGA/ProductDocuments/ProductBrief/SmartFusion2-SoC-FPGA-Product-Brief.pdf

17. (high) [LOAD-BEARING] [PolarFire SoC model (and PIC64GX)] microchip_pfsoc.c provides the 'microchip-icicle-kit' machine: E51 plus 4x U54 harts, ACLINT and PLIC, SiFive PDMA, two Cadence GEMs, Cadence SDHCI, five MMUARTs (a 16550 plus an extra register file), eNVM and QSPI-XIP ROMs, and register-level stubs for SYSREG, DDR PHY/CFG and IOSCB. Bus-error units, L2CC, AXISW, MPUCFG, FMETER, WDOG0-4, SPI0/1, I2C0/1, CAN0/1, USB, GPIO0-2 and the FPGA fabric (FIC0/1/3) are all unimplemented. The docs say HSS no longer boots because clock and memory-controller support is missing, and there is no test under tests/. Status: Odd Fixes, maintained by Conor Dooley <conor@kernel.org> and Sebastian Huber (embedded-brains.de). PIC64GX1000 uses the same cores (4x U54 + E51), but no PIC64GX machine exists.
   - ev: hw/riscv/microchip_pfsoc.c:87-140, 148-190, 235-489, 728-755
   - ev: hw/char/mchp_pfsoc_mmuart.c:30-106
   - ev: hw/misc/mchp_pfsoc_dmc.c:78-90, 161-173
   - ev: hw/misc/mchp_pfsoc_ioscb.c:247-312
   - ev: docs/system/riscv/microchip-icicle-kit.rst:107-108
   - ev: MAINTAINERS:1795-1810
   - ev: grep -rli 'icicle|pfsoc' tests/: no matches; grep 'pic64' in hw/include/docs: no matches
   - ev: https://www.cnx-software.com/2024/07/10/microchip-pic64gx-quad-core-64-bit-risc-v-microprocessor-real-time-processing/
   - ev: https://lists.infradead.org/pipermail/linux-riscv/2022-July/016951.html (Conor Dooley maintaining Linux PolarFire SoC support; his current affiliation is not verified)

18. (high) [Off-chip Microchip-family parts reusable for board-level twins] Upstream already models these parts: the AT24C-series I2C EEPROM (at24c-eeprom; no MAINTAINERS entry); SPI NOR/EEPROM in m25p80 for Atmel AT25FS/AT25DF/AT26/AT45DB, AT25128A/256A, SST25VF/25WF and Microchip 25CSM04 (SSI, Maintained by Alistair Francis); SMSC LAN9118 MAC+PHY and LAN91C111 (Network devices, Odd Fixes, Jason Wang); SMSC/Microchip EMC1413/EMC1414 temperature sensors (Overall sensors, Odd Fixes); 93C46-class Microwire EEPROM (Maintained by Stefan Weil); and SMSC FDC37M81x and FDC37C669 super-I/O chips. There are no MCP-prefixed parts, Micrel KSZ PHYs, Vitesse/Microsemi PHYs or SMSC USB hub/LAN95xx/78xx models, and nothing SAM- or PIC32-related. Name-search false positives: hw/ppc/sam460ex.c (AMCC 460EX), q35 MCHPCIState, cxl DSMSCIS, iotkit NSMSCEXP, and strongarm 'MCP'. The SAM E54 Xplained Pro's AT24MAC402 could reuse at24c-eeprom.
   - ev: hw/nvram/eeprom_at24c.c:30, 141-157, 254
   - ev: hw/block/m25p80.c:178-198, 312-321, 371-372
   - ev: hw/net/lan9118.c:1324; hw/net/lan9118_phy.c:214; include/hw/net/mii.h:117-119
   - ev: hw/net/smc91c111.c:2, 939
   - ev: hw/sensor/emc141x.c:2, 307-316
   - ev: hw/nvram/eeprom93xx.c:22-25
   - ev: include/hw/isa/superio.h:57-58
   - ev: MAINTAINERS:2403-2408, 2373-2377, 4085-4089, 2879-2883, 1499-1504, 728-732
   - ev: https://docs.zephyrproject.org/latest/boards/atmel/sam0/same54_xpro/doc/index.html (SERCOM7 to AT24MAC402)

19. (medium) [LOAD-BEARING] [Software ecosystem / test-firmware corpus] PIC32CM JH has upstream Zephyr boards pic32cm_jh01_cpro (SoC pic32cm5164jh01100) and pic32cm_jh01_cnano. They came from Microchip-authored PR #95480 (microchip.com sign-off), merged 2025-09-13. Drivers cover SERCOM UART/SPI/I2C, PORT GPIO and pinctrl, the clock tree (GCLK, MCLK, XOSC, FDPLL), NVMCTRL, TC/TCC, ADC, DMAC, WDT, RTC and ARMv6-M SysTick, using 'G1/G2' bindings. Zephyr's new boards/microchip/sam/sam_e54_xpro uses the same G1 bindings (sercom-g1, port-g1-gpio, tc-g1, tcc-g1, eic-g1, rtc-g1, wdt-g1, adc-g1), which is evidence of shared IP. SAM D21 has the widest coverage: Zephyr samd21_xpro, the official Arduino SAMD core (Zero/MKR/Nano 33 IoT), the CircuitPython atmel-samd port (SAMD21/SAMD51, plus a same54_xplained board), and TinyUSB (samd11/21/51, same5x/7x, samd2x_l2x, samg, sam3x). AVR Dx has the community DxCore Arduino core but no Zephyr AVR port was found. PolarFire and PIC64GX have Zephyr boards mpfs_icicle and pic64gx_curiosity_kit. PIC32CM JH has no USB peripheral, so TinyUSB and CircuitPython do not apply to it.
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic32c/pic32cm_jh01_cpro/doc/index.html
   - ev: https://github.com/zephyrproject-rtos/zephyr/pull/95480
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic32c/pic32cm_jh01_cnano/doc/index.html
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/sam/sam_e54_xpro/doc/index.html
   - ev: https://docs.zephyrproject.org/latest/boards/atmel/sam0/samd21_xpro/doc/index.html
   - ev: https://github.com/arduino/ArduinoCore-samd
   - ev: https://docs.circuitpython.org/en/9.2.x/ports/atmel-samd/README.html
   - ev: https://circuitpython.org/board/same54_xplained/
   - ev: https://raw.githubusercontent.com/hathach/tinyusb/master/docs/reference/dependencies.rst
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/mpfs_icicle/doc/index.html
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic64gx_curiosity_kit/doc/index.html
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf module list (no USB)

20. (high) [LOAD-BEARING] [PIC32CM JH DFP facts relevant to model reuse] JH00 and JH01 have identical peripheral instance lists except that JH01 adds CAN0/CAN1, which use the Bosch M_CAN IP (U2003); QEMU has no M_CAN model, only SJA1000, CTU CAN FD, Xilinx CAN/CANFD and FlexCAN. JH uses the newer clock IP (OSCCTRL, OSC32KCTRL, MCLK, and GCLK U2122 with GENCTRL[9] and PCHCTRL[43]) and has no SYSCTRL. SAM D21 uses the older SYSCTRL plus GCLK GENDIV/GENCTRL/CLKCTRL scheme, so clock models will not carry over between the two, while SERCOM/PORT/TC/TCC/EIC are the likely shared blocks. Microchip markets JH as the next generation of SAM C21. The installed DFP is 1.3.221, but the local pack index lists 1.7.296 as current.
   - ev: DFP PIC32CM-JH00/atdf/PIC32CM5164JH00100.atdf and PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf register-group lists
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf GCLK registers CTRLA/SYNCBUSY/GENCTRL(count 9 @0x20)/PCHCTRL(count 43 @0x80); 0 occurrences of SYSCTRL
   - ev: hw/net/can/ (can_sja1000, ctucan, xlnx-zynqmp-can, xlnx-versal-canfd, flexcan; no m_can)
   - ev: https://raw.githubusercontent.com/arduino/ArduinoCore-samd/master/cores/arduino/startup.c (SYSCTRL->DFLLCTRL, GCLK->CLKCTRL)
   - ev: https://www.microchip.com/en-us/products/microcontrollers/32-bit-mcus/pic32-sam/pic32cm-jh
   - ev: C:/Users/C19257/.mchp_packs/index.idx (Microchip.PIC32CM-JH_DFP.pdsc version 1.7.296)

21. (medium) [Out-of-tree precedents] Two out-of-tree QEMU projects already cover Microchip parts. Quarkslab's SSTIC 2024 article (Damien Cauquil) builds a SAMD21-based board on QEMU 8.2.0 by creating a TYPE_ARMV7M child with cpu-type cortex-m0 and num-irq 32 (section titled 'Let's add an ARM Cortex-M0+ CPU') and implements GPIO and SERCOM (USART/SPI); the code is at github.com/quarkslab/sstic-tame-the-qemu. Serge Vakulenko's 2015 QEMU fork adds PIC32 machines pic32mx7-explorer16/max32/maximite and pic32mz-explorer16/meb2/wifire on mipsel. Neither is upstream, and the code and licences were not inspected.
   - ev: https://www.sstic.org/media/SSTIC2024/SSTIC-actes/tame_the_qemu_debug_firmware_on_custom_emulated_bo/SSTIC2024-Article-tame_the_qemu_debug_firmware_on_custom_emulated_board-cauquil_gwqDg32.pdf (text extracted from PDF streams: listings 16/20)
   - ev: https://github.com/sergev/qemu/wiki

22. (high) [Upstream vendor-maintenance and test precedents] Silicon vendors do maintain SoC models upstream: Nuvoton NPCM7xx has a nuvoton.com maintainer with status 'Supported', ASPEED BMCs list aspeedtech.com reviewers, and ASPEED contributed a 2026 USB device-controller model (aspeed-udc.c). That file is the upstream pattern for a device-mode USB controller: a SysBus MMIO/IRQ/DMA face plus a USBDevice face, which would be relevant for SAM D21/E54 USB. Abandoned ports go Orphan (MAX78000FTHR). STM32L4x5 shows per-peripheral qtests (EXTI, GPIO, RCC, SYSCFG, USART).
   - ev: MAINTAINERS:1002-1006, 1265-1271, 904-906
   - ev: hw/usb/aspeed-udc.c:1-17
   - ev: tests/qtest/stm32l4x5_exti-test.c, stm32l4x5_gpio-test.c, stm32l4x5_rcc-test.c, stm32l4x5_syscfg-test.c, stm32l4x5_usart-test.c

23. (high) [MPLAB SIM coverage of the candidates] The local MPLAB SIM engine map routes PIC32CM5164JH01100/JH00100, PIC32CM1216MC00048, PIC32CM5164LE00100, ATSAMD21*, ATSAME54P20A, ATSAML11E16A, ATSAMV71Q21B, PIC32CK2051SG01144, PIC32CZ8110CA90208, PIC32MX795F512L and PIC32MZ2048EFH144 to 'pic32sim', and ATmega4809 and AVR128DA48 to 'sim'. A QEMU twin of any of these can be checked register-for-register against an existing in-house simulator.
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu:3035, 3073, 3052, 3076, 271-287, 329, 385, 439, 3003, 3230, 3431, 3587, 125, 672

24. (medium) [LOAD-BEARING] [Recommendation: first target family] First target: PIC32CM JH01, specifically PIC32CM5164JH01100 on the Curiosity Pro EV81X90A. Second: SAM D21 as the community-facing variant. Next: SAM E54, then PIC32CK, for M4F/M33. Defer AVR-Dx, the PIC32 MIPS families and PolarFire SoC.
   detail: Why JH: (1) at CPU level only CPUID, unprivileged mode and an IOBUS alias are missing; (2) the locally installed DFP (ATDF/SVD/headers) can generate register maps, reset values and IRQ tables; (3) Microchip's own Zephyr port provides open-source test firmware for nearly every peripheral; (4) its IP generation (G1 SERCOM/PORT/TC/TCC/EIC/RTC/WDT and the newer clock IP) is shared with SAM C2x/D5x/E5x and PIC32CM MC; (5) MPLAB SIM pic32sim gives a differential oracle; (6) NUM_IRQ=32 fits the v6-M NVIC. Risks: M_CAN and PTC have no QEMU counterpart, and the local DFP is outdated. SAM D21 has the biggest corpus (Arduino, CircuitPython, TinyUSB) and an out-of-tree example, but older clock IP and a USB-centric ecosystem that needs a device-mode USB model. SAM E54 and PIC32CK are CPU-ready but carry far more peripherals (GMAC, SDHC, QSPI, HSM). AVR-Dx needs new CPU types and a new data-space model, and PIC32 MIPS needs microAptiv/M5150 definitions plus SRS/EIC work. PolarFire extends an existing but Odd-Fixes machine and is MPU-class Linux work.
   - ev: See the findings above: CPU fit (cpu-v7m.c, armv7m_nvic.c, armv7m.c), the DFP ATDF, Zephyr PR #95480, the G1 bindings shared with sam_e54_xpro, the AVR/MIPS target gaps, and the PolarFire Odd Fixes status

## Table: Microchip family -> core -> QEMU support
| family | core/ISA | QEMU CPU model | existing machine/peripherals | gap | notes |
| SAM D10/D11/D20/D21/DA1 | Cortex-M0+ (ARMv6-M), no MPU | cortex-m0 (stand-in) | None upstream (out-of-tree Quarkslab SAMD21: SERCOM, GPIO) | SoC + all peripherals; CPUID reads M0; IOBUS alias; USB device controller | 28-29 IRQs fits the 32-IRQ v6-M NVIC; older SYSCTRL/GCLK-CLKCTRL clock IP; largest hobby ecosystem |
| SAM C20/C21 | Cortex-M0+ (C21 has MPU) | cortex-m0 (+ mpu-ns-regions) | None | SoC + peripherals incl. M_CAN (no QEMU M_CAN model) | 5 V; Microchip markets PIC32CM JH as its next generation |
| SAM L21/L22 | Cortex-M0+ (L21 no MPU, L22 MPU) | cortex-m0 | None | SoC + low-power PM/SUPC/clock models | - |
| PIC32CM JH00/JH01 | Cortex-M0+ r0p1 (ATDF): MPU, VTOR, 2 priority bits, 32 IRQs, PORT/DIVAS on IOBUS | cortex-m0 + num-irq 32 + mpu-ns-regions | None | CPUID; unprivileged mode; IOBUS alias; all peripherals (JH01 adds 2x M_CAN), DIVAS, PTC | DFP installed locally (1.3.221; latest 1.7.296); Zephyr pic32cm_jh01_cpro/cnano (Microchip-authored, 2025) |
| PIC32CM MC00 | Cortex-M0+ with MPU | cortex-m0 | None | as JH | 5 V motor control |
| PIC32CM GV | Cortex-M0+ (one SEGGER device page says M0) | cortex-m0 | None | as JH | AEC-Q100 |
| PIC32CM PL10 | Cortex-M0+ (launched Jan 2026) | cortex-m0 | None | as JH | 1.8-5.5 V; pin-compatible with AVR Dx |
| PIC32CM LE00 | Cortex-M23 (ARMv8-M Baseline), MPU, no TrustZone | none (no cortex-m23) | None | New cortex-m23 type with Baseline decode gating | Task premise that LE is M0+ is incorrect |
| PIC32CM LS00/LS60 | Cortex-M23 + TrustZone | none | None | cortex-m23 with M_SECURITY, SAU, IDAU | LS60 adds an ECC608 secure subsystem |
| SAM L10/L11 | Cortex-M23 (L11 has TrustZone) | none | None | same as LS | - |
| SAM D5x/E5x | Cortex-M4F (single-precision FPU) | cortex-m4 | None | SoC + G1 peripherals, GMAC, M_CAN, USB, SDHC, QSPI | Zephyr same54_xpro + sam_e54_xpro (G1 bindings); CircuitPython; TinyUSB; board AT24MAC402 can use at24c-eeprom |
| SAM G5x | Cortex-M4F | cortex-m4 | None | SoC | - |
| SAM4 (S/E/N/C/L) | Cortex-M4 (SAM4S no FPU; SAM4E M4F) | cortex-m4 (vfp=false where no FPU) | None | SoC | SAM4C is dual-core |
| SAM3 (X/A/U/S/N) | Cortex-M3 | cortex-m3 | None | SoC | Arduino Due uses SAM3X8E |
| SAM E70/S70/V70/V71 | Cortex-M7, double-precision FPU | cortex-m7 | None | SoC; caches/TCM only as plain RAM | - |
| PIC32CZ CA70/MC70, CA80/CA90 | Cortex-M7, double-precision FPU, 300 MHz | cortex-m7 | None | SoC; CA90 HSM not meaningfully modelable | MC70 = motor-control variant |
| PIC32CX SG41/SG60/SG61 | Cortex-M4F, 120 MHz | cortex-m4 | None | SoC; HSM on SG60/61 | - |
| PIC32CX MT (MTG/MTC/MTSH) | Cortex-M4F; MTC/MTSH dual-core | cortex-m4 (x2) | None | Dual-CPU SoC wiring; metrology front end | - |
| PIC32CX BZ2/BZ3 | Cortex-M4F 64 MHz + 2.4 GHz radio | cortex-m4 | None | Radio not modelled | - |
| PIC32CK SG/GC | Cortex-M33 + TrustZone, single-precision FPU, DSP | cortex-m33 | None | SoC; SG HSM; SAU regions fixed at 8 | armv7m has an IDAU link |
| SmartFusion2 (M2S) | Cortex-M3, 166 MHz | cortex-m3 | emcraft-sf2: MSS timer, sysreg, 2x SPI, 2x 16550 UART, EMAC | I2C, DMA, WDT, GPIO, CAN, RTC, USB are stubs | emcraft-sf2.rst wrongly says Cortex-M4 |
| PIC32MX | MIPS32 M4K (r2, MIPS16e, FMT MMU, shadow registers) | none (closest 4KEm) | None upstream (out-of-tree sergev pic32mx7 boards) | M4K definition; EIC multi-vector + shadow-set switch; all peripherals | mipsel target |
| PIC32MZ EC / DA | microAptiv (microMIPS, DSP r2, MMU) | none (closest M14Kc, which lacks DSP) | None upstream (out-of-tree sergev pic32mz boards) | microAptiv definition; MCU ASE; SRS switch; EIC vector offsets | - |
| PIC32MZ EF | M-Class M5150 (MIPS32 r5, FPU) | none (P5600 is r5 but a different core) | None | as above + r5/FPU configuration | NuttX lists PIC32MZEF as M5150 |
| PIC32MK GP/MC | microAptiv + FPU | none | None | as above | - |
| PIC32MM | microAptiv UC (microMIPS) | none (closest M14K) | None | as above | - |
| megaAVR classic (ATmega168/328/1280/2560) | AVRe+ (avr5/avr51/avr6) | avr5, avr51, avr6 | arduino-duemilanove/uno/mega/mega-2560-v3: USART, 16-bit timer, power reduction | GPIO, 8-bit timers, TWI, SPI, ADC, WDT, EEPROM are stubs | Maintained |
| Other classic AVR (ATtiny avr25, ATmega8 avr4, ...) | AVR/AVRe | none registered | None | CPU types | Docs list 17 cores; code registers 3 |
| AVR Dx/EA/EB/DU, tinyAVR 0/1/2, megaAVR 0, XMEGA | AVRxt / AVRxm | none | None | XMEGA-style data space (no register-file mapping, I/O 0x00-0x3F, flash mapped into data space); CCP; new CPU types | DxCore community Arduino core; no Zephyr AVR port |
| PolarFire SoC (MPFS) | RV64IMAC E51 + 4x RV64GC U54 | sifive-e51, sifive-u54 | microchip-icicle-kit (MMUART, DMC/IOSCB/SYSREG stubs, GEM, SDHCI, PDMA, PLIC/ACLINT) | GPIO/SPI/I2C/CAN/WDT/USB/fabric are stubs; HSS cannot boot; no tests | Odd Fixes; Zephyr mpfs_icicle |
| PIC64GX1000 | 4x U54 + E51 (RV64) | sifive-u54, sifive-e51 | None | New machine (could be derived from pfsoc) | Zephyr pic64gx_curiosity_kit |
| PIC64HX1000 | 8x SiFive X280 (RVV) + S7 | none (no x280 type; generic rv64 with V) | None | Everything | - |
| SAMA5D2/D3/D4 | Cortex-A5 | none (no cortex-a5) | None | CPU type + SoC | - |
| SAM9X60/SAM9X7 | ARM926EJ-S | arm926 | None | SoC | - |
| SAMA7G5/SAMA7D65 | Cortex-A7 | cortex-a7 | None | SoC | - |
| PIC10/12/16/18 | 8-bit PIC (proprietary) | no target | None | Entire TCG target | MPLAB SIM pic16esim/pic18sim |
| PIC24 / dsPIC30/33 | 16-bit (proprietary) | no target | None | Entire TCG target | MPLAB SIM dssim |
| dsPIC33A | 32-bit DSC with double-precision FPU (proprietary) | no target | None | Entire TCG target | - |

## Table: Existing Microchip-lineage models upstream
| file | device/IP | what is modeled | MAINTAINERS status |
| target/avr/* | AVR 8-bit CPU | avr5/avr51/avr6 TCG; classic data-space layout | AVR TCG CPUs: Maintained (Michael Rolnik, gmail) |
| hw/avr/atmega.c (+atmega.h, boot.c) | ATmega168/328/1280/2560 MCU | Memory map and IRQ tables; wires USART, 16-bit timers, PRR; GPIO, 8-bit timers, TWI, ADC, SPI, WDT, EEPROM, external memory are unimplemented stubs | AVR MCUs: Maintained (Michael Rolnik) |
| hw/avr/arduino.c | Arduino Duemilanove/Uno/Mega/Mega2560 | Machines with 16 MHz crystal; functional tests test_uno.py and test_mega2560.py | Arduino: Maintained (Philippe Mathieu-Daudé, oss.qualcomm.com) |
| hw/char/avr_usart.c | AVR USART | Data/control registers, RXC/DRE/TXC IRQs, chardev | AVR MCUs: Maintained |
| hw/timer/avr_timer16.c | AVR 16-bit timer | Count/compare/capture/overflow IRQs; PWM, external clock and output-compare pins unsupported | AVR MCUs: Maintained |
| hw/misc/avr_power.c | AVR power reduction (PRR) | Power-reduction mask outputs | AVR MCUs: Maintained |
| hw/arm/msf2-soc.c | SmartFusion2 MSS (M2S) | cortex-m3 (81 IRQs, bit-band), eNVM ROM + alias at 0, eSRAM, 2x 16550 UART, m3clk/refclk; I2C, DMA, WDT, GPIO, HS-DMA, CAN, RTC, APB config, USB are stubs | SmartFusion2: Maintained (Subbaraya Sundeep, gmail; Peter Maydell, Linaro) |
| hw/arm/msf2-som.c | Emcraft M2S-FG484 SOM | 'emcraft-sf2' machine; functional test boots U-Boot | Emcraft M2S-FG484: Maintained (same) |
| hw/misc/msf2-sysreg.c | MSS SYSREG | Clock divider / system registers | SmartFusion2: Maintained |
| hw/timer/mss-timer.c | MSS system timer (SmartFusion/SmartFusion2) | Two down-counters using ptimer | SmartFusion2: Maintained |
| hw/ssi/mss-spi.c | MSS SPI | SPI controller with FIFOs | SmartFusion2: Maintained (also under SSI) |
| hw/net/msf2-emac.c | MSS Ethernet MAC | Ethernet MAC | SmartFusion2: Maintained |
| hw/riscv/microchip_pfsoc.c | PolarFire SoC + Icicle Kit | E51 + 4x U54, ACLINT, PLIC, SiFive PDMA, 2x Cadence GEM, Cadence SDHCI, 5 MMUART, eNVM/QSPI-XIP ROM, L2LIM; many unimplemented blocks | Microchip PolarFire SoC Icicle Kit: Odd Fixes (Conor Dooley, kernel.org; Sebastian Huber, embedded-brains.de) |
| hw/char/mchp_pfsoc_mmuart.c | PolarFire MMUART | 16550 (serial_mm, regshift 2) + extra register file at +0x20 | Odd Fixes |
| hw/misc/mchp_pfsoc_dmc.c | PolarFire DDR SGMII PHY + DDR CFG | Register stubs for boot/training code | Odd Fixes |
| hw/misc/mchp_pfsoc_ioscb.c | PolarFire IOSCB | Lanes, PLLs, CCC, mailbox (with IRQ); mostly dummy | Odd Fixes |
| hw/misc/mchp_pfsoc_sysreg.c | PolarFire SYSREG | Minimal registers | Odd Fixes |
| hw/nvram/eeprom_at24c.c | Atmel/Microchip AT24Cxx I2C EEPROM | 'at24c-eeprom' with rom-size and initial contents | No MAINTAINERS entry |
| hw/block/m25p80.c | SPI NOR/EEPROM incl. Atmel AT25/AT26/AT45, AT25128A/256A, SST25VF/25WF, Microchip 25CSM04 | JEDEC IDs, erase/program, SFDP | SSI: Maintained (Alistair Francis) |
| hw/net/lan9118.c + lan9118_phy.c | SMSC LAN9118 | MAC + PHY (used by mps2/mps3/realview/vexpress/kzm) | Network devices: Odd Fixes (Jason Wang) |
| hw/net/smc91c111.c | SMSC LAN91C111 | Ethernet MAC | Network devices: Odd Fixes |
| hw/sensor/emc141x.c | SMSC/Microchip EMC1413/EMC1414 | I2C temperature sensor | Overall sensors: Odd Fixes (Philippe Mathieu-Daudé) |
| hw/nvram/eeprom93xx.c | 93C46-class Microwire EEPROM | Bit-banged serial EEPROM | EEPROM 93xx: Maintained (Stefan Weil) |
| hw/isa/fdc37m81x-superio.c | SMSC FDC37M81x Super I/O | Malta super-I/O | Malta: Odd Fixes |
| hw/isa/smc37c669-superio.c | SMSC FDC37C669 Super I/O | Alpha Clipper super-I/O | Clipper: Maintained (Richard Henderson) |

## Table: PIC32CM5164JH01100 core configuration vs QEMU cortex-m0 + armv7m
| feature | PIC32CM JH (DFP 1.3.221) | QEMU (cortex-m0 + armv7m) | evidence |
| Core / CPUID | Cortex-M0+ r0p1 (ATDF) or r0p0 (SVD) | cortex-m0, MIDR 0x410CC200; midr not passed through by armv7m | atdf:26,52; svd:29-30; target/arm/tcg/cpu-v7m.c:52; hw/arm/armv7m.c:553-567 |
| ISA | ARMv6-M | ARMv6-M 32-bit whitelist (MSR/MRS/DSB/DMB/ISB/BL) | target/arm/tcg/translate.c:6263-6288 |
| VTOR | Present | Always present, mask 0xFFFFFF80, reset value from init-nsvtor | atdf:55; hw/intc/armv7m_nvic.c:1109-1110,1638-1640; target/arm/cpu.c:552-553 |
| MPU | Present (8 regions per Arm M0+ TRM; DFP only says present) | Off by default; enable via mpu-ns-regions -> pmsav7-dregion; v6-M RASR without TEX | atdf:53; hw/arm/armv7m.c:349-355; hw/intc/armv7m_nvic.c:1998-2004 |
| NVIC IRQs | 32 (PERIPH_MAX_IRQn 31) | Maximum 32 for v6-M | atdf:51; header:127; hw/intc/armv7m_nvic.c:2758-2761 |
| Priority bits | 2 | Default 2 for v6-M (num-prio-bits configurable) | atdf:54; hw/intc/armv7m_nvic.c:2768-2773 |
| SysTick | Present (__Vendor_SysTickConfig 0) | Always present (cpuclk; refclk optional) | atdf:56,1432; hw/arm/armv7m.c:463-473 |
| Unprivileged Thread mode | Not stated in DFP | Not available (M_UNPRIV only with M_MAIN) | target/arm/cpu.c:1470-1472; target/arm/tcg/m_helper.c:2778-2781 |
| Single-cycle IOBUS | PORT at 0x60000000, DIVAS at 0x60000200 | Not modelled; use a memory-region alias | atdf:253,656; header:476,503; hw/arm/msf2-soc.c:125-129 |
| Bit-band | None | Off by default (enable-bitband=false) | hw/arm/armv7m.c:560 |
| Memory map | Flash 512 KiB @0, DATAFLASH 8 KiB @0x00400000, SRAM 64 KiB @0x20000000, peripherals on HPB0-3 @0x40000000-0x43000000 | SoC code must create these | atdf:29-46 |

## Code: M-profile CPU table (no cortex-m0plus / cortex-m23) (target/arm/tcg/cpu-v7m.c:325-340)
```c
static const ARMCPUInfo arm_v7m_cpus[] = {
    { .name = "cortex-m0",   .initfn = cortex_m0_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m3",   .initfn = cortex_m3_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m4",   .initfn = cortex_m4_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m7",   .initfn = cortex_m7_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m33",  .initfn = cortex_m33_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m55",  .initfn = cortex_m55_initfn,
                             .class_init = arm_v7m_class_init },
    { .name = "cortex-m85",  .initfn = cortex_m85_initfn,
                             .class_init = arm_v7m_class_init },
};
```

## Code: VTOR write has no feature gate (applies to cortex-m0) (hw/intc/armv7m_nvic.c:1638-1640)
```c
    case 0xd08: /* Vector Table Offset.  */
        cpu->env.v7m.vecbase[attrs.secure] = value & 0xffffff80;
        break;
```

## Code: Armv6-M NVIC limits: 32 IRQs max, 2 priority bits by default (hw/intc/armv7m_nvic.c:2758-2773)
```c
    if (!arm_feature(&s->cpu->env, ARM_FEATURE_V7) && s->num_irq > 32) {
        error_setg(errp, "Armv6-M NVIC cannot exceed 32 external IRQs");
        return;
    }

    qdev_init_gpio_in(dev, set_irq_level, s->num_irq);

    /* include space for internal exception vectors */
    s->num_irq += NVIC_FIRST_IRQ;

    if (s->num_prio_bits == 0) {
        /*
         * If left unspecified, use 2 bits by default on Cortex-M0/M0+/M1
         * and 8 bits otherwise.
         */
        s->num_prio_bits = arm_feature(&s->cpu->env, ARM_FEATURE_V7) ? 8 : 2;
```

## Code: Armv6-M MPU layout is modelled via PMSAv7 state (target/arm/cpu.c:1667-1677)
```c
    if (arm_feature(&cpu->env, ARM_FEATURE_PMSA)) {
        qdev_property_add_static(DEVICE(obj), &arm_cpu_has_mpu_property);
        /*
         * QEMU's PMSAv7 state also models the Armv6-M MPU register layout.
         */
        if (arm_feature(&cpu->env, ARM_FEATURE_V7) ||
            arm_feature(&cpu->env, ARM_FEATURE_M)) {
            qdev_property_add_static(DEVICE(obj),
                                     &arm_cpu_pmsav7_dregion_property);
        }
    }
```

## Code: armv7m container passes mpu-ns-regions through to the CPU (how a SoC enables an M0+ MPU) (hw/arm/armv7m.c:349-355)
```c
    if (s->mpu_ns_regions != UINT_MAX &&
        object_property_find(OBJECT(s->cpu), "pmsav7-dregion")) {
        if (!object_property_set_uint(OBJECT(s->cpu), "pmsav7-dregion",
                                      s->mpu_ns_regions, errp)) {
            return;
        }
    }
```

## Code: Feature implications: M_UNPRIV only with the Main Extension; V8-M implies V7 (target/arm/cpu.c:1466-1480)
```c
    if (arm_feature(env, ARM_FEATURE_M)) {
        set_feature(env, ARM_FEATURE_PMSA);
    }

    if (arm_feature(env, ARM_FEATURE_M_MAIN)) {
        set_feature(env, ARM_FEATURE_M_UNPRIV);
    }

    if (arm_feature(env, ARM_FEATURE_V8)) {
        if (arm_feature(env, ARM_FEATURE_M)) {
            set_feature(env, ARM_FEATURE_V7);
        } else {
            set_feature(env, ARM_FEATURE_V7VE);
        }
    }
```

## Code: CONTROL.nPRIV writes ignored without M_UNPRIV (so cortex-m0 has no unprivileged mode) (target/arm/tcg/m_helper.c:2778-2781)
```c
        if (cur_el > 0 && arm_feature(env, ARM_FEATURE_M_UNPRIV)) {
            env->v7m.control[env->v7m.secure] &= ~R_V7M_CONTROL_NPRIV_MASK;
            env->v7m.control[env->v7m.secure] |= val & R_V7M_CONTROL_NPRIV_MASK;
        }
```

## Code: ARMv6-M 32-bit Thumb whitelist applies only when the CPU lacks V7 (matters for a future cortex-m23) (target/arm/tcg/translate.c:6263-6288)
```c
    if (arm_dc_feature(s, ARM_FEATURE_M) &&
        !arm_dc_feature(s, ARM_FEATURE_V7)) {
        int i;
        bool found = false;
        static const uint32_t armv6m_insn[] = {0xf3808000 /* msr */,
                                               0xf3b08040 /* dsb */,
                                               0xf3b08050 /* dmb */,
                                               0xf3b08060 /* isb */,
                                               0xf3e08000 /* mrs */,
                                               0xf000d000 /* bl */};
        static const uint32_t armv6m_mask[] = {0xffe0d000,
                                               0xfff0d0f0,
                                               0xfff0d0f0,
                                               0xfff0d0f0,
                                               0xffe0d000,
                                               0xf800d000};

        for (i = 0; i < ARRAY_SIZE(armv6m_insn); i++) {
            if ((insn & armv6m_mask[i]) == armv6m_insn[i]) {
                found = true;
                break;
            }
        }
        if (!found) {
            goto illegal_op;
        }
```

## Code: Only upstream cortex-m0 SoC: nRF51 (pattern for an M0+ SoC) (hw/arm/nrf51_soc.c:186-191)
```c
    memory_region_init(&s->container, obj, "nrf51-container", UINT64_MAX);

    object_initialize_child(OBJECT(s), "armv6m", &s->armv7m, TYPE_ARMV7M);
    qdev_prop_set_string(DEVICE(&s->armv7m), "cpu-type",
                         ARM_CPU_TYPE_NAME("cortex-m0"));
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "num-irq", 32);
```

## Code: Alias precedent (pattern for PORT_IOBUS/DIVAS_IOBUS) (hw/arm/msf2-soc.c:125-129)
```c
    memory_region_init_alias(&s->nvm_alias, OBJECT(dev_soc), "MSF2.eNVM",
                             &s->nvm, 0, s->envm_size);

    memory_region_add_subregion(system_memory, ENVM_BASE_ADDRESS, &s->nvm);
    memory_region_add_subregion(system_memory, 0, &s->nvm_alias);
```

## Code: PIC32CM5164JH01100 core parameters (installed DFP) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:50-56 (PIC32CM-JH_DFP 1.3.221))
```xml
      <parameters>
        <param name="NUM_IRQ" value="32" caption="Number of interrupt request lines"/>
        <param name="__CM0P_REV" value="0x0001" caption="Cortex-M0+ Core Revision"/>
        <param name="__MPU_PRESENT" value="1" caption="MPU present or not"/>
        <param name="__NVIC_PRIO_BITS" value="2" caption="Number of Bits used for Priority Levels"/>
        <param name="__VTOR_PRESENT" value="1" caption="Vector Table Offset Register present or not"/>
        <param name="__Vendor_SysTickConfig" value="0" caption="Set to 1 if different SysTick Config is used"/>
```

## Code: PIC32CM JH PORT mapped twice (AHB/APB and IOBUS) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:655-656 (PIC32CM-JH_DFP 1.3.221))
```xml
            <register-group name="PORT" name-in-module="PORT" address-space="base" offset="0x41000000"/>
            <register-group name="PORT_IOBUS" name-in-module="PORT" address-space="base" offset="0x60000000"/>
```

## Code: AVR target hard-codes the classic register-file-in-data-space layout (target/avr/cpu.c:144-155)
```c
    /*
     * Two blocks in the low data space loop back into cpu registers.
     */
    memory_region_init_io(&cpu->cpu_reg1, OBJECT(cpu), &avr_cpu_reg1, env,
                          "avr-cpu-reg1", 32);
    memory_region_add_subregion(get_system_memory(),
                                OFFSET_DATA, &cpu->cpu_reg1);

    memory_region_init_io(&cpu->cpu_reg2, OBJECT(cpu), &avr_cpu_reg2, env,
                          "avr-cpu-reg2", 8);
    memory_region_add_subregion(get_system_memory(),
                                OFFSET_DATA + 0x58, &cpu->cpu_reg2);
```

## Code: ATmega peripherals left as unimplemented stubs (hw/avr/atmega.c:376-381)
```c
    create_unimplemented_device("avr-twi",          OFFSET_DATA + 0x0b8, 6);
    create_unimplemented_device("avr-adc",          OFFSET_DATA + 0x078, 8);
    create_unimplemented_device("avr-ext-mem-ctrl", OFFSET_DATA + 0x074, 2);
    create_unimplemented_device("avr-watchdog",     OFFSET_DATA + 0x060, 1);
    create_unimplemented_device("avr-spi",          OFFSET_DATA + 0x04c, 3);
    create_unimplemented_device("avr-eeprom",       OFFSET_DATA + 0x03f, 3);
```

## Code: Closest MIPS model to microAptiv UC (PIC32MM): M14K (r2 + microMIPS, FMT, no DSP/MCU ASE) (target/mips/cpu-defs.c.inc:373-395)
```c
        .name = "M14K",
        .CP0_PRid = 0x00019b00,
        /* Config1 implemented, fixed mapping MMU,
           no virtual icache, uncached coherency. */
        .CP0_Config0 = MIPS_CONFIG0 | (0x2 << CP0C0_KU) | (0x2 << CP0C0_K23) |
                       (0x1 << CP0C0_AR) | (MMU_TYPE_FMT << CP0C0_MT),
        .CP0_Config1 = MIPS_CONFIG1,
        .CP0_Config2 = MIPS_CONFIG2,
        .CP0_Config3 = MIPS_CONFIG3 | (0x2 << CP0C3_ISA) | (1 << CP0C3_VInt) |
                       (1 << CP0C3_M),
        .CP0_Config4 = MIPS_CONFIG4 | (1 << CP0C4_M),
        .CP0_Config5 = MIPS_CONFIG5 | (1 << CP0C5_NFExists),
        .CP0_Config7 = 1 << CP0C7_WII,
        .CP0_LLAddr_rw_bitmask = 0,
        .CP0_LLAddr_shift = 4,
        .SYNCI_Step = 32,
        .CCRes = 2,
        .CP0_Status_rw_bitmask = 0x1258FF17,
        .SEGBITS = 32,
        .PABITS = 32,
        .insn_flags = CPU_MIPS32R2 | ASE_MICROMIPS,
        .mmu_type = MMU_TYPE_FMT,
    },
```

## Code: PolarFire SoC model maintenance status (MAINTAINERS:1795-1799)
```text
Microchip PolarFire SoC Icicle Kit
M: Conor Dooley <conor@kernel.org>
M: Sebastian Huber <sebastian.huber@embedded-brains.de>
L: qemu-riscv@nongnu.org
S: Odd Fixes
```

## Open questions
- Does the PIC32CM JH Cortex-M0+ implement the optional unprivileged/privileged extension (CONTROL.nPRIV)? The DFP does not say; check the datasheet 'Processor' chapter. If it does, QEMU needs M_UNPRIV for an M0+ type, either by patching cortex-m0 or adding a new cortex-m0plus type.
- Upstream strategy: add a proper 'cortex-m0plus' CPU type (MIDR PARTNO 0xC60, M_UNPRIV, optional 8-region MPU) or keep cortex-m0 with SoC overrides? This needs target-arm maintainer (Peter Maydell) input; no prior qemu-devel series was found. Also untested: whether -global cortex-m0-arm-cpu.midr=... is an acceptable stop-gap.
- Exactly how much IP is shared between PIC32CM JH, SAM C21, SAM D5x/E5x and SAM D21 (SERCOM U2201 versions, PORT, TC/TCC, EIC, and GCLK U2122 vs SAM D21's SYSCTRL/older GCLK). Verify by diffing ATDFs after installing SAMC21_DFP 3.9.248, SAMD21_DFP 3.8.270 and SAME54_DFP 3.11.261, which are listed in the local pack index.
- The installed PIC32CM-JH_DFP is 1.3.221 but the index lists 1.7.296. Do register or reset-value changes, or the SVD r0p0 vs ATDF __CM0P_REV 0x0001 mismatch, matter for generating QEMU register models?
- For PIC32MZ/MK/MM: does XC32-generated code depend on MCU ASE instructions (ACLR/ASET/IRET), per-vector OFFx EIC offsets, or hardware shadow-set switching (IPLnSRS)? These determine how much MIPS target work a PIC32 MIPS twin needs. Also confirm from datasheets/FRM Section 50 which PIC32MZ sub-families use M5150 vs microAptiv.
- Cortex-M23: how much decoder gating (Baseline subset vs Main Extension) would upstream require for a cortex-m23 type serving PIC32CM LE/LS and SAM L10/L11? This was not tested in this session.
- Licensing and reusability of the out-of-tree Quarkslab SAMD21 code (github.com/quarkslab/sstic-tame-the-qemu) and Serge Vakulenko's PIC32 QEMU fork; neither was inspected.
- Is Microchip willing to maintain an upstream model as 'Supported', like Nuvoton NPCM7xx and ASPEED? That affects whether the twin is built as an upstream contribution or a downstream fork.
- Is a QEMU model of the Bosch M_CAN IP (used on SAM C21, PIC32CM JH01 and SAM E5x) in scope? Upstream has CAN-bus infrastructure (net/can, hw/net/can) but no M_CAN device.
- The PIC32CM JH Curiosity Nano+ (pic32cm_jh01_cnano) SoC/package variant was not checked; confirm which JH01 part it uses if it becomes a second board target.
