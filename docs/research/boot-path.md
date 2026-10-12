# boot-path

## Summary
On PIC32CM JH01 (Cortex-M0+), reaching main() needs almost no peripheral model. Every DFP startup (GCC, armcc, IAR, XC32) only copies .data, zeroes .bss, writes SCB->VTOR and calls main; the XC32 version also zero-fills all 64 KB of SRAM because the SRAM has ECC. SystemInit() is a no-op that only sets SystemCoreClock to 4 MHz, and none of the startups call it. QEMU's TYPE_ARMV7M with cortex-m0 already covers this (VTOR writable, 32 IRQs, 2 priority bits). The startup code that Harmony generates also reads WDT CTRLA, and if the WDT fuse is set it services the WDT (SYNCBUSY and CLEAR) while clearing RAM. So the WDT reset state has to come from the USER-row fuses at 0x00804000, which XC32 writes into the ELF/HEX. The real work happens between main() and the first UART byte. Harmony's SYS_Initialize writes NVMCTRL.CTRLB, PORT PINCFG/PMUX (PB10/PB11 to SERCOM4), OSCCTRL/GCLK/MCLK (OSC48M divided by 1 = 48 MHz, GCLK0, PCHCTRL[22]), then SERCOM4 USART and NVIC. Its busy-waits pass on a read-as-zero stub when they wait for SYNCBUSY==0, but hang on OSCCTRL.STATUS.OSC48MRDY and on reading PCHCTRL.CHEN back. Tier 0 therefore has to be a register file with behaviour: ready-immediately status bits, stored values that read back, write-1-to-clear (W1C) flags, and self-clearing SWRST/CMD bits. QEMU's unimplemented-device stub is not enough. Some generated code does a 64-bit load from the NVM calibration row at 0x00806020, and Zephyr reads the serial number at 0x0080A00C/40/44/48, so the 0x0080xxxx NVM rows must be mapped as ROM or the firmware hits a HardFault. The ATDF/EDC device files cannot be the only source of behaviour. They give reset values of 0 for GENCTRL0, OSCCTRL.STATUS and NVMCTRL.INTFLAG, but the datasheet gives GENCTRL0 = 0x00000106 and says the CPU starts at 4 MHz from OSC48M/12. The ATDF does flag write-synchronized registers (WSYNC) and set/clear registers, but the Enable-Protected rules and the flag-clearing rules are only in the datasheet. Zephyr's pic32cm_jh01_cpro board does all clock setup before main (32 MHz XOSC + FDPLL to 48 MHz, GCLK SWRST, 11 peripherals enabled in the devicetree) and enables the MPU, so the SoC must set mpu-ns-regions=8. QEMU has no cortex-m0plus CPU type and no Bosch M_CAN model, and it calls cpu_abort on CPU lockup — which the BKPT instructions in debug-build Harmony fault handlers will trigger. The board to mirror is EV81X90A, the PIC32CM JH01 Curiosity Pro: PIC32CM5164JH01100, EDBG, VCOM on SERCOM4 PB10/PB11, LED0 on PC05, SW0 on PB19, 32 MHz and 32.768 kHz crystals, 2× CAN. Free test suites: 38 Harmony csp_apps for this board, the Harmony core/ClassB/bootloader repos, at least 14 Zephyr test suites that list the board, and SAM C21 software, which uses the same IP blocks and base addresses. Rough size: Tier 0 ≈2.2–3.5 kLOC, Tier 1 ≈3–4.5 kLOC, Tier 2 ≈6–9.5 kLOC, estimated by comparison with similar QEMU MCU models already in the tree.

## Findings

1. (high) [LOAD-BEARING] [DFP startup (GCC/armcc/IAR)] The DFP startups touch no peripheral registers before main. Reset_Handler copies .relocate (.data/.ramfunc) from flash, zeroes .bss, writes SCB->VTOR = &_sfixed, calls the weak hooks _on_reset/__libc_init_array/_on_bootstrap, then main(). SystemInit() only sets SystemCoreClock = 4000000 and none of the startups call it. There is no FPU on this part.
   detail: GCC linker script: ROM at 0x0 (512 KB), RAM at 0x20000000 (64 KB). Defaults STACK_SIZE=0x400 and HEAP_SIZE=0x200; the heap and stack sections follow .bss, so the initial MSP is &_estack, not the top of RAM (pic32cm5164jh01100_flash.ld:36-39,54-57,146-163). Vector table: .pvStack, then 15 core vectors, then 32 IRQ vectors (startup .c:93-147).
   - ev: PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:153-199
   - ev: PIC32CM-JH01/gcc/system_pic32cm5164jh01100.c:34-49
   - ev: PIC32CM-JH01/armcc/armcc/startup_pic32cm5164jh01100.s:148-180
   - ev: PIC32CM-JH01/iar/iar/startup_pic32cm5164jh01100.c:143-158
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/config/devices_cortex_m0/PIC32CM_JH00_JH01.py#L131-L135

2. (high) [DFP XC32 startup] The XC32 DFP startup first zero-fills the whole 64 KB SRAM with 32-bit stores (ECC_Initialize, wrapped in DSB/ISB). It then runs _on_reset/__xc32_on_reset, __pic32c_data_initialization(), sets VTOR = &__svectors, runs libc init, then main(0,NULL). Builds for the MPLAB X simulator define __MPLAB_DEBUGGER_SIMULATOR and skip a GPNVM wait.
   detail: QEMU only needs plain RAM that takes 32-bit stores anywhere; there is no ECC state to model unless fault injection is wanted (Tier 3). A twin should not need simulator-specific builds.
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:97-113
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:305-306,341-364
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:68-74
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:557,576

3. (high) [LOAD-BEARING] [Harmony-generated startup (pre-main)] Harmony's startup for PIC32CM_JH00_JH01 (RAM_INIT defaults to true) reads WDT_CTRLA (ALWAYSON/ENABLE, then WEN) before clearing SRAM. If the WDT is running, every loop iteration tests WDT_SYNCBUSY.CLEAR and writes WDT_CLEAR=0xA5, and in window mode also checks INTFLAG.EW and clears it by writing 1. After that it does .dinit, VTOR, libc init, the optional MPU_Initialize, then main().
   detail: The WDT model's CTRLA/CONFIG/EWCTRL reset values must be loaded from the USER row, as the hardware does. A RAZ WDT simply takes the plain zero-fill branch; that is harmless but hides fuse-enabled WDT behaviour.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/templates/xc32/cortex_m/startup/devices/startup_xc32_PIC32CM_JH00_JH01.c.ftl#L1-L55
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/templates/xc32/cortex_m/startup/startup_xc32.c.ftl#L108-L111
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/templates/xc32/cortex_m/startup/startup_xc32.c.ftl#L156-L264
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/startup_xc32.c#L85-L190
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/config/devices_cortex_m0/PIC32CM_JH00_JH01.py#L231-L233

4. (high) [LOAD-BEARING] [Vector table and IRQ topology] There are 16 core vectors and 32 peripheral IRQs; SERCOM4 (VCOM) is IRQ 13. Several vectors are shared: SYSTEM (IRQ0 = MCLK/OSCCTRL/OSC32KCTRL/PAC/PM/SUPC), SERCOM0_6, SERCOM1_7, TC0_5, TC1_6, TC2_7. Each needs an OR gate in QEMU. Harmony places its own H3DeviceVectors table in .vectors with pvStack = &_stack (a symbol the XC32 linker creates; the .ld has no stack section).
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:51-55
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1433-1464
   - ev: PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:93-147
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/interrupts.c#L118-L147
   - ev: include/hw/core/or-irq.h:31

5. (high) [LOAD-BEARING] [Harmony SYS_Initialize: from main() to the first UART byte] For the Curiosity Pro VCOM example, main() → SYS_Initialize runs in this order. NVMCTRL_CTRLB=RWS(3). PORT: PB10/PB11 PINCFG=PMUXEN, PMUX[5]=0x33 (function D = SERCOM4 PAD2/PAD3), PC05 DIR=1. CLOCK: CAL48M from 0x00806020; OSC48MDIV=0 then wait OSC48MSYNCBUSY; wait STATUS.OSC48MRDY; OSC48MCTRL |= ONDEMAND; OSC32K=0; RTCCTRL=0; GENCTRL[0]=0x00010106 then wait SYNCBUSY.GENCTRL0; PCHCTRL[22]=CHEN then wait CHEN read-back; APBCMASK=0x08000020. NVMCTRL_CTRLB=0x84. EVSYS: nothing. SERCOM4: CTRLA=0x40310104, BAUD=63019, CTRLB=0x30000, wait SYNCBUSY, ENABLE, wait SYNCBUSY. NVIC: cpsie, IRQ13 at priority 3. The first SERCOM4_USART_Write then loops on INTFLAG.DRE writing DATA and sets INTENSET.DRE.
   detail: Harmony uses the clk_sam_c20_c21 clock plib for this family (PIC32CM_JH00_JH01.py:196-198). Flash wait states are irrelevant under TCG; Harmony's table is 19/38/48 MHz → 0/1/2 (PIC32CM_JH00_JH01.py:175-180).
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/initialization.c#L129-L146
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/port/plib_port.c#L77-L86
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L48-L112
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/nvmctrl/plib_nvmctrl.c#L63
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/sercom/usart/plib_sercom4_usart.c#L58-L131
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/nvic/plib_nvic.c#L55-L61
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/main.c#L98-L103

6. (high) [LOAD-BEARING] [Why a RAZ stub is not enough] QEMU's unimplemented-device stub returns 0 on reads. That satisfies the 'SYNCBUSY != 0' loops, but the Harmony clock code then spins forever on STATUS.OSC48MRDY and on the PCHCTRL.CHEN read-back. The datasheet tells software to 'Assert that PCHCTRLm.CHEN reads 1'. An existing QEMU M0 SoC (nRF51) already uses a 'return 1' clock stub for the same reason.
   detail: The minimum Tier 0 pattern is a register file where values are stored and read back, status bits are derived from enables (RDY = ENABLE), SYNCBUSY always reads 0, SWRST/CMD clear themselves, and INTFLAG is write-1-to-clear (W1C).
   - ev: hw/misc/unimp.c:21-28
   - ev: hw/arm/nrf51_soc.c:37-42
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L62-L65
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L104-L107
   - ev: https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/PIC32CM-JH00-JH01-Family-Data-Sheet-DS60001632.pdf (DS60001632H §14.6.2.10, p.123)

7. (high) [LOAD-BEARING] [NVM configuration rows must be mapped] Firmware reads the NVM software calibration row at 0x00806020 with a 64-bit load. Bits 2:0/5:3/8:6/11:9 are ADC bias, 18:12 OSC32K CAL and 40:19 CAL48M. The older Harmony-generated clock code reads CAL48M from it, the current template still reads OSC32K CALIB from it, and the ADC plib reads it. Zephyr hwinfo reads the 128-bit serial number at 0x0080A00C/0x0080A040/44/48. If these addresses are unmapped in QEMU, the access is a bus fault and the firmware hits a HardFault before any print.
   detail: Recommendation: one 64 KB ROM blob covering 0x00800000-0x0080FFFF, filled with 0xFF and then seeded with CAL (0x00800000), LOCKBIT (0x00802000), USER page (0x00804000, 256 B), OTP1-5 (0x00806000-27), TEMP_LOG (0x00806030) and the serial-number words. The CAL48M skip in the template is guarded by DeviceFamily != PIC32CM_JH00_JH01, but projects generated earlier (such as this csp_apps example) still perform the read.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L50-L51
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/clk_sam_c20_c21/templates/plib_clock.c.ftl#L116-L124
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/clk_sam_c20_c21/templates/plib_clock.c.ftl#L188
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/clk_sam_c20_c21/config/clk.py#L204-L206
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/adc/adc_sample/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/adc/plib_adc0.c#L90
   - ev: DS60001632H §10.2.2 Table 10-4 and §10.3, p.51
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/dts/arm/microchip/pic32c/pic32cm_jh/common/pic32cm_jh.dtsi#L40-L50
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:28-38

8. (high) [LOAD-BEARING] [Reset state: device files vs datasheet] ATDF and EDC reset values do not match the hardware for dynamic/state bits, so a model generated from them alone will hang or misclock. GCLK GENCTRL0: ATDF/EDC 0x00000000, datasheet 0x00000106 (generator 0 on, sourced from OSC48M). OSCCTRL.STATUS initval is 0 even though OSC48M is running at reset (OSC48MCTRL=0x82, OSC48MDIV=0x0B → 4 MHz). NVMCTRL.INTFLAG initval is 0 even though READY=1 means idle. MCLK.INTFLAG.CKRDY is correctly 1.
   detail: At reset: OSCULP32K provides 1.024 kHz to the WDT and RTC, GCLK0 = OSC48M/12 = 4 MHz, CPU and buses run undivided at 4 MHz, and XOSC/FDPLL/XOSC32K/OSC32K are off. QEMU's cpuclk must start at 4 MHz.
   - ev: DS60001632H §7.2 'Clocks After Reset', p.36
   - ev: DS60001632H §14.7.3 and Tables 14-5/14-6, p.129/131
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:3930
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC (SFRDef GENCTRL0 @0x40001C20 por=0)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:4860,4887,4892
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:4142
   - ev: DS60001632H NVMCTRL INTFLAG.READY, p.431
   - ev: PIC32CM-JH01/gcc/system_pic32cm5164jh01100.c:34

9. (high) [LOAD-BEARING] [Machine-readable inputs for a generator] The ATDF marks 95 registers as write- or read-write-synchronized (access=WSYNC/RWSYNC) and 102 as set/clear/toggle registers (atomic-op attribute). It also describes SERCOM's mode-qualified register views (USART_INT/EXT, SPIM/SPIS, I2CM/I2CS). The EDC .PIC has 3298 SFRDef entries with por/mclr/impl/access masks. The ATDF does NOT encode 'Enable-Protected'; that, plus 'Write-Synchronized' and the CAN 'Write-restricted (CCE=1 and INIT=1)', appears only in the datasheet's per-register 'Property:' lines (51 registers marked Enable-Protected). QEMU's register API accepts ro/w1c/reset masks plus pre/post hooks directly.
   detail: Suggested pipeline: ATDF+EDC → register skeletons (offsets, widths, reset values, W1C and set/clear pairs, SYNCBUSY bit maps), then a hand-written behaviour overlay for RDY bits, flag clearing, commands, protection and timing.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf (grep: 95 access=WSYNC|RWSYNC, 102 atomic-op)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5829-5836
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC (3298 SFRDef)
   - ev: DS60001632H register 'Property:' lines (e.g. SERCOM USART CTRLA/CTRLB §30.7.1-30.7.2)
   - ev: include/hw/core/register.h:45-60

10. (high) [Write-synchronization semantics] Each core register synchronizes on its own. A second write to the same register while it is still synchronizing is discarded and raises a PAC error, and a 16-bit synced register written as two byte accesses gets the same error. The sync delay is between 5·PGCLK+2·PAPB and 6·PGCLK+3·PAPB, and read-synchronized registers return the last synced value while SYNCBUSY is set.
   detail: Default the twin to instant sync (SYNCBUSY=0), because every Harmony and Zephyr loop tolerates it. Add an optional pedantic mode that holds SYNCBUSY in virtual time and reports violations (LOG_GUEST_ERROR/PAC flag), so firmware that skips a sync wait fails in the twin as it does on silicon.
   - ev: DS60001632H §9.6.2.1-9.6.2.5, p.48-49

11. (high) [PAC] At reset PAC STATUSB=0x00000002 (DSU write-protected) and STATUSC=0x09000000 (bit 27 SMBIST, plus bit 24, which has no name). WRCTRL keys are OFF/CLR/SET/SETLCK per PERID, and only word writes take effect. A write to a protected register is discarded, the peripheral returns an access error, and INTFLAGn is set. Illegal accesses, sync-write collisions and enable-protected writes report through PAC too. Class B clears protection on PERID 91 (SMBIST) before running MBIST.
   detail: Tier 0 needs WRCTRL/STATUS bookkeeping. Enforcement (MEMTX error leading to HardFault) can wait for a pedantic mode, but it matters for DSU CRC and Class B paths.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5124,5132
   - ev: DS60001632H §12.5.2.4-12.5.2.6, p.62-63
   - ev: https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/classb.c#L310

12. (high) [LOAD-BEARING] [USER row fuses → peripheral reset state] Datasheet Table 10-3 maps USER-row bits to reset values: BOOTPROT; BODVDD level/disable/action/hysteresis to SUPC.BODVDD; WDT enable/always-on/PER/WINDOW/EWOFFSET/WEN to WDT.CTRLA/CONFIG/EWCTRL; LOCK to NVMCTRL.LOCK. Bits 25:17 are factory BODCORE calibration (0x0A8, DO NOT CHANGE). XC32 emits the fuse words at 0x00804000/0x00804004 from #pragma config. QEMU's ROM loader writes ELF/HEX segments only into RAM or ROM regions, so the USER page must be a ROM region, and the WDT/SUPC/NVMCTRL reset logic must read it (via rom_ptr_for_as, as the CPU reset does).
   detail: The factory production values in the table are BOOTPROT 0xB, BODVDD level 0x8, BODVDD enabled, action reset, WDT off, PER/WINDOW/EWOFFSET 0xB, LOCK 0xFFFF. My recommendation (medium confidence): when the image carries USER words, merge them while preserving the 'DO NOT CHANGE' bits, as a programmer would.
   - ev: DS60001632H §10.2.1 Table 10-3, p.50-51
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/configuration.data:2-106
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/PIC32CM5164JH01100.ld:88-95,132-137
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/initialization.c#L55-L66
   - ev: hw/core/loader.c:1242-1280
   - ev: target/arm/cpu.c:555-563

13. (high) [LOAD-BEARING] [Class B safety firmware runs before main] Harmony Class B runs from _on_reset() before main. It reads RSTC.RCAUSE (WDT/SYST), clears PAC protection on SMBIST, starts SMBIST and polls STATUS.DONE, zeroes SRAM, then enables the WDT (CYC256) and spins until a WDT reset. After all tests pass it calls NVIC_SystemReset() and expects RCAUSE.SYST on the next boot. The Class B app comments out Harmony's RAM_Initialize so its persistent flags survive the reset.
   detail: Implications for Tier 0: RCAUSE must survive qemu_system_reset and be set per cause (POR 0x01, WDT 0x20, SYST 0x40). The WDT must really time out on a 1.024 kHz base. SRAM must not be cleared on warm reset. SYSRESETREQ should be wired to the RSTC model; if it is left unconnected, QEMU simply resets.
   - ev: https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/classb.c#L213-L338
   - ev: https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/classb.c#L481-L505
   - ev: https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/startup_xc32.c#L146
   - ev: hw/intc/armv7m_nvic.c:69-80
   - ev: hw/arm/armv7m.c:378

14. (high) [LOAD-BEARING] [Zephyr does everything before main] Zephyr pic32cm_jh01_cpro does the following before main. soc_early_reset_hook zero-fills SRAM with stmia; soc_reset_hook read-modify-writes SUPC VREF/VREG. The clock driver at PRE_KERNEL_1 sets NVMCTRL RWS and brings up OSC48M, XOSC (32 MHz crystal, WAIT_FOR XOSCRDY), XOSC32K and OSC32K. It then does GCLK SWRST (wait SYNCBUSY==0), configures FDPLL (WAIT_FOR DPLLSYNCBUSY==0 and DPLLSTATUS LOCK|CLKRDY → 48 MHz), and sets PCHCTRL (wait CHEN). After that, every enabled devicetree device initializes: SERCOM4 UART (with DMA), SERCOM3 I2C, SERCOM5 SPI, DMAC, ADC0, TCC2/TC5 PWM, TC0/TCC1 counters, DAC and RTC. The defconfig sets CONFIG_ARM_MPU=y, and z_arm_mpu_init fails when MPU_TYPE reports too few regions.
   detail: WAIT_FOR has timeouts, so a bad model produces LOG_ERR output and wrong clock rates rather than a hang. FDPLL config: XOSC/(2*(15+1)) = 1 MHz × (95+1) = 96 MHz, then /2 = 48 MHz (formula from SAM C2x knowledge; not re-verified in this datasheet).
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/soc/microchip/pic32c/pic32cm_jh/common/soc.c#L29-L91
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/clock_control/clock_control_mchp_pic32cm_jh.c#L1314-L1480
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/clock_control/clock_control_mchp_pic32cm_jh.c#L1797-L1836
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.dts
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro_defconfig
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/serial/uart_mchp_sercom_g1.c#L244
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/dma/dma_mchp_dmac_g2.c#L771-L788
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/arch/arm/core/mpu/arm_mpu.c#L458-L472

15. (high) [LOAD-BEARING] [QEMU CPU-side gaps (M0 vs M0+)] Upstream QEMU (11.1.50) has only 'cortex-m0' (MIDR 0x410cc200); there is no cortex-m0plus. VTOR is writable on v6-M, the NVIC caps v6-M at 32 IRQs with 2 priority bits by default, and the MPU appears only if the SoC sets mpu-ns-regions (cortex-m0 defaults to 0 regions). ARM_FEATURE_M_UNPRIV is set only for M_MAIN cores, so CONTROL.nPRIV writes are ignored on cortex-m0. The ATDF says Cortex-M0+ r0p1 with an MPU.
   detail: Workable now: cpu-type cortex-m0, num-irq 32, mpu-ns-regions 8. Better fidelity: a small cortex-m0plus type (CPUID 0x410CC601 for r0p1, plus M_UNPRIV); about 40-60 LOC in cpu-v7m.c. The CPUID value and the claim that the MPU implies the unprivileged extension were not checked against silicon (medium).
   - ev: target/arm/tcg/cpu-v7m.c:44-52,326
   - ev: target/arm/cpu.c:1466-1472
   - ev: target/arm/cpu.c:1664-1677,2342-2346
   - ev: hw/arm/armv7m.c:349-355,565
   - ev: hw/intc/armv7m_nvic.c:1378-1380,1638-1639,2758-2774
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:52-55

16. (high) [BKPT/lockup under QEMU] On M-profile, a guest BKPT pends DebugMonitor; on v6-M that exception is never enabled, so it escalates to HardFault. If that happens inside HardFault, QEMU calls cpu_abort ('Lockup... We don't model Lockup'), which terminates the emulator. Harmony/XC32 debug builds put __builtin_software_breakpoint() in Dummy_Handler, HardFault_Handler and after main returns.
   detail: Run release builds, or add a machine option so that a BKPT in a handler halts to the gdbstub as it would under MPLAB, instead of aborting. It also matters that every model IRQ is gated by INTEN, because an unhandled vector lands in Dummy_Handler.
   - ev: target/arm/tcg/m_helper.c:2385-2387
   - ev: hw/intc/armv7m_nvic.c:640-674
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:370-372,384-386
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/exceptions.c

17. (high) [LOAD-BEARING] [Clock tree must drive the QEMU Clock objects] The SysTick period follows the armv7m 'cpuclk' Clock input. With no refclk, SYST_CALIB.NOREF is set, and Harmony assumes the processor clock (no external SysTick clock). Harmony's SysTick example uses LOAD=0x493E00-1 (100 ms at 48 MHz). The Class B clock test compares SysTick with an RTC clocked from XOSC32K. So the GCLK/MCLK model must call clock_set_hz/propagate on cpuclk and on each peripheral GCLK whenever OSC48MDIV, GENCTRL, CPUDIV or FDPLL change.
   detail: From the ATDF, the GCLK channel map is: FDPLL=0, FDPLL32K=1, EIC=2, FREQM 3/4, EVSYS 5-16, SERCOM SLOW=17, SERCOM0-7 CORE 18-25, CAN0/1 26/27, TCC0/1=28, TCC2=29, TC0/1=30, TC2/3=31, TC4..7 32-35, ADC0/1 36/37, DAC 38, PTC 39, CCL 40, PDEC 41, AC 42.
   - ev: hw/timer/armv7m_systick.c:101-102,217-226,252-255
   - ev: hw/arm/armv7m.c:292-295,463-467
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/config/devices_cortex_m0/PIC32CM_JH00_JH01.py#L169-L173
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/systick/systick_periodic_timeout/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/systick/plib_systick.c#L49-L52
   - ev: https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/classb_clock_test.c#L92-L223
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:3940 (43 PCHCTRL channels)

18. (high) [LOAD-BEARING] [SERCOM USART semantics used by Harmony and Zephyr] Register map at the SERCOM4 base (0x42001400): CTRLA 0x00, CTRLB 0x04, CTRLC 0x08, BAUD 0x0C, RXPL 0x0E, INTENCLR 0x14, INTENSET 0x16, INTFLAG 0x18, STATUS 0x1A, SYNCBUSY 0x1C, DATA 0x28, DBGCTRL 0x30. Flag rules: DRE clears only on a DATA write; TXC clears on W1C or a DATA write; RXC clears only on a DATA read or receiver disable; ERROR/RXS/CTSIC/RXBRK are W1C; STATUS errors are W1C. Harmony blocking code polls DRE/RXC. In interrupt mode it primes DATA while DRE is set, sets INTENSET.DRE, and its ISR checks INTENSET != 0 and then ERROR/DRE/RXC against INTENSET. Zephyr poll_out loops on DRE. CTRLA and CTRLB are Enable-Protected and Write-Synchronized.
   detail: IRQ level = (INTFLAG & INTEN) != 0, OR'd into the shared vectors. An 'instant TX' model that leaves DRE=1 is functionally correct: Harmony's Write() then pushes the whole buffer in its priming loop. Optional pacing uses BAUD and the GCLK frequency (BAUD 63019 at 48 MHz with 16× sampling = 115200).
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5829 (SERCOM U2201 v3.1.1, USART_INT registers)
   - ev: DS60001632H §30.7.1-30.7.2 Property lines; §30.7.8 INTFLAG, p.533-534
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/sercom/usart/plib_sercom4_usart.c#L69-L90
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/sercom/usart/plib_sercom4_usart.c#L582-L611
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/sercom_u2201/templates/plib_sercom_usart_ring_buffer.c.ftl#L1115
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/serial/uart_mchp_sercom_g1.c#L1346-L1352

19. (high) [PORT usage] Harmony and Zephyr configure pins only with byte read-modify-writes of PINCFG (PMUXEN/INEN/PULLEN) and PMUX nibbles, plus DIR/OUT and the SET/CLR/TGL aliases and IN. Neither uses WRCONFIG. There are three groups at a 0x80 stride from 0x41000000, with an IOBUS alias at 0x60000000.
   detail: MMIO ops need byte granularity (8-bit PMUX/PINCFG next to 32-bit registers). Keep WRCONFIG for ASF-era code.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/port_u2210/templates/plib_port.c.ftl#L301-L324
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/port/plib_port.c#L77-L86
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/drivers/pinctrl/pinctrl_mchp_port_g1.c#L81-L89
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:654-656
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:502-503

20. (high) [DIVAS is on the printf path when C-operator override is enabled] Harmony's DIVAS plib can replace __aeabi_idiv/uidiv/idivmod/uidivmod with DIVAS hardware, which it polls via STATUS.BUSY. With that option, every integer divide (including printf number formatting) needs a DIVAS model at 0x48000000 (IOBUS alias 0x60000200). The model is about 150 LOC.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/divas_u2258/templates/plib_divas.c.ftl#L75-L103
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/divas_u2258/templates/plib_divas.c.ftl#L164-L201
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/tree/master/apps/divas/divas_c_operator_override
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:475-476

21. (high) [LOAD-BEARING] [Memory map; PIC32CM JH is a SAM C21 superset] PIC32CM JH reuses the SAM C2x IP and base addresses: MCLK 0x40000800, OSC32KCTRL 0x40001400, GCLK 0x40001C00, WDT 0x40002000, RTC 0x40002400, EIC 0x40002800, PORT 0x41000000, NVMCTRL 0x41004000, DMAC 0x41006000, SERCOM0-3 0x42000400-0x42001000, TCC0-2, ADC0 0x42004400, CAN0 0x42001C00. Harmony uses the same plibs for both (clk_sam_c20_c21, port_u2210, dmac_u2223, wdt_u2251, pac_u2120). JH adds NVMCTRL 03929, MCRAMC (SRAM ECC), SMBIST, ICM, PDEC, SERCOM6/7 and TC5-7 on HPB3 (0x43000000). JH has no USB, TSENS or SDADC.
   detail: One SoC model with parameters could cover JH00 (no CAN), JH01 and plausibly SAM C21, which widens the free test base. PIC32CM-JH00 has the same modules minus CAN.
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:468-527,537-582
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:28-46,62-1417
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/dts/arm/atmel/samc2x.dtsi
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/dts/arm/atmel/samc21.dtsi#L57-L58
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/config/devices_cortex_m0/PIC32CM_JH00_JH01.py#L193-L226
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/config/devices_cortex_m0/SAM_C20_C21.py#L286-L319
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/config/peripheral.py#L146

22. (high) [DSU device ID] PIC32CM5164JH01100 DSU.DID = 0x11060X00 with REVISION 0x1 (B0) or 0x2 (C0). The ATDF initval is 0x11060100, and the EDC DeviceIDSector compares 0x41002018 under mask 0xFFFFF0FF. The DSU is mirrored at 0x41002100 (DSU_EXT) and is PAC-protected at reset.
   detail: Nothing on the normal boot path reads DID; IDE/debugger device checks and some bootloaders do. Make the revision a machine property.
   - ev: https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/Errata/PIC32CM-JH00-JH01-Family-Silicon-Errata-and-Data-Sheet-Clarifications-DS80001000.pdf (DS80001000H Table 2)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1625,3406
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC (DeviceIDSector 0x41002018 mask 0xFFFFF0FF)
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:478-479,587

23. (high) [LOAD-BEARING] [Board to mirror: EV81X90A PIC32CM JH01 Curiosity Pro] The EV81X90A carries a PIC32CM5164JH01100 (100-pin, 512 KB flash, 64 KB SRAM) and an on-board EDBG over SWD (PA30/PA31). The EDBG virtual COM port is SERCOM4: PB10 TX (PAD2) and PB11 RX (PAD3); the EDBG needs DTR asserted. The yellow LED0 is on PC05 and is active low. SW0 is on PB19 and pulls to GND (Harmony: EXTINT3; Zephyr: pull-up, active low). Crystals are 32 MHz and 32.768 kHz. CAN0 is on PA24/PA25 and CAN1 on PB14/PB15 through ATA6561 transceivers. LIN uses SERCOM0 PA06/PA07 with enable on PC06. The TA100 sits on SERCOM5 SPI with SS on PC08. DGI uses SERCOM7 SPI and SERCOM1 I2C on PA16/PA17. Zephyr's board pic32cm_jh01_cpro mirrors all of this.
   detail: microchip.com product pages returned HTTP 403 to this environment. The part number EV81X90A and all pin data come from the official user-guide PDF, which loaded fine from ww1.microchip.com.
   - ev: https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/UserGuides/PIC32CM-JH01-Curiosity-Pro-Evaluation-Kit-User-Guide-DS70005482.pdf (DS70005482B: Tables 4-32/4-33 p.29; LED/buttons/crystals p.19; CAN p.20; LIN p.21; TA100 p.22)
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.dts
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro-pinctrl.dtsi
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/eic/eic_led_on_off/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/eic/plib_eic.c

24. (medium) [Alternative board] Zephyr also supports the PIC32CM JH01 Curiosity Nano+ Touch (EV29G58A): PIC32CM5164JH01048, nEDBG, console on SERCOM1, LED0 on PA19, SW0 on PA27. It is a smaller second machine to consider.
   detail: I did not check this on microchip.com (HTTP 403).
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cnano/pic32cm_jh01_cnano.dts
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cnano/doc/index.rst

25. (high) [Free test corpora (Microchip)] Microchip already publishes test firmware for this exact board. csp_apps_pic32cm_jh00_jh01 has 38 examples (AC, ADC, CAN-FD, CCL, clock, DAC, DIVAS, DMAC, DSU, EIC, EVSYS, FREQM, MCRAMC, NVMCTRL, PM, PORT, RSTC, RTC, SERCOM I2C/SPI/USART/LIN, SysTick, TC, TCC, WDT). core_apps adds FreeRTOS, driver and system examples, and there are separate Class B and bootloader repos. The I2C example talks to an AT24MAC at 0x54, which QEMU's eeprom_at24c can model. The SPI example uses 25xx commands (WREN/WRITE/RDSR/READ).
   detail: m25p80 is a NOR-flash model and only a partial stand-in for a 25xx EEPROM (medium confidence). rstc_reset_cause is a good end-to-end Tier 0/1 test: printf, SysTick, EIC button injection, WDT reset, and RCAUSE.
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01 (repo tree, 38 apps/*/firmware/src/main.c)
   - ev: https://github.com/Microchip-MPLAB-Harmony/core_apps_pic32cm_jh00_jh01 (apps/rtos/freertos/basic_freertos, driver/usart, system/console_debug)
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/i2c/master/i2c_eeprom/firmware/src/main.c#L58
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/spi/spi_eeprom_write_read/firmware/src/main.c#L71-L77
   - ev: hw/nvram/eeprom_at24c.c
   - ev: hw/block/m25p80.c

26. (high) [Free test corpora (Zephyr and third party)] The Zephyr board lists adc, clock_control, comparator, counter, dac, dma, flash, gpio, hwinfo, i2c, pinctrl, pwm, reset, rtc, spi, uart and watchdog as supported. GitHub code search returned 14 tests.yaml files that name the board. Zephyr's qemu_cortex_m0 board, which runs on QEMU's microbit machine, is the precedent for adding SUPPORTED_EMU_PLATFORMS qemu so twister can run the real board DTS. For SAM C21 there are Zephyr samc21n_xpro (atmel,sam0 drivers including CAN via can_sam0/can_mcan) and MattairTech's Arduino core. CircuitPython's atmel-samd port covers only samd21/samd51/same51/same54. TinyUSB is irrelevant because JH has no USB. Official FreeRTOS demos stop at SAMD20.
   detail: QEMU's functional-test pattern of booting a vendor .hex via -device loader and matching console output maps directly onto MPLAB-built .hex/.elf files.
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.yaml
   - ev: GitHub code search 'pic32cm_jh01_cpro repo:zephyrproject-rtos/zephyr path:tests' (e.g. tests/drivers/uart/uart_async_api/tests.yaml, tests/drivers/counter/counter_basic_api/tests.yaml)
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/qemu/cortex_m0/board.cmake
   - ev: https://github.com/zephyrproject-rtos/zephyr/blob/main/boards/atmel/sam0/samc21n_xpro/samc21n_xpro.yaml
   - ev: https://github.com/mattairtech/ArduinoCore-samd (README L7)
   - ev: https://github.com/adafruit/circuitpython/blob/main/ports/atmel-samd/Makefile#L41-L63
   - ev: https://github.com/FreeRTOS/FreeRTOS/tree/main/FreeRTOS/Demo (CORTEX_M0+_Atmel_SAMD20_XPlained only)
   - ev: tests/functional/arm/test_microbit.py:15-28
   - ev: hw/core/generic-loader.c:154-169

27. (high) [CAN-FD on JH01] JH01 CAN (U2003, two instances) is a Bosch M_CAN; the errata cites the 'M_CAN User's Manual'. Its message RAM lives in system SRAM. QEMU's CAN bus supports CAN FD frames (64 bytes, BRS/ESI flags) but has no M_CAN model, and the SocketCAN host bridge only builds on Linux hosts. Harmony's CAN plib polls CCCR.INIT and CSA read-back.
   detail: The ATDF CAN parameter MSG_RAM_ADDR reads '0x200000000' (9 hex digits), which looks malformed. The user is on a Windows host, so connecting to real CAN needs WSL2/Linux; in-process buses between QEMU devices work anywhere.
   - ev: DS80001000H item 2.3.4 (M_CAN User's Manual reference)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:152-186
   - ev: include/net/can_emu.h:46-51,62-64
   - ev: net/can/meson.build:3-5
   - ev: hw/net/can/ (sja1000, ctucan, flexcan, xlnx-zynqmp-can, xlnx-versal-canfd only)
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/can_u2003/templates/plib_can.c.ftl#L140
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/can_u2003/templates/plib_can.c.ftl#L1070

28. (high) [Errata worth an optional 'errata mode'] DS80001000H lists silicon behaviour a high-fidelity twin could reproduce. Examples: SERCOM SWRST does not start when ENABLE=0 (2.10.1); MCRAMC INTSTA is PAC-protected (2.7.1); reading FREQM CTRLB raises a PAC error (2.6.1); standby entry can HardFault with the SysTick interrupt on (2.4.1); spurious FDPLL unlocks (2.8.1); several I2C STATUS bits are not auto-cleared (2.11.1); TC SYNCBUSY releases early (2.14.1); TCC raises DMA requests regardless of DMAOS (2.15.2).
   detail: Gate each item on the DID revision property (A0-D0).
   - ev: https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/Errata/PIC32CM-JH00-JH01-Family-Silicon-Errata-and-Data-Sheet-Clarifications-DS80001000.pdf (Table 1-1)

29. (medium) [Size calibration from in-tree QEMU models] Comparable QEMU models give a size scale (wc -l): STM32L4 USART 657, STM32L4 RCC clock tree 1477, STM32L4 GPIO 478, STM32L4 EXTI 293, STM32F2 timer 346, STM32F2 SPI 226, STM32F2 ADC 308, nRF51 NVMC 396, CMSDK watchdog 434, PL080 DMA 471, flexcan 1461, Xilinx Versal CAN-FD 1941, STM32L4 SoC 494 plus board 137, nRF51 SoC 239 plus microbit 84.
   detail: The line counts are exact. The PIC32CM estimates in the roadmap are extrapolations: SAM peripherals carry extra mode views, sync/protection rules and more instances than these analogues.
   - ev: hw/char/stm32l4x5_usart.c
   - ev: hw/misc/stm32l4x5_rcc.c
   - ev: hw/gpio/stm32l4x5_gpio.c
   - ev: hw/misc/stm32l4x5_exti.c
   - ev: hw/timer/stm32f2xx_timer.c
   - ev: hw/ssi/stm32f2xx_spi.c
   - ev: hw/adc/stm32f2xx_adc.c
   - ev: hw/nvram/nrf51_nvm.c:340
   - ev: hw/watchdog/cmsdk-apb-watchdog.c
   - ev: hw/dma/pl080.c
   - ev: hw/net/can/flexcan.c
   - ev: hw/net/can/xlnx-versal-canfd.c
   - ev: hw/arm/stm32l4x5_soc.c
   - ev: tests/qtest/stm32l4x5_usart-test.c

30. (medium) [Relation to MPLAB SIM] Simulator.emu maps every PIC32CM JH00/JH01 device to the 'pic32sim' engine, and the XC32 startups already special-case __MPLAB_DEBUGGER_SIMULATOR. A QEMU twin should run the unmodified production image instead. Any peripheral behaviour pic32sim already encodes (ready bits, sync, flags) would be a useful seed for the behaviour overlay.
   detail: Whether pic32sim's models can be reused (internal source access, licensing) was not checked.
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu:3005-3088
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:68-74
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp/blob/master/arch/arm/templates/xc32/cortex_m/startup/startup_xc32.c.ftl#L50-L52

## Table: Pre-main register touches (Harmony XC32 VCOM example through first byte; 'ex/' = github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/)
| step | register | operation | poll condition? | model requirement | source |
| 0 HW reset (all flows) | Vector table @0x00000000 (MSP, Reset_Handler) | CPU fetches words 0 and 1 | no | Flash ROM at 0x0 holding the loaded ELF/HEX; armv7m reset reads SP/PC via rom_ptr_for_as | target/arm/cpu.c:555-563; PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:93-98 |
| 1 DFP GCC/armcc/IAR Reset_Handler | SRAM .relocate/.bss; SCB_VTOR 0xE000ED08 | copy .data, zero .bss, VTOR=&_sfixed; SystemInit() not called (and a no-op) | no | Plain RAM; VTOR is provided by the QEMU NVIC (writable on v6-M) | PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:153-190; hw/intc/armv7m_nvic.c:1638-1639 |
| 1' DFP XC32 Reset_Handler | SRAM 0x20000000-0x2000FFFF; SCB_VTOR | ECC_Initialize 32-bit zero of all 64 KB; .dinit; VTOR=&__svectors | no | Plain RAM, no ECC model needed | PIC32CM-JH01/xc32/PIC32CM5164JH01100/startup_pic32cm5164jh01100.c:97-113,306,342,348-349 |
| 1'' Harmony startup RAM_Initialize | WDT_CTRLA 0x40002000 (ALWAYSON, ENABLE, WEN) | read 2-3x to pick the SRAM-clear loop | no (branch only) | Reset value derived from USER row (WDT_ENABLE bit26, ALWAYSON bit27, WEN bit40); production default 0 | ex/startup_xc32.c#L85-L87; DS60001632H Table 10-3 p.50-51 |
| 1'' (only if WDT enabled) | WDT_SYNCBUSY 0x40002008 .CLEAR; WDT_CLEAR 0x4000200C; WDT_INTFLAG 0x40002006 .EW | per word: if SYNCBUSY.CLEAR==0 then CLEAR=0xA5; window mode also tests EW and W1C | no (if-test inside zero loop) | SYNCBUSY=0; 0xA5 restarts the timeout; any other value causes immediate reset (RCAUSE.WDT) | ex/startup_xc32.c#L89-L127; DS60001632H p.250 |
| 1''' optional Class B _on_reset | RSTC_RCAUSE 0x40000C00; PAC_WRCTRL 0x40000000; SMBIST_STATUS/CTRL (0x42006C00 block) | read RCAUSE.WDT/SYST; WRCTRL=KEY_CLR|PERID(91); start MBIST; zero SRAM; enable WDT CYC256 and spin; later NVIC_SystemReset() | while SMBIST_STATUS.DONE==0 | RCAUSE latched per reset cause; SMBIST sets DONE with FAIL=0; WDT must really reset; SRAM preserved across WDT/SYSRESETREQ resets | classb_pic32cm_jh .../classb/classb.c#L276-L338,#L481-L505 |
| 2 Harmony startup tail | SCB_VTOR; MPU_TYPE/CTRL/RNR/RBAR/RASR (only if MPU used) | VTOR=&__svectors; __libc_init_array; optional MPU_Initialize() | no | QEMU NVIC; armv7m mpu-ns-regions=8 so MPU_TYPE.DREGION=8 | ex/startup_xc32.c#L171-L190; csp startup_xc32.c.ftl#L240-L245; hw/arm/armv7m.c:349-355 |
| 3 main() → SYS_Initialize | NVMCTRL_CTRLB 0x41004004 | write RWS(3) | no | Store (no timing effect under TCG) | ex/initialization.c#L129 |
| 4 PORT_Initialize | PB PINCFG[10]/[11] 0x410000CA/0x410000CB=0x01; PMUX[5] 0x410000B5=0x33 | 8-bit writes | no | Byte-granular registers; function D routes PB10/PB11 to SERCOM4 PAD2/PAD3 | ex/peripheral/port/plib_port.c#L77-L80 |
| 4 PORT_Initialize | PC DIR 0x41000100=0x20; PINCFG[5] 0x41000145=0; PMUX[2] 0x41000132=0 | writes | no | PC05 output (LED0, active low) | ex/peripheral/port/plib_port.c#L83-L86 |
| 5 CLOCK_Initialize (older generation) | 64-bit read @0x00806020 → OSCCTRL_CAL48M 0x40001038 | load + store | no | NVM software-calibration row mapped as readable ROM; otherwise bus fault → HardFault | ex/peripheral/clock/plib_clock.c#L50-L51; csp clk_sam_c20_c21 plib_clock.c.ftl#L116-L124 |
| 5 CLOCK_Initialize | OSCCTRL_OSC48MDIV 0x40001015 | write 0 (48 MHz) | while OSC48MSYNCBUSY(0x40001018).OSC48MDIV(bit2)==1 | Reads 0; recompute GCLK0/cpuclk 4→48 MHz | ex/peripheral/clock/plib_clock.c#L55-L60 |
| 5 | OSCCTRL_STATUS 0x4000100C | read | while OSC48MRDY(bit4)!=1 | OSC48MRDY = OSC48MCTRL.ENABLE (reset 0x82); a RAZ stub hangs here | ex/peripheral/clock/plib_clock.c#L62-L65; PIC32CM5164JH01100.atdf:4860,4887 |
| 5 | OSC48MCTRL 0x40001014; OSC32KCTRL_OSC32K 0x40001418; RTCCTRL 0x40001410 | read-modify-write ONDEMAND; write 0; RTCSEL(0) | no | Store | ex/peripheral/clock/plib_clock.c#L66,#L72-L74 |
| 5 GCLK0_Initialize | GCLK_GENCTRL[0] 0x40001C20 | write 0x00010106 (DIV1, SRC6=OSC48M, GENEN) | while GCLK_SYNCBUSY(0x40001C04).GENCTRL0(bit2)==1 | SYNCBUSY=0; reset value 0x00000106 per datasheet (ATDF says 0) | ex/peripheral/clock/plib_clock.c#L82-L87; DS60001632H p.129 |
| 5 | GCLK_PCHCTRL[22] SERCOM4_CORE 0x40001CD8 | write GEN(0)|CHEN = 0x40 | while PCHCTRL[22].CHEN!=1 | Store and read back (RAZ hangs); sets the SERCOM4 GCLK frequency | ex/peripheral/clock/plib_clock.c#L102-L107 |
| 5 | MCLK_APBCMASK 0x4000081C | write 0x08000020 | no | Store (optional bus-clock gating) | ex/peripheral/clock/plib_clock.c#L112 |
| 6 NVMCTRL_Initialize | NVMCTRL_CTRLB 0x41004004 | write 0x84 (RWS2|MANW) | no | Store | ex/peripheral/nvmctrl/plib_nvmctrl.c#L63 |
| 7 SERCOM4_USART_Initialize | CTRLA 0x42001400; BAUD 0x4200140C; CTRLB 0x42001404 | CTRLA=0x40310104; BAUD=63019; CTRLB=0x00030000 | while SYNCBUSY(0x4200141C)!=0 | CTRLA.MODE selects the USART register view; SYNCBUSY reads 0 | ex/peripheral/sercom/usart/plib_sercom4_usart.c#L58,#L103-L121 |
| 7 | SERCOM4 CTRLA.ENABLE | read-modify-write |=0x2 | while SYNCBUSY!=0 | Enable immediately; INTFLAG.DRE=1 | ex/peripheral/sercom/usart/plib_sercom4_usart.c#L125-L131 |
| 8 NVIC_Initialize | PRIMASK; NVIC IPR3 (0xE000E40C); ISER0 bit13 | cpsie i; priority 3; enable IRQ13 | no | QEMU NVIC (2 priority bits) | ex/peripheral/nvic/plib_nvic.c#L55-L61 |
| 9 first print (main) | SERCOM4 INTFLAG 0x42001418; DATA 0x42001428; INTENSET 0x42001416 | write DATA while DRE; then INTENSET=DRE → IRQ13 ISR finishes | loop while DRE==1 and bytes remain | DATA → chardev; DRE/TXC per DS §30.7.8; IRQ level = INTFLAG & INTEN | ex/peripheral/sercom/usart/plib_sercom4_usart.c#L319-L335,#L582-L611 |
| Zephyr (pre-main, for contrast) | SUPC_VREF/VREG; NVMCTRL_CTRLB; XOSCCTRL/STATUS; GCLK_CTRLA.SWRST; DPLLRATIO/CTRLB/PRESC/CTRLA; PCHCTRL; per-device SWRST | soc_reset_hook read-modify-write; clock_mchp_init; device init at PRE_KERNEL/POST_KERNEL | WAIT_FOR XOSCRDY, OSC48MSYNCBUSY==0, DPLLSYNCBUSY==0, DPLLSTATUS LOCK|CLKRDY, GCLK_SYNCBUSY==0, PCHCTRL.CHEN, DMAC CTRL.SWRST==0 | Ready-immediately makes every wait pass; otherwise timeouts, LOG_ERR and wrong clock rates | zephyr soc/microchip/pic32c/pic32cm_jh/common/soc.c#L29-L91; drivers/clock_control/clock_control_mchp_pic32cm_jh.c#L1314-L1480,#L1797-L1836 |

## Table: Harmony busy-wait loops ('csp' = github.com/Microchip-MPLAB-Harmony/csp/blob/master/peripheral/; 'cb' = github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/)
| plib/function | exact condition | register/bit | model must... | source URL |
| clock / OSCCTRL_Initialize (XOSC on, ONDEMAND off) | while((OSCCTRL_REGS->OSCCTRL_STATUS & OSCCTRL_STATUS_XOSCRDY_Msk) != OSCCTRL_STATUS_XOSCRDY_Msk) | OSCCTRL.STATUS[0] XOSCRDY @0x4000100C | Set XOSCRDY when XOSCCTRL.ENABLE=1 | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L106 |
| clock / OSC48MDIV write | while((OSCCTRL_REGS->OSCCTRL_OSC48MSYNCBUSY & OSCCTRL_OSC48MSYNCBUSY_OSC48MDIV_Msk) == OSCCTRL_OSC48MSYNCBUSY_OSC48MDIV_Msk) | OSC48MSYNCBUSY[2] @0x40001018 | Read 0; apply the new divider to the Clock tree | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L141 |
| clock / OSC48M ready | while((OSCCTRL_REGS->OSCCTRL_STATUS & OSCCTRL_STATUS_OSC48MRDY_Msk) != OSCCTRL_STATUS_OSC48MRDY_Msk) | OSCCTRL.STATUS[4] | 1 whenever OSC48MCTRL.ENABLE (true at reset) | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L147 |
| clock / OSC32KCTRL_Initialize (XOSC32K) | while(!((OSC32KCTRL_REGS->OSC32KCTRL_STATUS & OSC32KCTRL_STATUS_XOSC32KRDY_Msk) == OSC32KCTRL_STATUS_XOSC32KRDY_Msk)) | OSC32KCTRL.STATUS[0] @0x4000140C | Set when XOSC32K.ENABLE | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L179 |
| clock / OSC32KCTRL_Initialize (OSC32K) | while(!((OSC32KCTRL_REGS->OSC32KCTRL_STATUS & OSC32KCTRL_STATUS_OSC32KRDY_Msk) == OSC32KCTRL_STATUS_OSC32KRDY_Msk)) | OSC32KCTRL.STATUS[1] | Set when OSC32K.ENABLE; also map 0x00806020 (CALIB read at #L188) | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L199 |
| clock / FDPLL_Initialize (ratio; same form for PRESC #L246 and ENABLE #L254) | while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) | DPLLSYNCBUSY @0x4000102C | Read 0 | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L237 |
| clock / FDPLL lock | while((OSCCTRL_REGS->OSCCTRL_DPLLSTATUS & (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) != (OSCCTRL_DPLLSTATUS_LOCK_Msk | OSCCTRL_DPLLSTATUS_CLKRDY_Msk)) | DPLLSTATUS[1:0] @0x40001030 | Return 0x3 when DPLLCTRLA.ENABLE and a reference clock exists; compute the output frequency | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L260-L261 |
| clock / GCLK0_Initialize (CPUDIV != 1) | while((MCLK_REGS->MCLK_INTFLAG & MCLK_INTFLAG_CKRDY_Msk) != MCLK_INTFLAG_CKRDY_Msk) | MCLK.INTFLAG[0] @0x40000803 (reset 1, W1C) | Set CKRDY after every CPUDIV write; update cpuclk | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L290 |
| clock / GCLKn_Initialize | while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL${i}_Msk) == GCLK_SYNCBUSY_GENCTRL${i}_Msk) | GCLK.SYNCBUSY[2+n] @0x40001C04 | Read 0 | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L307 |
| clock / CLOCK_Initialize per channel (also #L216 for FDPLL ref) | while ((GCLK_REGS->GCLK_PCHCTRL[${.vars[GCLK_ID_INDEX]}] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk) | PCHCTRLm[6] CHEN @0x40001C80+4m | Store and read back CHEN (RAZ hangs) | csp clk_sam_c20_c21/templates/plib_clock.c.ftl#L339 |
| sercom usart / Initialize, SerialSetup, Enable/Disable, TX/RX enable | while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_SYNCBUSY) != 0U) | SERCOMn.SYNCBUSY (SWRST/ENABLE/CTRLB) +0x1C | Whole register reads 0; SWRST/ENABLE take effect at once | csp sercom_u2201/templates/plib_sercom_usart.c.ftl#L199 |
| sercom usart / blocking Write (WriteByte #L687) | while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG & (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_DRE_Msk) == 0U) | INTFLAG[0] DRE +0x18 | DRE=1 whenever DATA can take a byte (instant TX keeps it 1; paced TX re-sets it after one frame) | csp sercom_u2201/templates/plib_sercom_usart.c.ftl#L568 |
| sercom usart / blocking Read | while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG & SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_RXC_Msk) == 0U) | INTFLAG[2] RXC | Set when a chardev byte is buffered; clear only on a DATA read or RX disable | csp sercom_u2201/templates/plib_sercom_usart.c.ftl#L766 |
| sercom usart / ErrorClear (RX flush) | while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG & (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_RXC_Msk) == (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_RXC_Msk) | INTFLAG.RXC + DATA | A DATA read pops the 2-level RX buffer and drops RXC when empty (otherwise infinite loop) | csp sercom_u2201/templates/plib_sercom_usart.c.ftl#L140 |
| sercom usart ring buffer / TX ISR | while ((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG & SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_DRE_Msk) == SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_DRE_Msk) | INTFLAG.DRE; INTENCLR.DRE | Loop exits via INTENCLR when the queue is empty; the IRQ line must follow INTEN & INTFLAG | csp sercom_u2201/templates/plib_sercom_usart_ring_buffer.c.ftl#L1115 |
| wdt / Enable, Disable, window, ClearWithSync (#L105,#L114,#L207,#L217) | while(${WDT_INSTANCE_NAME}_REGS->WDT_SYNCBUSY != 0U) | WDT.SYNCBUSY @0x40002008 | Read 0 | csp wdt_u2251/templates/plib_wdt.c.ftl#L89 |
| pm / IdleModeEnter (also #L197, Standby #L219) | while ((${PM_INSTANCE_NAME}_REGS->PM_SLEEPCFG & PM_SLEEPCFG_SLEEPMODE_IDLE_Val) == 0U) | PM.SLEEPCFG @0x40000401 | Store and read back SLEEPMODE; WFI is handled by the CPU | csp pm_u2240/templates/plib_pm.c.ftl#L205 |
| supc / SUPC_Initialize (then BODVDDRDY at #L294) | while((${SUPC_INSTANCE_NAME}_REGS->SUPC_STATUS & SUPC_STATUS_${SUPC_BOD_SRDY}_Msk) == 0U) | SUPC.STATUS[2] BVDDSRDY, [0] BODVDDRDY @0x4000180C | Set both when BODVDD.ENABLE; reset BODVDD fields come from the USER row | csp supc_u2117/templates/plib_supc.c.ftl#L287 |
| dsu / CRCCalculate | while((${DSU_INSTANCE_NAME}_REGS->DSU_STATUSA & DSU_STATUSA_DONE_Msk) == 0U) | DSU.STATUSA[0] DONE @0x41002001 | Compute CRC32 over ADDR/LENGTH, set DONE (BERR on bad range); DSU is PAC-protected at reset | csp dsu_u2209/templates/plib_dsu.c.ftl#L75 |
| divas / div, sqrt (also the C-operator override) | while((DIVAS_REGS->DIVAS_STATUS & DIVAS_STATUS_BUSY_Msk) == DIVAS_STATUS_BUSY_Msk) | DIVAS.STATUS.BUSY (0x48000000 block) | Compute instantly with BUSY=0; set DBZ on divide by zero | csp divas_u2258/templates/plib_divas.c.ftl#L78 |
| nvmctrl 03929 / ECC fault injection | while ((${NVMCTRL_INSTANCE_NAME}_REGS->NVMCTRL_FLTCTRL & NVMCTRL_FLTCTRL_FLTEN_En) != 0U) | NVMCTRL.FLTCTRL.FLTEN @0x41004084 | Clear FLTEN once the injected fault is consumed (Tier 3) | csp nvmctrl_03929/templates/plib_nvmctrl.c.ftl#L389 |
| classb flash test (NVMCTRL READY) | while ((NVMCTRL_REGS->NVMCTRL_INTFLAG & status) != NVMCTRL_INTFLAG_READY_Msk) | NVMCTRL.INTFLAG[0] READY @0x41004014 | Re-assert READY after the W1C while idle (ATDF initval 0 is misleading) | cb classb_flash_test.c#L79 |
| classb startup / SMBIST | while ((SMBIST_REGS->SMBIST_STATUS & SMBIST_STATUS_DONE_Msk) == 0) | SMBIST.STATUS DONE/FAIL | Set DONE (FAIL=0) after CTRL start; PERID 91 is PAC-protected at reset | cb classb.c#L317 |
| classb SRAM test / MCRAMC | while ((MCRAMC_REGS->MCRAMC_INTSTA & status) != 0U) | MCRAMC.INTSTA @0x40003010 | Implement W1C | cb classb_sram_test.c#L78 |
| tc / timer (also SWRST #L117, SYNCBUSY #L154, ENABLE #L166) | while((${TC_INSTANCE_NAME}_REGS->${TC_CTRLA_MODE}.TC_CTRLBSET & TC_CTRLBSET_CMD_Msk) != 0U) | TC.CTRLBSET.CMD; TC.SYNCBUSY | Run RETRIGGER/STOP/UPDATE/READSYNC and self-clear CMD; SWRST self-clears | csp tc_u2249/templates/plib_tc_timer.c.ftl#L214 |
| eic / Initialize (SWRST at #L84) | while((${EIC_INSTANCE_NAME}_REGS->EIC_SYNCBUSY & EIC_SYNCBUSY_ENABLE_Msk) == EIC_SYNCBUSY_ENABLE_Msk) | EIC.SYNCBUSY | Read 0; SWRST resets registers | csp eic_u2254/templates/plib_eic.c.ftl#L179 |
| rtc / timer mode (SWRST at #L76) | while((${RTC_INSTANCE_NAME}_REGS->MODE0.RTC_SYNCBUSY & RTC_MODE0_SYNCBUSY_COUNTSYNC_Msk) == RTC_MODE0_SYNCBUSY_COUNTSYNC_Msk) | RTC.SYNCBUSY | Read 0; COUNT derived from virtual time at the RTCCTRL-selected rate | csp rtc_u2250/templates/plib_rtc_timer.c.ftl#L112 |
| adc / Initialize (64-bit OTP5 calibration read at #L227; all-SYNCBUSY at #L294) | while((${ADC_INSTANCE_NAME}_REGS->ADC_SYNCBUSY & ADC_SYNCBUSY_SWRST_Msk) == ADC_SYNCBUSY_SWRST_Msk) | ADC.SYNCBUSY | Read 0; SWRST self-clears; map 0x00806020 | csp adc_u2247/templates/plib_adc.c.ftl#L220 |
| dmac / channel disable and CRC calculate (#L646) | while((${DMA_INSTANCE_NAME}_REGS->DMAC_CHCTRLA & DMAC_CHCTRLA_ENABLE_Msk) != 0U) ; while ((${DMA_INSTANCE_NAME}_REGS->DMAC_CRCSTATUS & DMAC_CRCSTATUS_CRCBUSY_Msk) == 0U) | DMAC.CHCTRLA.ENABLE; DMAC.CRCSTATUS.CRCBUSY | Disable completes immediately; CRCBUSY must be SET after each CRCDATAIN write, then cleared by W1C | csp dmac_u2223/templates/plib_dmac.c.ftl#L349 |
| tcc / PWM commands (SWRST #L329, SYNCBUSY #L394) | while((${TCC_INSTANCE_NAME}_REGS->TCC_CTRLBSET & TCC_CTRLBSET_CMD_Msk) != 0U) | TCC.CTRLBSET.CMD | Self-clear CMD | csp tcc_u2213/templates/plib_tcc_pwm.c.ftl#L524 |
| can (M_CAN) / Initialize, clock stop (#L1070) | while ((${CAN_INSTANCE_NAME}_REGS->CAN_CCCR & CAN_CCCR_INIT_Msk) != CAN_CCCR_INIT_Msk) ; while ((${CAN_INSTANCE_NAME}_REGS->CAN_CCCR & CAN_CCCR_CSA_Msk) != CAN_CCCR_CSA_Msk) | CAN.CCCR.INIT, CCCR.CSA | INIT reads back as written; CSA follows CSR | csp can_u2003/templates/plib_can.c.ftl#L140 |
| sercom i2c master / bus idle and MB (#L928) | while(((${SERCOM_INSTANCE_NAME}_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) != SERCOM_I2CM_STATUS_BUSSTATE(0x01U))) | I2CM.STATUS.BUSSTATE; INTFLAG.MB | BUSSTATE=IDLE after the forced write (#L158) and after STOP; set MB/SB per transfer | csp sercom_u2201/templates/plib_sercom_i2c_master.c.ftl#L883 |
| sercom spi master / blocking transfer (DRE #L472, RXC #L521) | while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_SPI_REG_NAME}.SERCOM_INTFLAG & SERCOM_${SERCOM_SPI_REG_NAME}_INTFLAG_TXC_Msk) != SERCOM_${SERCOM_SPI_REG_NAME}_INTFLAG_TXC_Msk) | SPI INTFLAG TXC/DRE/RXC | Full-duplex SSI transfer sets RXC and TXC | csp sercom_u2201/templates/plib_sercom_spi_master.c.ftl#L543 |

## Table: Model roadmap
| tier | peripheral | key semantics | est. LOC | complexity drivers | validation firmware |
| 0 | CPU/NVIC/SysTick/SCB/MPU (existing QEMU TYPE_ARMV7M) | cpu-type cortex-m0, num-irq 32, 2 priority bits, mpu-ns-regions 8; cpuclk Clock (4 MHz at reset); SYSRESETREQ GPIO wired to RSTC | 0 (+40-60 for an optional cortex-m0plus type) | M0 CPUID instead of M0+; no M_UNPRIV on v6-M; cpu_abort on lockup; no DWT/MTB | DFP templates/main.c, Zephyr hello_world/synchronization, FreeRTOS basic_freertos |
| 0 | SoC + board container, memory map | Flash 512 KB ROM @0, data flash 8 KB @0x00400000, NVM rows ROM 0x00800000-0x0080FFFF (CAL, LOCKBIT, USER, OTP1-5/SW calibration, TEMP_LOG, serial), SRAM 64 KB, IOBUS alias 0x60000000, unimplemented gaps on HPB0-3, OR gates for shared IRQs, ELF/HEX loading including fuse words | 600-900 | Mixed 8/16/32-bit registers; per-part parameters (JH00 has no CAN; 32-100 pins; 16/32/64 KB SRAM; per-part DID); USER-row merge | Any Harmony/Zephyr image |
| 0 | Clock system (OSCCTRL, OSC32KCTRL, GCLK, MCLK) | Ready-immediately RDY/LOCK/CLKRDY, SYNCBUSY=0, GENCTRL/PCHCTRL store and read back, GENCTRL0 reset 0x106, GCLK SWRST, CKRDY, frequency propagation to cpuclk and per-peripheral GCLK Clocks | 350-500 stub; 900-1400 full (cf. stm32l4x5_rcc.c 1477) | 9 generators × 43 channels, FDPLL ratio math, write locks, on-demand clocks, CFD | clock_config, freqm_measure_clock_freq, Class B clock test, Zephyr clock_control_api |
| 0 | NVMCTRL (minimal) | CTRLB store, INTFLAG.READY=1 (re-asserts after W1C), STATUS, PARAM (8192 pages × 64 B, 128 data-flash pages; derived), CTRLA CMDEX key check, INVALL no-op | 150-250 | READY semantics differ from ATDF initval | Every Harmony app; Class B flash test; Zephyr flash init |
| 0 | SUPC | BODVDD reset value from USER row; STATUS.BVDDSRDY/BODVDDRDY=1 when enabled; VREG/VREF store | 100-150 | Fuse-derived reset | Zephyr soc_reset_hook; Harmony SUPC_Initialize |
| 0 | PM | SLEEPCFG/STDBYCFG store and read back; WFI left to the CPU | 60-100 | Sleep timing deferred to Tier 3 | pm_wakeup_eic, pm_wakeup_rtc (basic) |
| 0 | RSTC | RCAUSE latched across qemu_system_reset: POR 0x01 cold, SYST 0x40 via SYSRESETREQ, WDT 0x20, EXT 0x10 via monitor | 80-150 | SRAM must survive warm reset; ordering against ROM reload | rstc_reset_cause, Class B |
| 0 | WDT | Reset from USER row (ENABLE/ALWAYSON/PER/WINDOW/EWOFFSET/WEN); 1.024 kHz base; CLEAR=0xA5, any other value → reset; window and early-warning (EW) IRQ; reset with RCAUSE.WDT | 250-400 (cf. cmsdk-apb-watchdog.c 434) | ALWAYSON lock; window timing in virtual time | wdt_timeout, rstc_reset_cause, Class B WDT test, Zephyr wdt_error_cases |
| 0 | PAC | WRCTRL CLR/SET/SETLCK per PERID; STATUSA-D reset (B=0x2, C=0x09000000); INTFLAGx; optional enforcement (discard + MEMTX error) | 150-300 | Hook into every peripheral write path; double set/clear errors | Class B (PERID 91), dsu_crc32_generate |
| 0 | DSU | DID 0x110601xx (B0) or 0x110602xx (C0); mirror at 0x41002100; optional CRC32 (ADDR/LENGTH/DATA, DONE/BERR) | 40-60 (+150 with CRC) | PAC-protected at reset | dsu_crc32_generate; IDE device-ID checks |
| 0 | DIVAS | Signed/unsigned divide, sqrt, DLZ; BUSY=0 immediately; DBZ; AHB 0x48000000 + IOBUS 0x60000200 | 120-180 | None | divas_c_operator_override (all integer division incl. printf) |
| 0 | MCRAMC, SMBIST, ICM, MTB, FREQM (stubs) | Register files: MCRAMC CTRLA=0x2 and INTSTA W1C; SMBIST DONE immediately, FAIL=0 | 50-120 each (generatable) | Generate from ATDF/EDC | Class B, mcramc_ecc_mbist_testing (partial) |
| 1 | PORT (3 groups) | DIR/OUT plus SET/CLR/TGL, IN = f(DIR, OUT, PULLEN, external), PINCFG/PMUX nibble routing, WRCONFIG, EVCTRL actions, IOBUS alias, per-pin qemu_irq in/out (LED PC05, SW0 PB19) injectable via QMP | 450-650 (cf. stm32l4x5_gpio.c 478) | Byte read-modify-write of 8-bit registers, pull resolution, PMUX → peripheral pin ownership | port_led_on_off_polling, eic_led_on_off, Zephyr gpio_hogs, blinky/button samples |
| 1 | SERCOM core + USART (8 instances) | Register view chosen by CTRLA.MODE; SWRST/ENABLE immediate; Enable-Protected bits logged; DRE/TXC/RXC/RXS/ERROR per DS §30.7.8; STATUS errors W1C; 2-level RX buffer to chardev; optional baud pacing; IRQ = INTFLAG & INTEN, shared vectors; DMAC RX/TX triggers | 700-1000 (cf. stm32l4x5_usart.c 657) | Mode-dependent register map, SERCOM6/7 on HPB3, LIN/auto-baud/RS485/collision later, sync and enable protection | usart_echo_interrupt, rstc demo printf, core_apps usart drivers, Zephyr uart_errors/uart_async_api, MattairTech C21 Serial |
| 1 | EIC | 16 EXTINT lines, SENSE none/rise/fall/both/high/low, FILTEN, DEBOUNCEN, ASYNCH, NMI, EVCTRL, INTFLAG W1C; inputs from PORT pins with PMUX=A (PB19 → EXTINT3) | 300-450 (cf. stm32l4x5_exti.c 293) | Pin → EXTINT map from ATDF signals; level vs edge; async wake | eic_led_on_off, pm_wakeup_eic, Zephyr gpio-keys |
| 1 | TC (8 instances) | COUNT8/16/32 register views, 32-bit pairs, prescaler × GCLK, NFRQ/MFRQ/NPWM/MPWM, one-shot, CTRLBSET CMD self-clear, READSYNC, capture (PPW/PWP), events, DMA, PERBUF/CCBUF | 700-950 | ptimer per instance driven by a GCLK Clock; mode views; double buffering | tc_timer_mode, tc_compare_mode, tc_capture_mode, Zephyr counter_basic_api, pwm_api (TC5) |
| 1 | RTC | MODE0/1/2, prescaler 1-1024, COUNTSYNC, COMP/ALARM, PER_n events; source from OSC32KCTRL.RTCCTRL | 550-800 | Read synchronization, calendar arithmetic, frequency correction | rtc_alarm, pm_wakeup_rtc, Class B clock test, Zephyr rtc/counter |
| 1 | EVSYS | 12 channels, ~100 generators, ~50 users, sync/resync/async paths, EDGSEL, SWEVT, CHSTATUS, OVR/EVD flags | 400-600 | qemu_irq fabric between models; pulse vs level; errata 2.5.x | evsys_trigger, ADC/AC sleepwalking |
| 2 | SERCOM SPI (QEMU SSI bus) | DATA/DRE/RXC/TXC/SSL, DIPO/DOPO, MSSEN hardware SS, 8/9-bit, client preload | 300-450 incremental (cf. stm32f2xx_spi.c 226) | Client mode, preload errata | spi_eeprom_write_read (needs a 25xx device), Zephyr SPI tests |
| 2 | SERCOM I2C (QEMU I2C bus) | ADDR write starts the transfer, MB/SB/ERROR flags, BUSSTATE, ACKACT/CMD, RXNACK, smart mode, client mode | 600-900 incremental | Protocol state machine, repeated start, errata 2.11.x | i2c_eeprom (AT24MAC @0x54 via eeprom_at24c), i2c_eeprom_emulation, Zephyr i2c_target_api |
| 2 | ADC ×2 | INPUTCTRL, REFCTRL, AVGCTRL, SAMPCTRL, RESSEL/DIFF/FREERUN, SWTRIG, window, sequencing, OFFSET/GAIN correction, CALIB from calibration row, master/slave, events, DMA; analog values injected via QOM/QMP | 500-800 (cf. stm32f2xx_adc.c 308) | Conversion timing, averaging math, dual-ADC sync | adc_sample, adc_*_sleepwalking, Zephyr adc_error_cases/adc_accuracy_test |
| 2 | DMAC (12 channels) | Descriptors in SRAM via BASEADDR/WRBADDR, linked lists, BTCTRL fields, 63 triggers with BLOCK/BEAT/TRANSACTION actions, 4 priority levels + round-robin, CHCTRLB CMD, CRC engine, events, INTPEND/BUSYCH/PENDCH | 1200-1800 (pl080.c 471 is simpler) | Descriptor fetch and write-back through address_space DMA; trigger wiring from every peripheral | dmac_usart_echo, dac_wav_gen_dma, adc_dmac_sleepwalking, Zephyr chan_blen_transfer, uart_async_api |
| 2 | CAN-FD ×2 (JH01 only; Bosch M_CAN) | CCCR INIT/CCE/CSR/CSA/FDOE/BRSE/TEST, NBTP/DBTP, message RAM in system SRAM (SIDFC/XIDFC/RXF0C/RXF1C/RXBC/TXBC/TXEFC...), filters, Rx FIFOs/buffers, Tx buffers/FIFO/queue, Tx event FIFO, IR/IE/ILS/ILE, timestamps, ECR/PSR, loopback; QEMU can-bus FD frames | 1800-2500 (cf. xlnx-versal-canfd.c 1941, flexcan.c 1461) | Message-RAM addressing in guest SRAM, write-restricted registers, error/bus-off states; SocketCAN only on Linux hosts | can_fd_operation_blocking, can_fd_operation_interrupt_timestamp, Zephyr can_sam0/can_mcan (samc21n_xpro) |
| 2 | TCC ×3 | 24/16-bit counters, CC channels, WAVE modes, dead time, PATT, faults, DRVCTRL, CTRLBSET CMD, PERBUF/CCBUF, dithering, events, DMA (errata 2.15.2) | 1000-1500 | Double-buffer update rules, fault logic, observable waveform outputs | tcc_synchronous_pwm_channels, Zephyr pwm_api (TCC2 → PC05), counter (TCC1) |
| 2 | AC, DAC, CCL, PDEC, FREQM (full) | AC 4 channels/window with injected inputs; DAC DATA/DATABUF/EMPTY event; CCL 4 LUT + SEQ; PDEC quadrature/Hall; FREQM GCLK ratio | 200-500 each | Analog injection, event fabric, PDEC errata | ac_*, dac_*, ccl manchester_encoder, freqm_measure_clock_freq, Zephyr comparator/dac |
| 2 | NVMCTRL full + DSU CRC/MBIST | Page buffer filled by writes into the flash ROM device; ER/WP/DFER/DFWP/EAR/WAP/LR/UR/PBC/SSB/SCEHL/INVALL; region LOCK and BOOTPROT; LOCKE/PROGE/NVME; persistent backing file | +350-500 (cf. nrf51_nvm.c 396 with rom_device) | romd write interception, row vs page granularity, USER-row rewrite, data flash | nvmctrl_flash_read_write, nvmctrl_data_flash, bootloader_apps, Zephyr flash interface_test/negative_tests |
| 3 | PTC (QTouch) | Acquisition engine that the closed-source QTouch library expects; driven-shield touch button on the board | 800-1500+ | Behaviour not documented for the binary library | QTouch demos (none in csp_apps) |
| 3 | Sleep/standby, on-demand clocks, sleepwalking | RUNSTDBY/ONDEMAND per clock source and peripheral, standby wake sources, errata 2.4.1 (SysTick + standby) | 300-600 cross-cutting | Clock-request modelling across all peripherals | pm_wakeup_*, adc/ac sleepwalking |
| 3 | Timing fidelity | icount for cycle-approximate CPU vs peripheral time; flash wait states; sync delay (5-6 GCLK + 2-3 APB); UART pacing | 200-500 | TCG is not cycle-accurate | Class B clock test, timing-sensitive apps |
| 3 | ECC fault injection and ICM | NVMCTRL FLTCTRL/FFLT* and MCRAMC FLTCTRL/ERRC*, SERR/DERR IRQs and bus errors; ICM SHA engine | 400-800 | Corrupt reads at a chosen address/bit; SHA DMA | nvmctrl_ecc_testing, mcramc_ecc_mbist_testing, Class B |
| 3 | Errata / pedantic mode | Sync-write and enable-protect errors raised through PAC, SWRST-with-ENABLE=0 (2.10.1), MCRAMC INTSTA PAC (2.7.1), FREQM CTRLB read error (2.6.1), I2C status bits not auto-cleared (2.11.1); selected by DID revision | 200-400 | Per-revision switches | Errata DS80001000H scenarios |
| n/a | TSENS / SDADC / USB | Not present on PIC32CM JH (they are SAM C21 / other-part peripherals) | 0 | - | - |

## Table: EV81X90A (PIC32CM JH01 Curiosity Pro) resources the QEMU machine should mirror
| resource | MCU instance / pins | twin mapping | source |
| MCU | PIC32CM5164JH01100 (100-pin, 512 KB flash, 8 KB data flash, 64 KB SRAM) | SoC parameters; DID revision property (B0/C0) | DS70005482B; DS80001000H Table 2 |
| Debugger | EDBG, SWD on PA30 SWCLK / PA31 SWDIO | QEMU gdbstub (-s) in place of SWD | DS70005482B Table 4-32, p.29 |
| Virtual COM | SERCOM4 PAD2 TX = PB10, PAD3 RX = PB11 (DTR needed on the real EDBG) | -serial chardev attached to SERCOM4 | DS70005482B Table 4-33, p.29; Zephyr pic32cm_jh01_cpro-pinctrl.dtsi |
| LED0 (yellow) | PC05, active low (also TCC2 WO1 PWM in Zephyr) | GPIO out → trace/QMP event | DS70005482B Table 4-17, p.19; Zephyr dts led0 |
| SW0 | PB19 to GND; Harmony EXTINT3 rising edge, Zephyr pull-up active low | GPIO in set via QMP/qom-set | DS70005482B Table 4-18, p.19; eic_led_on_off plib_eic.c/plib_port.c |
| Crystals | 32 MHz XOSC, 32.768 kHz XOSC32K | Oscillator frequency properties (XOSCRDY/XOSC32KRDY) | DS70005482B §4.3.3, p.19; Zephyr dts xosc 32 MHz |
| CAN | CAN0 TX PA24 / RX PA25; CAN1 TX PB14 / RX PB15 (ATA6561) | Two can-bus objects (loopback or SocketCAN on Linux) | DS70005482B Tables 4-21/4-22, p.20 |
| LIN | SERCOM0 PAD2 TX PA06 / PAD3 RX PA07; LIN_EN PC06 (ATA663211) | SERCOM0 USART LIN mode (later tier) | DS70005482B Table 4-24, p.21 |
| TA100 secure element | SERCOM5 SPI (PB00 MOSI PAD2, PB01, PB02) + SS PC08 | SSI stub device | DS70005482B Table 4-29, p.22 |
| DGI | SPI: SERCOM7 PC12/PC14/PC15 + SS PC09; I2C: SERCOM1 PA16/PA17 | Optional stubs | DS70005482B Tables 4-34/4-35, p.29 |
| Extension UARTs | EXT1 SERCOM2 PA12/PA13; EXT2 SERCOM3 PB20/PB21; EXT3 SERCOM6 PC16/PC17 | Extra -serial chardevs | DS70005482B pp.12-14 |

## Code: Harmony-generated Reset_Handler RAM_Initialize: WDT-aware SRAM clear (pre-main WDT accesses) (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/startup_xc32.c#L85-L106)
```c
    if (((WDT_REGS->WDT_CTRLA & WDT_CTRLA_ALWAYSON_Msk) != 0U) || ((WDT_REGS->WDT_CTRLA & WDT_CTRLA_ENABLE_Msk) != 0U))
    {
        if ((WDT_REGS->WDT_CTRLA & WDT_CTRLA_WEN_Msk) != 0U)
        {
            for (pRam = (uint32_t*)0x20000000U ; pRam < ((uint32_t*)(0x20000000U + 0x10000U)) ; pRam++)
            {
                *pRam = 0U;

                if ((WDT_REGS->WDT_INTFLAG & WDT_INTFLAG_EW_Msk) == WDT_INTFLAG_EW_Msk)
                {
                    if ((WDT_REGS->WDT_SYNCBUSY & WDT_SYNCBUSY_CLEAR_Msk) == 0U)
                    {

                        /* Clear WDT and reset the WDT timer before the
                        timeout occurs */
                        WDT_REGS->WDT_CLEAR = (uint8_t)WDT_CLEAR_CLEAR_KEY;

                        WDT_REGS->WDT_INTFLAG |= WDT_INTFLAG_EW_Msk;
                    }
                }
            }
        }
```

## Code: Generated OSCCTRL_Initialize: 64-bit calibration-row read plus two busy-waits (OSC48MRDY hangs on a RAZ stub) (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L48-L68)
```c
static void OSCCTRL_Initialize(void)
{
    uint32_t calibValue = (uint32_t)(((*(uint64_t*)0x00806020UL) >> 19 ) & 0x3fffffUL);
    OSCCTRL_REGS->OSCCTRL_CAL48M = calibValue;


    /* Selection of the Division Value */
    OSCCTRL_REGS->OSCCTRL_OSC48MDIV = (uint8_t)OSCCTRL_OSC48MDIV_DIV(0UL);

    while((OSCCTRL_REGS->OSCCTRL_OSC48MSYNCBUSY & OSCCTRL_OSC48MSYNCBUSY_OSC48MDIV_Msk) == OSCCTRL_OSC48MSYNCBUSY_OSC48MDIV_Msk)
    {
        /* Waiting for the synchronization */
    }

    while((OSCCTRL_REGS->OSCCTRL_STATUS & OSCCTRL_STATUS_OSC48MRDY_Msk) != OSCCTRL_STATUS_OSC48MRDY_Msk)
    {
        /* Waiting for the OSC48M Ready state */
    }
    OSCCTRL_REGS->OSCCTRL_OSC48MCTRL |= OSCCTRL_OSC48MCTRL_ONDEMAND_Msk;

}
```

## Code: Generated CLOCK_Initialize: PCHCTRL CHEN read-back loop for SERCOM4_CORE (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/clock/plib_clock.c#L90-L112)
```c
void CLOCK_Initialize (void)
{
    /* Function to Initialize the Oscillators */
    OSCCTRL_Initialize();

    /* Function to Initialize the 32KHz Oscillators */
    OSC32KCTRL_Initialize();

    GCLK0_Initialize();


    /* Selection of the Generator and write Lock for SERCOM4_CORE */
    GCLK_REGS->GCLK_PCHCTRL[22] = GCLK_PCHCTRL_GEN(0x0UL)  | GCLK_PCHCTRL_CHEN_Msk;

    while ((GCLK_REGS->GCLK_PCHCTRL[22] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }



    /* Configure the APBC Bridge Clocks */
    MCLK_REGS->MCLK_APBCMASK = 0x8000020U;
```

## Code: Generated SERCOM4_USART_Initialize (VCOM, 115200 8N1 at 48 MHz) (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01/blob/master/apps/sercom/usart/usart_echo_interrupt/firmware/src/config/pic32cm_jh01_curiosity_pro/peripheral/sercom/usart/plib_sercom4_usart.c#L103-L131)
```c
    SERCOM4_REGS->USART_INT.SERCOM_CTRLA = SERCOM_USART_INT_CTRLA_MODE_USART_INT_CLK | SERCOM_USART_INT_CTRLA_RXPO(0x3UL) | SERCOM_USART_INT_CTRLA_TXPO(0x1UL) | SERCOM_USART_INT_CTRLA_DORD_Msk | SERCOM_USART_INT_CTRLA_IBON_Msk | SERCOM_USART_INT_CTRLA_FORM(0x0UL) | SERCOM_USART_INT_CTRLA_SAMPR(0UL) ;

    /* Configure Baud Rate */
    SERCOM4_REGS->USART_INT.SERCOM_BAUD = (uint16_t)SERCOM_USART_INT_BAUD_BAUD(SERCOM4_USART_INT_BAUD_VALUE);

    /*
     * Configures RXEN
     * Configures TXEN
     * Configures CHSIZE
     * Configures Parity
     * Configures Stop bits
     */
    SERCOM4_REGS->USART_INT.SERCOM_CTRLB = SERCOM_USART_INT_CTRLB_CHSIZE_8_BIT | SERCOM_USART_INT_CTRLB_SBMODE_1_BIT | SERCOM_USART_INT_CTRLB_RXEN_Msk | SERCOM_USART_INT_CTRLB_TXEN_Msk;

    /* Wait for sync */
    while((SERCOM4_REGS->USART_INT.SERCOM_SYNCBUSY) != 0U)
    {
        /* Do nothing */
    }


    /* Enable the UART after the configurations */
    SERCOM4_REGS->USART_INT.SERCOM_CTRLA |= SERCOM_USART_INT_CTRLA_ENABLE_Msk;

    /* Wait for sync */
    while((SERCOM4_REGS->USART_INT.SERCOM_SYNCBUSY) != 0U)
    {
        /* Do nothing */
    }
```

## Code: QEMU precedent: nRF51 clock stub returns 1 so ready-polls pass (hw/arm/nrf51_soc.c:37-42)
```c
static uint64_t clock_read(void *opaque, hwaddr addr, unsigned int size)
{
    qemu_log_mask(LOG_UNIMP, "%s: 0x%" HWADDR_PRIx " [%u]\n",
                  __func__, addr, size);
    return 1;
}
```

## Code: QEMU: Lockup is not modeled; cpu_abort terminates the emulator (reached by BKPT in debug-build fault handlers) (hw/intc/armv7m_nvic.c:665-674)
```c
            if (running <= vec->prio) {
                /* We want to escalate to HardFault but we can't take the
                 * synchronous HardFault at this point either. This is a
                 * Lockup condition due to a guest bug. We don't model
                 * Lockup, so report via cpu_abort() instead.
                 */
                cpu_abort(CPU(s->cpu),
                          "Lockup: can't escalate %d to HardFault "
                          "(current priority %d)\n", irq, running);
            }
```

## Code: Harmony Class B _on_reset path: PAC unlock plus SMBIST DONE poll before main (https://github.com/Microchip-MPLAB-Harmony/classb_pic32cm_jh/blob/master/apps/mplabx/classb_api_usage/firmware/src/config/classb_api_usage/classb/classb.c#L308-L320)
```c
            // Test the SRAM using SMBIST Module
            // SMBIST is no longer PAC secure to perform SMBIST Test
            PAC_REGS->PAC_WRCTRL = (uint32_t)(PAC_WRCTRL_KEY_CLR | PAC_WRCTRL_PERID (91));

            // Clear SMBIST Status Flags
            SMBIST_REGS->SMBIST_STATUS = (uint32_t) SMBIST_STATUS_Msk;
            // Launch the MBIST Test on SRAM
            SMBIST_REGS->SMBIST_CTRL = (uint32_t)(SMBIST_CTRL_SMBISTP1_Msk | SMBIST_CTRL_SMBISTP2_Msk);
            // Wait for Test to Finish
            while ((SMBIST_REGS->SMBIST_STATUS & SMBIST_STATUS_DONE_Msk) == 0)
            {
                // Wait till SMBIST Test is complete
                ;
```

## Code: Datasheet: clocks after reset (defines the model's reset state) (DS60001632H §7.2, p.36 (https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/PIC32CM-JH00-JH01-Family-Data-Sheet-DS60001632.pdf))
```text
After the power has stabilized throughout the device, the device will start with the following default
clock configuration.
•
XOSC, FDPLL96M, XOSC32K, OSC32K are disabled.
•
OSCULP32K is enabled, providing a 1.024 kHz clock to the WDT and RTC.
•
OSC48M is enabled and divided by 12, providing a 4 MHz clock to GCLK0.
•
GCLK Generator 0 is enabled and undivided, providing a 4 MHZ GCLK_MAIN clock to MCLK.
•
GCLK Generators 1 to 8 are disabled.
•
CPU and BUS clocks are undivided, running at 4 MHz.
```

## Open questions
- What are the real production contents of CAL (0x00800000), OTP1-OTP5 (incl. the SW calibration row at 0x00806020), TEMP_LOG, the USER row (including the BODCORE calibration bits 25:17) and the serial-number words on B0/C0 silicon? These should be dumped from a real EV81X90A through EDBG/MPLAB IPE to seed the NVM-rows blob.
- Does the PIC32CM JH Cortex-M0+ implement the unprivileged/privileged extension, and what is its exact CPUID? 0x410CC601 is expected from __CM0P_REV=0x0001. The answer decides whether a cortex-m0plus QEMU CPU type is required for Zephyr userspace fidelity.
- Does 'peripheral returns an access error' (DS60001632H §12.5.2.4) for PAC-protected, enable-protected and sync-collision writes produce an AHB error response, and so a HardFault on M0+, or only the PAC flag? This decides what the pedantic mode should do.
- How should the twin handle a USER row supplied in the HEX/ELF? It could merge masked fields the way MPLAB IPE does (keeping the 'DO NOT CHANGE' BODCORE bits) or take the image verbatim. The programmer's exact behaviour needs to come from the tools team.
- Can MPLAB SIM's pic32sim behavioural models for PIC32CM JH (ready bits, sync, flag semantics) be reused or mined to generate the QEMU behaviour overlay (source access, licensing)?
- Is Microchip's internal datasheet register database (the source of the 'Property:' lines: Enable-Protected, Write-Synchronized, Write-restricted) available in machine-readable form, so these semantics don't have to be scraped from the PDF?
- Fidelity target: is functional (instant-sync, instant-TX) behaviour enough, or does the twin need icount-based timing (sync delays, baud pacing, flash wait states) for the intended use cases?
- Is upstreaming to QEMU a goal (maintainership, licence of generated tables), or will this be an out-of-tree machine? Separately, Windows hosts lack the SocketCAN bridge: is a WSL2/Linux host acceptable for CAN-in-the-loop?
- Two ATDF anomalies need confirmation: the CAN parameter MSG_RAM_ADDR reads '0x200000000' (9 hex digits), and PAC STATUSC resets to 0x09000000 with bit 24 set but unnamed. Which peripheral is protected at bit 24?
- Which Harmony release generated the csp_apps examples that still read CAL48M from 0x00806020, given the current template skips that read for PIC32CM_JH00_JH01? Field firmware will contain both variants, so the twin must tolerate both.
