# microchip-data

## Summary
I examined PIC32CM-JH DFP 1.3.221 (about 91 MB, 24 parts). For each part it ships three machine-readable descriptions: ATDF (about 550 KB), SVD (about 1.3 MB) and EDC .PIC (about 2.2 MB). It also ships CMSIS headers, startup files and linker scripts, and the headers state they were generated from ATDF 2024-08-19T07:01:34Z. For PIC32CM5164JH01100 the ATDF should be the generator's primary input. It is the only source with all of the following: the full memory map (NVM rows and IOBUS aliases included), all 55 instances with IP id/version, all 32 IRQs including the 6 shared vectors, register modes with selector values, pin-mux signals, EVSYS wiring, and per-instance PAC/GCLK/DMA ids. The SVD and EDC carry identical register data (1838/1838 and 1651/1651 reset values match), but the SVD drops the shared IRQs, PTC and FUSES and has no modifiedWriteValues/readAction. The ATDF does carry some behavior hints (access=WSYNC/RWSYNC, atomic-op set/clear/toggle), but they are coarse or wrong for a model. INTFLAG is marked write-1-to-clear as a whole even though DRE/RXC are not. The USART STATUS W1C bits are not marked at all. The SAM C21 pack's copy of the same SERCOM U2201/3.1.1 omits WSYNC on CTRLA. The missing semantics have to come from the datasheet: W1C/RO bits, enable protection, which field maps to which SYNCBUSY bit, bus-error-on-busy, strobes, FIFO side effects, PAC protection and dynamic ready flags. Its 482 register 'Property:' lines and per-bit notes are regular enough to mine into per-IP overlays. Several reset values the twin needs are wrong or missing in the DFP. GENCTRL0 resets to 0x106, port group 0 PMUX15 to 0x06, NVMCTRL.PARAM reads 0x08032000, and SUPC, WDT and NVMCTRL.LOCK load their resets from the user row. The factory user row is 0xB15088FB/0xFFFFF8BB, not the DFP default 0xFFFFFFFF, which would boot with an always-on watchdog and BODVDD off. The serial number and the OSC32K/CAL48M calibration fields are absent from the ATDF. IP ids are shared across families: JH and SAM C21 share id+version for 11 modules (SERCOM included) and id-only for 14, Harmony CSP is organized by these ids, and SAM D21 differs much more. So overlays should be keyed by (id, version range) with structural probes. A prototype (gen/atdf2qemu.py, 206 lines, plus a 39-line overlay sercom_u2201.yaml) emits QEMU REGn/FIELD constants, an MchpRegInfo mask table and a reset routine. The same overlay applied to the SAM C21 ATDF produced an identical table, and the version guard rejected SAM D21. The generated C compiled warning-free and passed W1C and enable-protect checks. Its files are under C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/. On the QEMU side, RegisterAccessInfo (ro/w1c/cor/rsvd plus hooks) matches the overlay vocabulary. Its blocks, however, assume one register width and exact-address hits, so SAM's mixed 8/16/32-bit registers need a small custom byte-lane dispatcher over the generated table.

## Findings

1. (high) [LOAD-BEARING] [Primary source selection] The ATDF is the right primary generator input. SVD, EDC (.PIC) and the CMSIS headers are all derived from the same register data and add almost nothing for code generation.
   detail: Use the SVD only for debugger register views and cross-checks. Use the EDC only for MPLAB X/MPLAB SIM parity (DID mask, config-fuse enumerations, core NMMRs).
   - ev: Flattened comparison: all 1838 ATDF absolute registers (incl. IOBUS aliases) match the SVD with 0 reset-value and 0 access differences (scratchpad/work-microchip-data/atdf_vs_svd.py + svdflat.py)
   - ev: ATDF vs EDC: 1651 matched, 0 reset-value differences, 0 RW registers where EDC per-bit access shows read-only bits; the 187 unmatched are exactly PORT_IOBUS/DIVAS_IOBUS; the only EDC-only register is ICTR @0xE000E004 (scratchpad/work-microchip-data/atdf_vs_edc.py)
   - ev: PIC32CM-JH01/include/component/sercom.h:22 and PIC32CM-JH01/include/pic32cm5164jh01100.h:22: 'generated from device description file (ATDF) version 2024-08-19T07:01:34Z'; ATDF timestamp PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:20
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:2 root element carries atdf:architecture/atdf:family attributes
   - ev: Harmony's configurator itself reads the ATDF: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/clk_sam_c20_c21/config/clk.py lines 204-206

2. (high) [LOAD-BEARING] [ATDF device-level content] A single ATDF gives the whole SoC skeleton: 18 memory segments, 55 instances of 37 module types with IP id/version and base, 32 NVIC lines (6 shared vectors) plus 6 core exceptions, pin mux, EVSYS wiring, and per-instance PAC/GCLK/DMA ids.
   detail: Per-instance parameters: INSTANCE_ID is the PAC peripheral id (SERCOM0=65). GCLK_ID_CORE/GCLK_ID_SLOW give the GCLK channels (SERCOM0 18/17). DMAC_ID_RX/TX give DMA triggers (2/3). NVMCTRL params give the flash geometry: PAGES=8192, PAGE_SIZE=64, DATAFLASH_PAGES=128. The MCLK instance has an AHB clock-group (one bit per AHB client). APBxMASK bitfields are named <INSTANCE>_, so clock gating can be wired by name.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:26-46 (device + memory-segments)
   - ev: atdf:50-58 parameters: NUM_IRQ=32, __CM0P_REV=0x0001, __MPU_PRESENT=1, __NVIC_PRIO_BITS=2, __VTOR_PRESENT=1
   - ev: atdf:1427-1464 interrupts; atdf:1433 SYSTEM index 0 module-instance 'MCLK OSCCTRL OSC32KCTRL PAC PM SUPC'; atdf:1442 SERCOM0_6; atdf:1453 TC0_5
   - ev: atdf:1466-1619 events (99 generators, 49 users); atdf:1625 SIGNATURES DSU_DID=0x11060000
   - ev: Element census (scratchpad structure.py): 553 register defs, 2725 bitfields, 257 value-groups, 578 signals, 344 instance params
   - ev: Extra register-groups: atdf:656 PORT_IOBUS@0x60000000, atdf:253 DIVAS_IOBUS@0x60000200, atdf:279 DSU_EXT@0x41002100 size 0x100

3. (high) [LOAD-BEARING] [SVD gaps] The SVD is not safe as the primary source. It drops the shared IRQ vectors and PTC, has no PTC or FUSES peripheral, encodes no write side effects, and reports CPU revision r0p0 where the ATDF says r0p1.
   detail: A generator built on the SVD (svd2rust-style) would lose the OR'ed vectors for SERCOM0/6, SERCOM1/7, TC0/5, TC1/6, TC2/7 and the SYSTEM line. Where the ATDF has no initval (MPU, MTB, ICM.CTRL), the SVD silently uses the device-level default resetValue of 0.
   - ev: PIC32CM-JH01/svd/PIC32CM5164JH01100.svd: 25 <interrupt> entries; vectors 0 SYSTEM, 9 SERCOM0_6, 10 SERCOM1_7, 20 TC0_5, 21 TC1_6, 22 TC2_7, 30 PTC absent (grep count of those names = 0)
   - ev: SVD counts: modifiedWriteValues 0, readAction 0, writeConstraint 0, alternateRegister 21, cluster 13, derivedFrom 21
   - ev: svd:30 '<revision>r0p0</revision>' vs atdf:52 '__CM0P_REV value="0x0001"'
   - ev: svd:20950 SERCOM clusters use alternateCluster=I2CM without any CTRLA.MODE selector value

4. (high) [EDC (.PIC) content] The EDC adds MPLAB metadata but no new peripheral semantics. Its per-bit access alphabet is only n/r/w/- and simply mirrors the ATDF register-level rw. It omits the IOBUS aliases.
   detail: For SERCOM0/USART_INT, INTFLAG has access 'n-nnnnnn' (plain RW) and STATUS 'nnnnnnn' (CTS included). The EDC therefore encodes neither W1C nor the read-only CTS bit. Its PinList (100 pins, 577 VirtualPins with function letters) duplicates the ATDF signals.
   - ev: Access characters observed {n,-,r,w}; POR characters {0,1,x} (scratchpad edcaccess.py)
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:16787 RegisterMap 'SERCOM0/USART_INT' (75 RegisterMaps: RTC/MODEx, SERCOMn/6 modes, TCn/COUNTx)
   - ev: edc:114-139 NMMRPlace core registers R0..PC, PSR, PRIMASK, CONTROL; edc:15 InstructionSet armv6m
   - ev: edc:15497 DeviceIDSector beginaddr 0x41002018 value 0x11060000 mask 0xFFFFF0FF
   - ev: edc:142-143 ConfigFuseSector DCRDef USER_WORD_0 factorydefault 0xFFFFFFFF impl 0xFC01FF0F

5. (high) [LOAD-BEARING] [ATDF behavior hints and their limits] The ATDF has partial behavior hints: access=WSYNC/RWSYNC on 95 register definitions and atomic-op on 102. They are coarse, ambiguous and inconsistent between packs, so they can seed a behavior overlay but cannot replace it.
   detail: Concrete failure modes of an ATDF-only model. (1) STATUS treated as plain RW: Harmony's 'clear all errors' write would latch PERR/FERR/BUFOVF instead of clearing them. (2) INTFLAG treated as fully W1C: a write of 0xFF would clear DRE and stall a DRE-driven TX ISR. (3) WSYNC alone says neither which SYNCBUSY bit is involved nor that CTRLB sync only applies while enabled.
   - ev: atdf:6137 USART_INT INTFLAG atomic-op='clear:INTFLAG' covers DRE/RXC, but DS60001632H §30.7.8 p.533 says writing 1 to DRE/RXC has no effect (DRE is cleared by writing DATA; RXC by reading DATA or disabling RX)
   - ev: atdf:6183 USART_INT STATUS has no atomic-op, but DS §30.7.9 p.535-537 makes PERR/FERR/BUFOVF/ISF/COLL/TXE write-1-to-clear and CTS read-only
   - ev: atdf:5376 DIRSET atomic-op='set:DIR' (targets another register) vs atdf:6051 INTENCLR atomic-op='clear:INTENCLR' (same syntax as INTFLAG's W1C, but it is a set/clear pair)
   - ev: SAM C21 pack, same SERCOM U2201/3.1.1: USART_INT CTRLA has no access=WSYNC while CTRLB has it (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/dev_packs/master/Microchip/SAMC21_DFP/3.8.119/samc21/atdf/ATSAMC21J18A.atdf lines 9619-9625 vs 9767-9771); JH atdf:5904 CTRLA has WSYNC
   - ev: Harmony USART error path clears STATUS by writing PERR|FERR|BUFOVF and drains DATA while RXC (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/sercom_u2201/templates/plib_sercom_usart.c.ftl lines 128-144)

6. (high) [LOAD-BEARING] [Datasheet as the overlay source] The missing behavior is stated regularly enough in the datasheet to mine semi-automatically. 482 register 'Property:' lines and per-bit notes name PAC protection, enable protection, write/read synchronization and the governing SYNCBUSY bit. Section 12.5.2.4 defines the generic access-error rules.
   detail: The counts are approximate because of PDF text extraction (110 '-' and 31 unrenderable tokens). Since the user is inside Microchip, the best path is to ask whether the internal register database behind both the datasheet register chapters and the ATDF can export these properties, plus per-bit W1C/RO/strobe, directly. Otherwise a regex miner plus one human review per IP version is workable.
   - ev: DS60001632H https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/PIC32CM-JH00-JH01-Family-Data-Sheet-DS60001632.pdf : Property token counts (pdftotext): PAC Write-Protection 261, Write-Synchronized 74, Write-Synchronized Bits 26, Enable-Protected 48(+2), Enable-Protected Bits 21, Read-Synchronized 18, Write-restricted 19
   - ev: DS §30.7.1 p.518: 'This bit is write-synchronized: SYNCBUSY.ENABLE must be checked...' / 'This bit is not enable-protected'
   - ev: DS §30.7.2 p.522: CTRLB.LINCMD is a strobe ('always read back as zero'), write-synchronized via SYNCBUSY.CTRLB, and enable-protected
   - ev: DS §30.7.10 p.537: writing CTRLB while SYNCBUSY.CTRLB is set generates an APB error
   - ev: DS §12.5.2.4 p.62: Peripheral Access Errors = protected write (not written, access error, PAC INTFLAGn set), illegal access, write during synchronization, write to an enable-protected register/bit

7. (high) [LOAD-BEARING] [Reset values ATDF cannot express or gets wrong] A twin reset from ATDF initval alone diverges from silicon. Arrays have only one initval, several resets are loaded from NVM rows or factory calibration, and 47 register definitions have no initval at all.
   detail: The overlay needs per-element reset_overrides, reset_from (user row, OTP1, factory calibration) and a reset-cause input. The ATDF hints at the fuse-to-register mapping through value-group names on USER_FUSES bitfields (WDT_PER values='WDT_CONFIG__PER', BODVDD_ACTION values='SUPC_BODVDD__ACTION'; atdf:4641-4649). DS Table 10-3 states the mapping explicitly in its 'Related Peripheral Register' column.
   - ev: atdf:3930 GCLK GENCTRL count=9 initval 0x00000000 vs DS §14.7.3 p.129 'Reset: 0x00000106 for GENCTRL0, 0x00000000 for others'
   - ev: atdf:5425 PORT PMUX count=16 initval 0x00 vs DS §27.7.13 p.472 'Reset: 0x00 except group 0 PMUX15 = 0x06'; DS §7.4 p.37 PA30 comes out of reset in SWCLK peripheral mode with pull-up, vs atdf:5429 PINCFG initval 0x00
   - ev: DS: SUPC INTFLAG/STATUS/BODVDD 'Reset: X determined from NVM User Row' (§18.6, p.219); WDT CTRLA/CONFIG/EWCTRL same (§22.6.1, p.255); NVMCTRL.LOCK 'Loaded at start-up from NVM User Row' (§26.6.9 p.435); OSCCTRL.CAL48M 'Calibrated value for VDD 3.6-5.5 V' (§17.7.18 p.209) vs ATDF 0x00200220
   - ev: NVMCTRL.PARAM: DS §26.6.3 p.425 reset 0x000XXXXX and all bits R, vs atdf:4431 rw=RW initval 0x00000000
   - ev: No initval: atdf:5477 RSTC.RCAUSE, OSC32KCTRL.OSCULP32K (DS reset 0x0000XX06), MPU, MTB, ICM, DMAC/ICM descriptors, FUSES, SysTick RVR/CVR (scratchpad listing, 47 defs)

8. (high) [LOAD-BEARING] [NVM user row: DFP default vs production] The DFP default for the user row is 0xFFFFFFFF (ATDF initval, EDC factorydefault and XC32 CWORD all agree). The production value is 0xB15088FB / 0xFFFFF8BB, and the twin must preload it; with the erased value, POR would enable an always-on WDT and disable BODVDD.
   detail: The erased row decodes to WDT_ENABLE=1, WDT_ALWAYSON=1, PER=0xF, BODVDD_DIS=1, ACTION=3. BOOTPROT=0xB (4096 B) protects the factory UART/I2C bootloader that occupies flash 0-4 KB on shipped parts (DS p.1). That bootloader image is not in the DFP, so a 'fresh part' twin would need it from elsewhere.
   - ev: DS60001632H Table 10-3 p.50-51 production settings: BOOTPROT 0xB, [7:4] 0xF, BODVDD level 0x8, BODVDD disable 0, action 0x1, BODCORE cal [25:17] 0x0A8 (DO NOT CHANGE), WDT enable 0, always-on 0, PER 0xB, WINDOW 0xB, EWOFFSET 0xB, WEN 0, HYST 0, bit42 0, [47:43] 0x1F, LOCK 0xFFFF; computed with these bit positions -> 0xB15088FB / 0xFFFFF8BB
   - ev: atdf:4641 USER_WORD_0 initval 0xFFFFFFFF
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:143 factorydefault 0xFFFFFFFF, impl 0xFC01FF0F (BODCORE bits 25:17 hidden)
   - ev: PIC32CM-JH01/xc32/PIC32CM5164JH01100/configuration.data:2 and :62 'CWORD:00804000:FFFFFFFF:FFFFFFFF' / 'CWORD:00804004:...'

9. (high) [LOAD-BEARING] [Calibration row, serial number and other NVM data] The Software Calibration Row is only partly described in the ATDF, and the 128-bit serial number not at all. The twin needs a separately supplied NVM image covering roughly 0x00800000-0x0080A04B.
   - ev: atdf:36 OTP5 segment @0x00806020 size 8 = DS 'NVM Software Calibration Row' @0x00806020 (§10.2.2 p.51)
   - ev: ATDF OTP5_WORD_0 (atdf:4633) and PIC32CM-JH01/include/component/fuses.h:87-100 define only bits 11:0 (ADC0/ADC1 BIASREFBUF/BIASCOMP, mask 0x00000FFF); DS Table 10-4 adds OSC32K CAL [18:12], CAL48M [40:19], reserved [63:41]
   - ev: Harmony loads OSC32K.CALIB = (*(uint32_t*)SW_CALIB_ROW_ADDR >> 12) & 0x7F (plib_clock.c.ftl line 188) and skips CAL48M for PIC32CM_JH00_JH01 (lines 116-124); ADC CALIB from the same row (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/adc_u2247/templates/plib_adc.c.ftl lines 224-229)
   - ev: Serial number words 0x0080A00C, 0x0080A040, 0x0080A044, 0x0080A048 (DS §10.3 p.51) lie in no ATDF/SVD/EDC segment
   - ev: CAL 0x00800000 and TEMP_LOG 0x00806030 (atdf:30, atdf:37) have no field definitions; DS Figure 8-1 p.38 labels them only 'CAL/AUX' / 'Internal Data'

10. (high) [LOAD-BEARING] [Dynamic status bits firmware spins on] Boot code hangs unless the model synthesizes ready / not-busy status. Harmony polls OSC48MSYNCBUSY, OSC48MRDY/XOSCRDY, DPLLSYNCBUSY, DPLLSTATUS LOCK|CLKRDY, OSC32KCTRL.STATUS, GCLK.SYNCBUSY, PCHCTRL.CHEN, MCLK.CKRDY, NVMCTRL.INTFLAG.READY, SUPC.STATUS, and WDT/SERCOM SYNCBUSY.
   - ev: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/clk_sam_c20_c21/templates/plib_clock.c.ftl polling loops at lines 106, 141, 147, 179, 199, 216, 237, 246, 254, 260, 290, 307, 339
   - ev: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/nvmctrl_03929/templates/plib_nvmctrl.c.ftl lines 313-316: IsBusy = (NVMCTRL_INTFLAG & READY) == 0
   - ev: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/supc_u2117/templates/plib_supc.c.ftl lines 287-294; .../wdt_u2251/templates/plib_wdt.c.ftl lines 89-147; .../sercom_u2201/templates/plib_sercom_usart.c.ftl line 199
   - ev: ATDF initval for NVMCTRL.INTFLAG and OSCCTRL.STATUS is 0 (DS also lists 0); these are momentary reset values, not steady state

11. (high) [LOAD-BEARING] [IP id/version reuse across families] IP ids/versions are shared across families, so overlays should be keyed by (module id, version range). JH and SAM C21 share id+version for 11 modules and the id alone for 14 more; SAM D21 differs much more.
   - ev: scratchpad ipcompare.py over JH ATDF vs https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/dev_packs/master/Microchip/SAMC21_DFP/3.8.119/samc21/atdf/ATSAMC21J18A.atdf: same id+version = AC, DIVAS, EVSYS, FREQM, MCLK, MTB, PAC, RSTC, SERCOM(U2201/3.1.1), SUPC, WDT; same id only = ADC, CAN, CCL, DAC, DMAC, DSU, EIC, GCLK, OSC32KCTRL, OSCCTRL, PM, PORT, TC, TCC
   - ev: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/dev_packs/master/Microchip/SAMD21_DFP/3.6.144/samd21a/atdf/ATSAMD21J18A.atdf line 836: SERCOM U2201/2.0.0 (no CTRLC, no CTRLB.LINCMD); D21 GCLK U2102, PM U2206, EIC U2217, EVSYS U2208, PAC U2211, WDT U2203, RTC U2202, TC U2212
   - ev: Harmony CSP peripheral folders are named by IP id (sercom_u2201, port_u2210, nvmctrl_03929, tc_u2249, tcc_u2213, wdt_u2251, supc_u2117, pac_u2120, rstc_u2239, pm_u2240, rtc_u2250 ...) plus shared clk_sam_c20_c21: https://api.github.com/repos/Microchip-MPLAB-Harmony/csp/contents/peripheral
   - ev: Harmony probes the ATDF for layout differences inside one id: https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/sercom_u2201/config/sercom.py lines 721-726 (SYNCBUSY register present?)
   - ev: Prototype: the unchanged overlay applied to the SAM C21 ATDF gave an identical MchpRegInfo table; SAM D21 was rejected by the version guard

12. (high) [JH-specific and opaque IPs] Eight JH modules have no SAM C21 counterpart and need new overlays and behavior code. PTC is opaque: its module defines zero registers, and its instance and module version strings disagree.
   detail: Without an internal spec, PTC can only be stubbed (register window 0x42005400, IRQ 30), and QTouch-library firmware will not run against a stub. RTC 03608 appears derived from U2250 (Harmony rtc_u2250), so start its overlay as a reviewed diff of the U2250 one.
   - ev: atdf:546 / atdf:4418 NVMCTRL 03929/1a0 name2 'pfm_ctrl_c21_apb_v1' (19 registers incl. ECC/fault injection vs 11 in SAM C21 U2207)
   - ev: atdf:883 / atdf:5487 RTC 03608/3a1 name2 'tmr_rtc_u2250_v3'; MCRAMC 03727 'drm_mcramc_v3'; SMBIST 03788; ICM U2010; PDEC U2263
   - ev: atdf:756 PTC instance version 7a0 vs atdf:5472 module version 7b0 with 0 registers; no PTC peripheral in the SVD; EDC lists PTC IRQ/pins but no SFRs
   - ev: Harmony has a dedicated nvmctrl_03929 library (raw.githubusercontent.com/.../peripheral/nvmctrl_03929/templates/plib_nvmctrl.c.ftl)

13. (medium) [How ATDF expresses alternate layouts] The ATDF expresses alternate layouts at one base in three ways. First, register-group <mode> selectors (qualifier/mask/value) with registers tagged modes=. Second, register-level <mode> children with bitfield@modes. Third, repetition through nested groups and count attributes.
   detail: Every SERCOM mode carries mask='6'. Applied to CTRLA.MODE, that mask would make USART_EXT/USART_INT indistinguishable, so treat value as the selector until checked against the ATDF schema (avr_tools_device_file.xsd is referenced but not shipped in the pack). The descriptor groups can drive generated decoders for DMA descriptors and CAN message RAM.
   - ev: atdf:5831-5836 SERCOM modes I2CM=5, I2CS=4, SPIS=2, SPIM=3, USART_EXT=0, USART_INT=1, qualifier '<MODE>_CTRLA.MODE', all mask='6'
   - ev: USART_INT BAUD register-level modes DEFAULT/FRAC/FRACFP/USARTFP (qualifier SERCOM.USART.CTRLA.FORM) with bitfield@modes: BAUD 0xFFFF vs BAUD 0x1FFF + FP 0xE000 (atdf near 6002-6006); TCC COUNT/PER/CC/PERBUF/CCBUF modes DITH4/5/6 on TCC.CTRLA.RESOLUTION; RTC MODE0-2 and TC COUNT8/16/32 group modes
   - ev: atdf:5437 PORT nested GROUP count=3 size=0x80; count arrays GCLK PCHCTRL 43, GENCTRL 9, PORT PMUX 16, PINCFG 32
   - ev: 'aligned' groups describe RAM-resident structures: DMAC_DESCRIPTOR (aligned 8), CAN_RXBE/RXF0E/RXF1E/TXBE/TXEFE/SIDFE/XIDFE (aligned 4), ICM_DESCRIPTOR

14. (medium) [LOAD-BEARING] [QEMU register API fit] QEMU's RegisterAccessInfo already has the masks an overlay produces (ro, w1c, cor, rsvd, unimp, plus pre/post hooks). Its block helpers, though, assume one register width per block and exact-address hits, which does not fit SAM's mixed 8/16/32-bit registers at byte offsets. Emit a table plus a small byte-lane MemoryRegionOps dispatcher instead.
   detail: Even with one register block per width, a legal 32-bit APB access at 0x18 spans INTFLAG (u8) and STATUS (u16), so custom handling is still needed. The REGn/FIELD macros in include/hw/core/registerfields.h:21-46 remain the idiomatic way to emit field constants, and the prototype uses them.
   - ev: include/hw/core/register.h:45-60 (struct RegisterAccessInfo)
   - ev: hw/core/register.c:103-108 (no_w_mask = ro | w1c | rsvd | ~we; w1c clear)
   - ev: hw/core/register.c:187-198 (register_write_memory requires access->addr == addr)
   - ev: hw/core/register.c:263-269 (index = addr / data_size; one data_size per block)
   - ev: system/memory.c:559 (access_size widening; no merge with neighbouring bytes)
   - ev: PIC32CM-JH01/include/component/sercom.h sercom_usart_int_registers_t: u8 INTENCLR@0x14, u8 INTENSET@0x16, u8 INTFLAG@0x18, u16 STATUS@0x1A, u16 DATA@0x28

15. (medium) [LOAD-BEARING] [QEMU core/SoC mapping of DFP core parameters] The DFP core parameters map onto existing QEMU properties, with one gap: QEMU models only cortex-m0 (CPUID 0x410CC200), so the twin should override midr with the Cortex-M0+ value. The shared vectors need OR gates.
   detail: Cortex-M0+ CPUID is 0x410CC601 for r0p1 (per ATDF) or 0x410CC600 for r0p0 (per SVD), computed from PARTNO 0xC60; confirm on silicon. The 8-region MPU count comes from Arm architecture knowledge, not the DFP (MPU.TYPE has no initval). The startup code writes SCB->VTOR (PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:174), which QEMU supports. IOBUS (0x60000000) can be a MemoryRegion alias of PORT/DIVAS.
   - ev: target/arm/tcg/cpu-v7m.c:45-52 cortex_m0_initfn midr 0x410cc200; target/arm/tcg/cpu-v7m.c:326 only 'cortex-m0'
   - ev: target/arm/cpu.c:2504 DEFINE_PROP_UINT64("midr", ...)
   - ev: hw/arm/armv7m.c:259-262 aliases num-irq / num-prio-bits; hw/arm/armv7m.c:558-565 init-svtor, init-nsvtor, mpu-ns-regions
   - ev: hw/intc/armv7m_nvic.c:2650, 2773 num-prio-bits (default 2 for v6-M); hw/intc/armv7m_nvic.c:1109, 1638 VTOR read/write implemented
   - ev: include/hw/core/or-irq.h:31 TYPE_OR_IRQ (used e.g. by hw/arm/armsse.c)
   - ev: No SAM/SERCOM models in hw/ or include/ (grep); Microchip code upstream is only hw/char/avr_usart.c, hw/misc/avr_power.c and PolarFire mchp_pfsoc_* files

16. (high) [Device identification] DSU.DID for PIC32CM5164JH01100 is 0x11060X00, where X is the silicon revision (B0=1, C0=2). The ATDF initval 0x11060100 corresponds to B0. Tools compare DID under mask 0xFFFFF0FF, and the DSU registers are mirrored at +0x100 for debugger accesses.
   - ev: atdf:3406 DID initval 0x11060100; atdf:1625 DSU_DID 0x11060000; atdf:3467-3488 value-groups (PROCESSOR CM0P=1, FAMILY 5V_Industrial=2, SERIES JH00_JH01=6, DEVSEL 512KB_64KB_100PIN_CAN=0x0)
   - ev: edc:15497 DeviceIDSector mask 0xFFFFF0FF
   - ev: Errata DS80001000H Table 2: PIC32CM5164JH01100 0x11060X00, revision B0=0x1, C0=0x2 (https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/Errata/PIC32CM-JH00-JH01-Family-Silicon-Errata-and-Data-Sheet-Clarifications-DS80001000.pdf)
   - ev: DS p.88 (DSU): first 0x100 bytes mirrored at offset 0x100 for external accesses; matches atdf:279 DSU_EXT and SVD DSU_EXT derivedFrom DSU
   - ev: ATDF DID initvals are inconsistent for some JH00 parts: PIC32CM3204JH00032 initval 0x1100050D vs signature 0x1106001D

17. (high) [LOAD-BEARING] [Prototype generator] The prototype turns the ATDF SERCOM USART_INT group plus the YAML overlay into QEMU REGn/FIELD constants, an MchpRegInfo table (reset, impl, wmask, w1c, rdz, enprot, syncbusy, flags) and a reset routine. The generated C compiles cleanly and behaves as the datasheet describes.
   detail: Not yet implemented: the byte-lane dispatcher, the alias storage (INTEN), the hooks themselves, PAC checks and the irq_source key, which the overlay declares but the generator does not consume. The overlay's version guard and its 'unknown register/field' errors catch drift between ATDF releases.
   - ev: C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/gen/atdf2qemu.py (206 lines) and gen/overlays/sercom_u2201.yaml (39 lines)
   - ev: Outputs: .../work-microchip-data/out/sercom_usart_int_jh.h, run_jh.txt, sercom_usart_int_c21.h, run_c21.txt (JH and C21 tables identical)
   - ev: out/check.c compiled with MinGW gcc -std=c11 -Wall -Wextra (no warnings): INTFLAG 0xBF after writing 0xFF -> 0x05 (DRE/RXC survive); CTRLA MODE write while enabled -> unchanged plus access error; while disabled -> accepted
   - ev: Generator report: 'INTFLAG: ATDF atomic-op=clear:INTFLAG implies w1c=0xBF, overlay says 0xBA'; 'STATUS: W1C bits 0x77 come only from overlay'; an ATDF-only run flags CTRLA/CTRLB 'access=WSYNC but no SYNCBUSY mapping'

18. (high) [DFP folder roles] Beyond the three description files, the DFP mainly contributes the boot contract and toolchain files. Most of it is irrelevant to peripheral models.
   - ev: PIC32CM-JH01/gcc/system_pic32cm5164jh01100.c: __SYSTEM_CLOCK 4000000, SystemInit keeps reset state
   - ev: PIC32CM-JH01/gcc/gcc/startup_pic32cm5164jh01100.c:153-182 Reset_Handler copies .data, sets SCB->VTOR, calls __libc_init_array
   - ev: Microchip.PIC32CM-JH_DFP.pdsc:4284-4310 device memories IROM1 0x0/0x80000, IROM2 0x400000/0x2000, IRAM1 0x20000000/0x10000, Keil FLM algorithms
   - ev: PIC32CM-JH01/hwtools/sdm/sdmSimulator.xml declares a 'sim' profile; PIC32CM-JH01/hwtools/persfr/arm32.persfr is a generic SAM3/4-style SFR grouping
   - ev: scripts/dap_cortex-m0plus.py, j-link_cortex-m0plus.py (DAP/FPB primitives), samba.py (SAM-BA applets), properties_device.py (erase clears BOOTPROT/region locks)

19. (high) [Simulator.emu vs DFP device list (tool-support side note)] Simulator.emu maps 6 PIC32CM JH part numbers that have no ATDF/EDC in DFP 1.3.221. All 24 DFP parts are mapped to pic32sim.
   - ev: C:/Users/C19257/Desktop/Code/DTS Test/dts-530/Simulator.emu:3005-3012 lists PIC32CM1216JH00032/048/064/100 and PIC32CM1216JH01064/01100
   - ev: DFP 1.3.221 JH00 atdf has no 1216 parts; JH01 has only PIC32CM1216JH01032/048

20. (high) [PIC32CM MC00 ATDF errors found later (DFP 1.4.67)] Two more places where the ATDF is wrong
   or incomplete for the PIC32CM1216MC00048. Silicon or the data sheet outranks it in both.
   - AC U2245 COMPCTRL bits 21:20: in neither the ATDF nor DS60001638E, but writable and read back on
     rev A1 silicon (they change nothing measurable). The overlay declares them with
     `extra_fields: {HYST: [20, 2]}` so the generated impl/wmask (0x373BF75E) match silicon
     (`docs/research/peripherals/ac-u2245.md` section 7).
   - DMAC U2223 trigger numbers: the value-group `DMAC_CHCTRLB__TRIGSRC` (and the dmac.h
     `DMAC_CHCTRLB_TRIGSRC_*_Val` macros made from it) carries SAM C21 numbers from 0x0A up
     (TCC0_OVF = 0x10, TC0_OVF = 0x1B). DS Table 24-2, the per-instance `DMAC_ID_*` parameters,
     `devices/*.json dmac_triggers` and Harmony agree on 0x00-0x27 without gaps (TCC0_OVF = 0x0A).
     Use the instance parameters (`docs/research/peripherals/dmac-u2223.md`).

21. (high) [PIC32CM MC00 data sheet gaps found on silicon (DS60001638E)] Places where rev A1
   silicon does what DS60001638E does not say, or not what it says (each probed and checked; see
   the research note named):
   - WDT: a CLEAR write with a wrong key resets the part also while the WDT is disabled; the data
     sheet only describes it for a running WDT (`wdt-u2251.md`).
   - NVMCTRL: ADDR is 16 bits wide and relative to the array (0 for the first data flash word),
     where the data sheet shows ADDR[21:0] and implies absolute addresses; 16-bit page buffer
     writes load like 32-bit ones, and 8-bit writes fault but still load their halfword, where
     the data sheet says both fault (`nvmctrl-03929.md`).
   - TC: CTRLA bit 15 is writable with no documented field; CAPTMIN/CAPTMAX are described (33.6.3.3)
     but CTRLA.CAPTMODE does not exist on this part (nor in the DFP) (`tc-u2249.md`).
   - TCC: a STOP leaves COUNT at 0 (TOP counting down), where 31.6.2.4 says the counter keeps its
     value; WAVE and PATT drop a write made while the previous one synchronizes (about 4 GCLK
     cycles), which the data sheet does not mention; WAVE bit 6 (RAMP's third bit) is writable
     though the MC00 ATDF gives RAMP as 0x30 and the data sheet has no RAMP2CS; STATUS.CMPx reads
     0 a few cycles after a STOP with prescaler DIV1 only (`tcc-u2213.md`).
   - TCC faults: a recoverable fault acts only with EVCTRL.MCEIx set, which 34.6.3.5 does not
     say; fault capture fills a channel only in capture mode (CTRLA.CPTENx); BLANK = START does
     not blank the first cycle after the enable; STATUS shows a fault 5-8 GCLK cycles late
     (`tcc-u2213.md` section 5).
   - TCC capture and buffers: a CCBUFx or PERBUF written before the enable waits for the first
     update condition; a PER or CCx write also writes its buffer; DSBOTH's circular PER swaps
     only at ZERO while CCx swaps at TOP and ZERO; STATUS.FAULTnIN shows EVn for any event
     action (`tcc-u2213.md` section 6).
   - DAC: EVCTRL is enable-protected as 39.6.2.1 says, though its register description (39.7.3)
     lists only PAC protection; SYNCBUSY.DATA and DATABUF stay set from a DATABUF write until a
     START event moves it (forever with STARTEI = 0), so writes meanwhile are dropped; every
     enable sets EMPTY, even with DATABUF full; with DITHER, EMPTY comes every 16th START from
     the enable and UNDERRUN only on the START opening a group, while a full DATABUF moves at any
     START (`dac-u2214.md` section 3).
   - PM and standby: SLEEPCFG resets to 0, a value Table 16-1 calls reserved; in standby a
     requested GCLK channel runs on demand even with its generator's and OSC48M's RUNSTDBY at
     0, where Table 16-2 says such a source stops; a channel (not a peripheral) is gated, so a
     TC without RUNSTDBY counts when another peripheral on its channel asks for it
     (`pm-u2240.md` section 3).

## Table: DFP inventory
| folder / file | contents | approx size | contribution to a QEMU twin |
| Microchip.PIC32CM-JH_DFP.pdsc (+ package.content, .sha1) | CMSIS-Pack index of 24 devices (JH00: 14, JH01: 10): memories IROM1 0x0/0x80000, IROM2 0x400000/0x2000, IRAM1 0x20000000/0x10000; flash algorithms; Microchip extension pointing at atdf/edc/xc32/hwtools | 332 KB (+336 KB, 80 KB) | Device list and file index for the generator front-end; memory cross-check |
| PIC32CM-JH0x/atdf/*.atdf | ATDF schema 4.4 per device: variants, address-spaces/memory-segments, peripherals (instances, register-groups, signals, params), interrupts, events, property-groups, modules (register/bitfield/value-group/mode), pinouts | JH01 5.0 MB, JH00 5.9 MB (~557 KB per device) | PRIMARY input: memory map, instances and IP id/version, IRQs incl. shared, register layouts and modes, pin mux, EVSYS/DMA/GCLK/PAC wiring, fuse/OTP layout |
| svd/*.svd | CMSIS-SVD 1.3 per device: cpu block, 56 peripherals (21 derivedFrom), clusters/alternateCluster for modes, 306 enumeratedValues | JH01 13 MB (~1.35 MB per device) | Debugger/IDE register views and cross-check only; lacks shared IRQs, PTC, FUSES, side effects |
| edc/*.PIC | MPLAB X EDC: per-bit access/por/mclr strings, RegisterMaps per mode, core NMMRs, DeviceIDSector, ConfigFuseSector DCRs, PinList | JH01 18 MB (~2.28 MB per device) | MPLAB X / MPLAB SIM (pic32sim) parity; DID mask and fuse enumerations; no new peripheral semantics |
| include/ (component 33, instance 51, pio 10, 10 device headers, pic32c.h, system_pic32cmjh01.h, component-version.h) | CMSIS headers generated from ATDF 2024-08-19T07:01:34Z: register structs with natural widths, IRQn, CMSIS config macros, *_BASE_ADDRESS, memory macros | ~4.0 MB | Build test firmware; struct layouts fix the access widths the model must accept |
| gcc/ (startup_*.c, system_*.c, *_flash.ld, *_sram.ld) | Vector table + Reset_Handler (copies .data, writes SCB->VTOR, calls __libc_init_array); SystemInit no-op; SystemCoreClock 4 MHz | 336 KB | Boot contract the twin must honour; smoke-test images |
| xc32/<device>/ (ld, startup, specs, configuration.data) + include/xc.h | XC32 linker/startup; configuration.data lists CWORD 0x804000/0x804004 defaults 0xFFFFFFFF and #pragma config names/values | 528 KB | Config-fuse naming; its default differs from the production user row |
| armcc/, iar/, keil/ | Other toolchain startup/linker files, IAR ddf/i79/menu, Keil .FLM flash algorithms | ~0.9 MB | Not needed (FLM only for debug-probe emulation) |
| templates/ | main.c/main.cpp/library.c skeletons | 4 KB | None |
| hwtools/ (mplab/, sdm/, persfr/) | MPHeader.xml, PluginBoardSupport.xml, sdm debugger capability maps incl. sdmSimulator.xml ('sim' profile), generic arm32.persfr | 61 KB | MPLAB X integration only |
| scripts/ | dap_cortex-m0plus.py / j-link_cortex-m0plus.py (DAP read/write, FPB at 0xE0002000), samba.py (SAM-BA applets), properties_device.py (erase clears BOOTPROT and region locks) | 105 KB | Only for debug-probe / bootloader parity |

## Table: ATDF vs SVD vs EDC (PIC32CM5164JH01100)
| aspect | ATDF | SVD | EDC (.PIC) |
| Consistency | Source; headers say generated from it | 1838/1838 ATDF registers identical reset + access | ATDF-derived (atdf: attributes); 1651/1651 identical resets |
| Reset values | initval; 47 defs missing (MPU, MTB, ICM, DMAC/ICM descriptors, FUSES, RCAUSE, OSCULP32K, SysTick RVR/CVR) | resetValue; defaults to 0 where ATDF has none | por and mclr per bit ('x' = unknown) |
| Access granularity | register rw R/W/RW; access=WSYNC (74) / RWSYNC (21) | register access only | per-bit n/r/w/-, but mirrors ATDF register rw (0 refinements) |
| W1C / side effects | atomic-op on 102 defs (clear:INTFLAG W1C; set/clear/toggle aliases); STATUS W1C missing | modifiedWriteValues 0, readAction 0, writeConstraint 0 | none |
| Alternate layouts | register-group mode (qualifier, mask, value) + register@modes; register-level modes + bitfield@modes | cluster + alternateCluster (no selector); alternateRegister | RegisterMap per mode (no selector) |
| Interrupts | 32 IRQs + 6 exceptions; shared vectors list all module-instances | 25: drops 0, 9, 10, 20, 21, 22, 30 | 38 incl. shared |
| Pin mux | signals (group/index/function/pad) + pinout CMJH100 | none | PinList / VirtualPin ppsfunction |
| NVM rows / fuses | memory segments + FUSES module (USER, OTP1-5, LOCKBIT) | none | ConfigFuseSector DCRs with value semantics; factorydefault 0xFFFFFFFF |
| IOBUS aliases | PORT_IOBUS, DIVAS_IOBUS register-groups | PORT_IOBUS, DIVAS_IOBUS peripherals | absent |
| Wiring metadata | events (99 generators / 49 users), DMAC_ID_*, GCLK_ID*, INSTANCE_ID (PAC), MCLK clock-group | none | none |
| PTC | instance + module with 0 registers (7a0 vs 7b0) | absent | listed, IRQ + pins, no SFRs |
| Core configuration | __CM0P_REV 0x0001, MPU 1, NVIC prio 2, VTOR 1, NUM_IRQ 32 | CM0+ r0p0, mpu, vtor, prio 2, 32 IRQs | core NMMRs, armv6m |
| Device ID | SIGNATURES DSU_DID 0x11060000; DID initval 0x11060100 | none | DeviceIDSector 0x41002018 = 0x11060000 / mask 0xFFFFF0FF |

## Table: IP id/version across families
| module | PIC32CM5164JH01100 | ATSAMC21J18A | ATSAMD21J18A | reuse of a JH overlay |
| AC | U2245/1.1.1 | U2245/1.1.1 | U2205/1.1.1 | C21 as-is |
| ADC | U2247/2.4.0 | U2247/2.2.0 | U2204/1.2.0 | C21 reviewed diff |
| CAN | U2003/2.1.0 | U2003/2.0.0 | - | C21 reviewed diff |
| CCL | U2225/2.0.0 | U2225/1.1.0 | - | C21 reviewed diff |
| DAC | U2214/2.1.0 | U2214/2.0.1 | U2214/1.1.0 | reviewed diff |
| DIVAS | U2258/1.0.0 | U2258/1.0.0 | - | C21 as-is |
| DMAC | U2223/2.4.0 | U2223/2.2.3 | U2223/1.0.0 | reviewed diff |
| DSU | U2209/3.0.0 | U2209/2.5.0 | U2209/2.0.0 | reviewed diff |
| EIC | U2254/3.1.0 | U2254/2.0.4 | U2217/1.0.1 | C21 diff; D21 new |
| EVSYS | U2256/1.0.2 | U2256/1.0.2 | U2208/1.0.1 | C21 as-is |
| FREQM | U2257/1.0.1 | U2257/1.0.1 | - | C21 as-is |
| GCLK | U2122/1.1.2 | U2122/1.1.1 (SRC mask 0x7 vs 0x1F, no WSYNC tag) | U2102/2.1.0 | C21 diff; D21 new |
| MCLK | U2234/2.0.0 | U2234/2.0.0 | - (PM U2206) | C21 as-is |
| MTB | U2002/1.0.0 | U2002/1.0.0 | U2002/1.0.0 | as-is |
| NVMCTRL / FUSES | 03929/1a0 | U2207/4.0.1.1 | U2207/1.0.6 | new (JH-only) |
| OSC32KCTRL | U2246/2.2.0 | U2246/2.1.0 | - (SYSCTRL) | C21 reviewed diff |
| OSCCTRL | U2119/2.2.0 | U2119/2.1.0 (CAL48M reset 0 vs 0x00200220) | - (SYSCTRL) | C21 reviewed diff |
| PAC | U2120/1.1.0 | U2120/1.1.0 | U2211/1.0.1 | C21 as-is |
| PDEC | U2263/1.0.0 | - | - | new |
| PM | U2240/2.1.1 | U2240/2.1.0 | U2206/2.0.1 | C21 reviewed diff |
| PORT | U2210/3.1.0 | U2210/2.1.1 (same registers) | U2210/1.0.0 | C21 reviewed diff |
| PTC | 03795/7b0 (0 registers) | U2215/3.1.0 | U2215/1.0.2 | stub |
| RSTC | U2239/2.0.2 | U2239/2.0.2 | - | C21 as-is |
| RTC | 03608/3a1 (tmr_rtc_u2250_v3) | U2250/1.1.1 | U2202/1.0.1 | derive from U2250, review |
| SERCOM | U2201/3.1.1 | U2201/3.1.1 (overlay gave identical table) | U2201/2.0.0 (no CTRLC/LINCMD; rejected) | C21 as-is |
| SUPC | U2117/2.1.1 | U2117/2.1.1 | - | C21 as-is |
| TC | U2249/3.0.0 | U2249/2.0.0 | U2212/1.2.1 | C21 reviewed diff |
| TCC | U2213/4a0 | U2213/3.0.0 | U2213/1.0.1 | reviewed diff |
| WDT | U2251/1.0.1 | U2251/1.0.1 | U2203/2.0.0 | C21 as-is |
| ICM / MCRAMC / SMBIST | U2010/1.2.0, 03727/3a0, 03788/1a0 | - | - | new |

## Table: Core configuration
| parameter | value | source |
| CPU core | Arm Cortex-M0+ (ARMv6-M), little-endian, no FPU | atdf:26 architecture=CORTEX-M0PLUS; svd:29; Microchip.PIC32CM-JH_DFP.pdsc:4285-4288 (Dfpu NO_FPU); edc:15 armv6m |
| Core revision | ATDF __CM0P_REV=0x0001 (r0p1); SVD r0p0 (conflict); device header defines no __CM0PLUS_REV, so CMSIS defaults to 0 with a #warning | atdf:52; svd:30; ARM/CMSIS/6.1.0/CMSIS/Core/Include/core_cm0plus.h:136-138 |
| __MPU_PRESENT | 1 (region count not in DFP; MPU.TYPE has no initval) | atdf params; PIC32CM-JH01/include/pic32cm5164jh01100.h:239; svd mpuPresent |
| __NVIC_PRIO_BITS | 2 | atdf:54; header:240; svd nvicPrioBits |
| __VTOR_PRESENT | 1 (startup writes SCB->VTOR) | header:241; gcc/gcc/startup_pic32cm5164jh01100.c:174 |
| __Vendor_SysTickConfig | 0 | header:242 |
| NUM_IRQ | 32 (lines 0-31) | atdf:51; svd:36 |
| Shared vectors | 0 SYSTEM (MCLK, OSCCTRL, OSC32KCTRL, PAC, PM, SUPC); 9 SERCOM0+6; 10 SERCOM1+7; 20 TC0+5; 21 TC1+6; 22 TC2+7 | atdf:1433-1455 |
| Max CPU clock | 48 MHz (both -I/PF and -E/PF variants) | atdf:22-23 speedmax |
| Clock after reset | 4 MHz: OSC48M (OSC48MCTRL 0x82) / 12 (OSC48MDIV 0x0B) -> GCLK0 (GENCTRL0 0x00000106) -> CPUDIV 1 | atdf:4887, 4892; DS60001632H §14.7.3 p.129; gcc/system_pic32cm5164jh01100.c __SYSTEM_CLOCK 4000000 |
| Debug interface | SWD; PA30 SWCLK held in peripheral mode with pull-up after reset | atdf interfaces (SWD); DS §7.4 p.37 |
| Boot | Vector table at 0x00000000; factory UART/I2C bootloader in flash 0-4 KB (production BOOTPROT 0xB) | DS p.1 feature list; DS Table 10-3 p.50 |
| Operating range | 2.7-5.5 V; -40..85 C (I) / -40..125 C (E); TQFP100 pinout CMJH100 | atdf:22-23 |
| Device signature | DSU_DID 0x11060000, compared under mask 0xFFFFF0FF | atdf:1625; edc:15497 |
| Suggested QEMU mapping | cpu-type cortex-m0 + midr 0x410CC601; num-irq 32; num-prio-bits 2; mpu-ns-regions 8 (Arm M0+ assumption); init-svtor 0 | target/arm/tcg/cpu-v7m.c:52; target/arm/cpu.c:2504; hw/arm/armv7m.c:259-262, 558-565; hw/intc/armv7m_nvic.c:2650 |

## Table: Memory map
| region | start | size | access/notes | source |
| FLASH (main NVM) | 0x00000000 | 0x80000 (512 KB; 8192 x 64-B pages) | RWX through NVMCTRL; ECC (72-B physical page); factory bootloader in 0-4 KB | atdf:29; DS Table 10-1 p.50 |
| DATAFLASH | 0x00400000 | 0x2000 (8 KB; 128 x 64-B pages) | RW, executable; own ECC | atdf:39; DS Table 10-1 |
| CAL | 0x00800000 | 0x8 | R (fuses); contents undocumented ('CAL/AUX') | atdf:30; DS Figure 8-1 p.38 |
| LOCKBIT | 0x00802000 | 0x4 | R; NVMCTRL_CEHL [29:24]; security bit | atdf:31; ATDF FUSES LOCKBIT_FUSES |
| USER_PAGE (NVM User Row) | 0x00804000 | 0x100 (4 x 64-B pages) | RW via NVMCTRL WAP/EAR; words 0-1 = config loaded at POR; 0x804008-0x8040FF user ID area | atdf:38; edc:141-142, 246; DS §10.2.1 |
| OTP1..OTP4 | 0x00806000 / 08 / 10 / 18 | 0x8 each | R; OTP1 = NVM geometry feeding NVMCTRL.PARAM; OTP2 = ECC/CEHL availability | atdf:32-35; ATDF FUSES OTPn_FUSES |
| OTP5 = Software Calibration Row | 0x00806020 | 0x8 | R; ADC bias, OSC32K CAL, CAL48M | atdf:36; DS §10.2.2 p.51 |
| TEMP_LOG | 0x00806030 | 0x8 | R; undocumented ('Internal Data') | atdf:37; DS Figure 8-1 |
| Serial number words | 0x0080A00C, 0x0080A040, 0x0080A044, 0x0080A048 | 4 x 4 B | R; NOT described in ATDF/SVD/EDC | DS §10.3 p.51 |
| DRAM (SRAM) | 0x20000000 | 0x10000 (64 KB) | RWX; with ECC, initialize with 32-bit writes before 8/16-bit use | atdf:40; DS §7.3 p.37 |
| HPB0 / APB-A | 0x40000000 | 0x4000 | PAC, PM, MCLK, RSTC, OSCCTRL, OSC32KCTRL, SUPC, GCLK, WDT, RTC, EIC, FREQM, MCRAMC | atdf:41 |
| HPB1 / APB-B | 0x41000000 | 0x10000 | PORT, DSU (+0x1000 CoreSight ROM table, DSU_EXT mirror at +0x100), NVMCTRL, DMAC, MTB | atdf:42 |
| HPB2 / APB-C | 0x42000000 | 0x8000 | EVSYS, SERCOM0-5, CAN0/1, TCC0-2, TC0-4, ADC0/1, AC, DAC, PTC, CCL, ICM, PDEC, SMBIST | atdf:43 |
| HPB3 / APB-D | 0x43000000 | 0x1800 | SERCOM6/7, TC5-7 | atdf:44 |
| DIVAS (AHB) | 0x48000000 | 0x20 | DIVAS | atdf:45 |
| IOBUS (single-cycle I/O port) | 0x60000000 | PORT 3 x 0x80; DIVAS at 0x60000200 | alias register-groups, not memory segments; no wait states (PORT IN needs CTRL.SAMPLING; read DIVAS only when not BUSY) | atdf:656, 253; DS p.454 |
| PPB | 0xE0000000 | 0x100000 | SCS: SystemControl 0xE000E000, SysTick 0xE000E010, NVIC 0xE000E100, MPU 0xE000ED90; EDC emulation sector 0xE0001000-0xE000F000 | atdf:46; edc:25234 |

## Table: Peripheral instances
| instance | module | IP id/version | base address | IRQ number(s) | notes |
| PAC | PAC | U2120/1.1.0 | 0x40000000 | 0 (shared SYSTEM) | PAC id 0 |
| PM | PM | U2240/2.1.1 | 0x40000400 | 0 (shared SYSTEM) | PAC id 1 |
| MCLK | MCLK | U2234/2.0.0 | 0x40000800 | 0 (shared SYSTEM) | PAC id 2; AHB clock-group |
| RSTC | RSTC | U2239/2.0.2 | 0x40000C00 | - | PAC id 3 |
| OSCCTRL | OSCCTRL | U2119/2.2.0 | 0x40001000 | 0 (shared SYSTEM) | PAC id 4; GCLK FDPLL=0, FDPLL32K=1 |
| OSC32KCTRL | OSC32KCTRL | U2246/2.2.0 | 0x40001400 | 0 (shared SYSTEM) | PAC id 5 |
| SUPC | SUPC | U2117/2.1.1 | 0x40001800 | 0 (shared SYSTEM) | PAC id 6 |
| GCLK | GCLK | U2122/1.1.2 | 0x40001C00 | - | PAC id 7; 9 generators, 43 channels |
| WDT | WDT | U2251/1.0.1 | 0x40002000 | 1 | PAC id 8 |
| RTC | RTC | 03608/3a1 | 0x40002400 | 2 | PAC id 9; DMA TIMESTAMP=62 |
| EIC | EIC | U2254/3.1.0 | 0x40002800 | 3 | PAC id 10; GCLK 2 |
| FREQM | FREQM | U2257/1.0.1 | 0x40002C00 | 4 | PAC id 11; GCLK MSR=3, REF=4 |
| MCRAMC | MCRAMC | 03727/3a0 | 0x40003000 | 5 | PAC id 12 |
| PORT | PORT | U2210/3.1.0 | 0x41000000 | - | PAC id 32; IOBUS alias 0x60000000 |
| DSU | DSU | U2209/3.0.0 | 0x41002000 | - | PAC id 33; DSU_EXT mirror 0x41002100 |
| NVMCTRL | NVMCTRL | 03929/1a0 | 0x41004000 | 6 | PAC id 34 |
| DMAC | DMAC | U2223/2.4.0 | 0x41006000 | 7 | PAC id 35 |
| MTB | MTB | U2002/1.0.0 | 0x41008000 | - | PAC id 36 |
| EVSYS | EVSYS | U2256/1.0.2 | 0x42000000 | 8 | PAC id 64; GCLK ch0-11 = 5-16 |
| SERCOM0 | SERCOM | U2201/3.1.1 | 0x42000400 | 9 (shared SERCOM0_6) | PAC 65; GCLK CORE=18, SLOW=17; DMA RX=2, TX=3 |
| SERCOM1 | SERCOM | U2201/3.1.1 | 0x42000800 | 10 (shared SERCOM1_7) | PAC 66; GCLK CORE=19, SLOW=17; DMA RX=4, TX=5 |
| SERCOM2 | SERCOM | U2201/3.1.1 | 0x42000C00 | 11 | PAC 67; GCLK CORE=20, SLOW=17; DMA RX=6, TX=7 |
| SERCOM3 | SERCOM | U2201/3.1.1 | 0x42001000 | 12 | PAC 68; GCLK CORE=21, SLOW=17; DMA RX=8, TX=9 |
| SERCOM4 | SERCOM | U2201/3.1.1 | 0x42001400 | 13 | PAC 69; GCLK CORE=22, SLOW=17; DMA RX=10, TX=11 |
| SERCOM5 | SERCOM | U2201/3.1.1 | 0x42001800 | 14 | PAC 70; GCLK CORE=23, SLOW=17; DMA RX=12, TX=13 |
| CAN0 | CAN | U2003/2.1.0 | 0x42001C00 | 15 | PAC 71; GCLK 26; DMA DEBUG=14 |
| CAN1 | CAN | U2003/2.1.0 | 0x42002000 | 16 | PAC 72; GCLK 27; DMA DEBUG=15 |
| TCC0 | TCC | U2213/4a0 | 0x42002400 | 17 | PAC 73; GCLK 28; DMA OVF=16, MC0-3=17-20 |
| TCC1 | TCC | U2213/4a0 | 0x42002800 | 18 | PAC 74; GCLK 28; DMA OVF=21, MC0-1=22-23 |
| TCC2 | TCC | U2213/4a0 | 0x42002C00 | 19 | PAC 75; GCLK 29; DMA OVF=24, MC0-1=25-26 |
| TC0 | TC | U2249/3.0.0 | 0x42003000 | 20 (shared TC0_5) | PAC 76; GCLK 30; DMA OVF=27, MC=28-29 |
| TC1 | TC | U2249/3.0.0 | 0x42003400 | 21 (shared TC1_6) | PAC 77; GCLK 30; DMA OVF=30, MC=31-32 |
| TC2 | TC | U2249/3.0.0 | 0x42003800 | 22 (shared TC2_7) | PAC 78; GCLK 31; DMA OVF=33, MC=34-35 |
| TC3 | TC | U2249/3.0.0 | 0x42003C00 | 23 | PAC 79; GCLK 31; DMA OVF=36, MC=37-38 |
| TC4 | TC | U2249/3.0.0 | 0x42004000 | 24 | PAC 80; GCLK 32; DMA OVF=39, MC=40-41 |
| ADC0 | ADC | U2247/2.4.0 | 0x42004400 | 25 | PAC 81; GCLK 36; DMA RESRDY=42 |
| ADC1 | ADC | U2247/2.4.0 | 0x42004800 | 26 | PAC 82; GCLK 37; DMA RESRDY=43 |
| AC | AC | U2245/1.1.1 | 0x42004C00 | 27 | PAC 83; GCLK 42 |
| DAC | DAC | U2214/2.1.0 | 0x42005000 | 28 | PAC 84; GCLK 38; DMA EMPTY=45 |
| PTC | PTC | 03795/7a0 (module section 7b0) | 0x42005400 | 30 | PAC 85; GCLK 39; DMA EOC=46, WCOMP=47, SEQ=48; no registers in ATDF |
| CCL | CCL | U2225/2.0.0 | 0x42005800 | - | PAC 86; GCLK 40 |
| ICM | ICM | U2010/1.2.0 | 0x42006400 | 31 | PAC 89 |
| PDEC | PDEC | U2263/1.0.0 | 0x42006800 | 29 | PAC 90; GCLK 41 |
| SMBIST | SMBIST | 03788/1a0 | 0x42006C00 | - | PAC 91 |
| SERCOM6 | SERCOM | U2201/3.1.1 | 0x43000000 | 9 (shared SERCOM0_6) | PAC 96; GCLK CORE=24, SLOW=17; DMA RX=49, TX=50 |
| SERCOM7 | SERCOM | U2201/3.1.1 | 0x43000400 | 10 (shared SERCOM1_7) | PAC 97; GCLK CORE=25, SLOW=17; DMA RX=51, TX=52 |
| TC5 | TC | U2249/3.0.0 | 0x43000800 | 20 (shared TC0_5) | PAC 98; GCLK 33; DMA OVF=53, MC=54-55 |
| TC6 | TC | U2249/3.0.0 | 0x43000C00 | 21 (shared TC1_6) | PAC 99; GCLK 34; DMA OVF=56, MC=57-58 |
| TC7 | TC | U2249/3.0.0 | 0x43001000 | 22 (shared TC2_7) | PAC 100; GCLK 35; DMA OVF=59, MC=60-61 |
| DIVAS | DIVAS | U2258/1.0.0 | 0x48000000 | - | AHB; IOBUS alias 0x60000200 |
| SystemControl | SystemControl (SCB) | -/1.0.0 | 0xE000E000 | - | Arm core |
| SysTick | SysTick | -/1.0.0 | 0xE000E010 | exception -1 | Arm core |
| NVIC | NVIC | -/1.0.0 | 0xE000E100 | - | Arm core |
| MPU | MPU | -/1.0.0 | 0xE000ED90 | - | Arm core |
| FUSES | FUSES | 03929/1a0 | 0x00802000 (fuses space) | - | LOCKBIT 0x00802000, USER 0x00804000, OTP1-5 0x00806000-0x00806020 |

## Table: SERCOM USART_INT registers
| register | offset | size | access | reset value | key bitfields with masks |
| CTRLA | 0x00 | 4 | RW, PAC; ATDF WSYNC; enable-protected except SWRST/ENABLE; SWRST/ENABLE synced via SYNCBUSY.SWRST/ENABLE; SWRST self-clearing | 0x00000000 | SWRST 0x1, ENABLE 0x2, MODE 0x1C (USART_INT=1), RUNSTDBY 0x80, IBON 0x100, SAMPR 0xE000, TXPO 0x30000, RXPO 0x300000, SAMPA 0xC00000, FORM 0xF000000, CMODE 0x10000000, CPOL 0x20000000, DORD 0x40000000 (impl 0x7FF3E19F) |
| CTRLB | 0x04 | 4 | RW, PAC; ATDF WSYNC; enable-protected except RXEN/TXEN; RXEN/TXEN/LINCMD synced via SYNCBUSY.CTRLB while enabled; write while busy = APB error; LINCMD strobe reads 0 | 0x00000000 | CHSIZE 0x7, SBMODE 0x40, COLDEN 0x100, SFDE 0x200, ENC 0x400, PMODE 0x2000, TXEN 0x10000, RXEN 0x20000, LINCMD 0x3000000 (impl 0x03032747) |
| CTRLC | 0x08 | 4 | RW, PAC; enable-protected | 0x00000000 | GTIME 0x7, BRKLEN 0x300, HDRDLY 0xC00 |
| BAUD | 0x0C | 2 | RW, PAC; enable-protected; layout depends on CTRLA.FORM/SAMPR | 0x0000 | arithmetic: BAUD 0xFFFF; fractional (FRAC/FRACFP): BAUD 0x1FFF + FP 0xE000 |
| RXPL | 0x0E | 1 | RW, PAC; enable-protected | 0x00 | RXPL 0xFF |
| INTENCLR | 0x14 | 1 | write-1 clears shared INTEN, reads INTEN; PAC (ATDF atomic-op clear:INTENCLR) | 0x00 | DRE 0x1, TXC 0x2, RXC 0x4, RXS 0x8, CTSIC 0x10, RXBRK 0x20, ERROR 0x80 |
| INTENSET | 0x16 | 1 | write-1 sets shared INTEN, reads INTEN; PAC (set:INTENSET) | 0x00 | same masks as INTENCLR |
| INTFLAG | 0x18 | 1 | DRE, RXC read-only (DRE cleared by DATA write; RXC by DATA read or RX disable); TXC, RXS, CTSIC, RXBRK, ERROR W1C (TXC also cleared by DATA write); ATDF wrongly W1C for all | 0x00 | same masks; IRQ = INTFLAG & INTEN on NVIC 11-14 (SERCOM2-5) or shared 9/10 |
| STATUS | 0x1A | 2 | PERR, FERR, BUFOVF, ISF, COLL, TXE W1C (also cleared by RX disable); CTS read-only pin level; ATDF says plain RW | 0x0000 | PERR 0x1, FERR 0x2, BUFOVF 0x4, CTS 0x8, ISF 0x10, COLL 0x20, TXE 0x40 |
| SYNCBUSY | 0x1C | 4 | R, dynamic | 0x00000000 | SWRST 0x1, ENABLE 0x2, CTRLB 0x4 |
| DATA | 0x28 | 2 | RW with side effects: read pops RX (clears RXC; STATUS errors are per character); write pushes TX (clears DRE/TXC) | 0x0000 | DATA 0x1FF |
| DBGCTRL | 0x30 | 1 | RW, PAC | 0x00 | DBGSTOP 0x1 |

## Table: PORT group registers
| register | offset | size | access | reset value | notes |
| DIR | 0x00 | 4 | RW (PAC) | 0x00000000 | Group stride 0x80 (GROUP count=3): PA 0x41000000, PB 0x41000080, PC 0x41000100; IOBUS alias 0x60000000 / +0x80 / +0x100 |
| DIRCLR | 0x04 | 4 | write-1 clears DIR; reads DIR | 0x00000000 | ATDF atomic-op clear:DIR |
| DIRSET | 0x08 | 4 | write-1 sets DIR; reads DIR | 0x00000000 | atomic-op set:DIR |
| DIRTGL | 0x0C | 4 | write-1 toggles DIR; reads DIR | 0x00000000 | atomic-op toggle:DIR |
| OUT | 0x10 | 4 | RW (PAC) | 0x00000000 | output latch |
| OUTCLR | 0x14 | 4 | write-1 clears OUT; reads OUT | 0x00000000 | atomic-op clear:OUT |
| OUTSET | 0x18 | 4 | write-1 sets OUT; reads OUT | 0x00000000 | atomic-op set:OUT |
| OUTTGL | 0x1C | 4 | write-1 toggles OUT; reads OUT | 0x00000000 | atomic-op toggle:OUT |
| IN | 0x20 | 4 | R | 0x00000000 | pad state when PINCFG.INEN=1 (driven by the external pin model); IOBUS reads need CTRL.SAMPLING set for the pin (DS p.454) |
| CTRL | 0x24 | 4 | RW (PAC) | 0x00000000 | SAMPLING 0xFFFFFFFF (continuous sampling per pin) |
| WRCONFIG | 0x28 | 4 | write-only (reads 0; PAC) | 0x00000000 | bulk update of up to 16 pins: PINMASK 0xFFFF, PMUXEN 0x10000, INEN 0x20000, PULLEN 0x40000, DRVSTR 0x400000, PMUX 0xF000000, WRPMUX 0x10000000, WRPINCFG 0x40000000, HWSEL 0x80000000; needs a post_write hook |
| EVCTRL | 0x2C | 4 | RW (PAC) | 0x00000000 | per byte n=0..3: PIDn 0x1F, EVACTn 0x60, PORTEIn 0x80; EVSYS users PORT_EV_0..3 = 1..4 |
| PMUX0..15 | 0x30-0x3F | 1 each | RW (PAC) | 0x00; group 0 PMUX15 = 0x06 per DS §27.7.13 (ATDF cannot express) | PMUXE 0xF (even pin), PMUXO 0xF0 (odd pin); function letters per pad from ATDF signals (e.g. SERCOM0 PAD0 = PA04 D / PA08 C) |
| PINCFG0..31 | 0x40-0x5F | 1 each | RW (PAC) | 0x00 (DS §7.4: PA30 out of reset in SWCLK mode with pull-up) | PMUXEN 0x1, INEN 0x2, PULLEN 0x4, DRVSTR 0x40 |
| (instance params) | - | - | - | - | GROUPS=3, BITS=93 (MSB 92), EV_NUM=4, DRVSTR=1, ODRAIN=0, SLEWLIM=0; PAC id 32; no NVIC line |

## Table: Clock bring-up status bits
| peripheral | register | offset | bit(s)/mask | meaning | what model must return |
| OSCCTRL | OSC48MSYNCBUSY | 0x18 | OSC48MDIV 0x4 | OSC48MDIV write synchronizing | 0 (or briefly 1 after an OSC48MDIV write); Harmony spins while set |
| OSCCTRL | STATUS | 0x0C | OSC48MRDY 0x10 | OSC48M stable | 1 whenever OSC48MCTRL.ENABLE=1 (on at reset, 0x82); the reset value 0 is transient; Harmony spins until set |
| OSCCTRL | STATUS | 0x0C | XOSCRDY 0x1, XOSCFAIL 0x2, XOSCCKSW 0x4 | crystal ready / failure / clock switched | XOSCRDY=1 after XOSCCTRL.ENABLE if the board model has a crystal, else stays 0 (firmware hangs, as on HW); FAIL/CKSW only with CFD modeling |
| OSCCTRL | DPLLSYNCBUSY | 0x2C | ENABLE 0x2, DPLLRATIO 0x4, DPLLPRESC 0x8 | DPLL register sync | 0 after the write; Harmony spins on each |
| OSCCTRL | DPLLSTATUS | 0x30 | LOCK 0x1, CLKRDY 0x2 | DPLL locked / output ready | both 1 once DPLLCTRLA.ENABLE=1 and the DPLLCTRLB.REFCLK source runs; Harmony waits for LOCK|CLKRDY |
| OSCCTRL | INTFLAG | 0x08 | XOSCRDY 0x1, XOSCFAIL 0x2, OSC48MRDY 0x10, DPLLLCKR 0x100, DPLLLCKF 0x200, DPLLLTO 0x400, DPLLLDRTO 0x800 | event flags | W1C; set on events; drive shared IRQ 0 when enabled |
| OSC32KCTRL | STATUS | 0x0C | XOSC32KRDY 0x1, OSC32KRDY 0x2, CLKFAIL 0x4, CLKSW 0x8 | 32 kHz oscillator ready / failure | XOSC32KRDY or OSC32KRDY = 1 after the matching ENABLE (Harmony spins when ONDEMAND=0) |
| GCLK | SYNCBUSY | 0x04 | SWRST 0x1, GENCTRLn = 1<<(n+2) (0x4..0x400) | generator write synchronizing | 0 after the write (self-clearing); Harmony spins per generator |
| GCLK | GENCTRL0 | 0x20 | SRC 0x1F, GENEN 0x100, DIV 0xFFFF0000 | generator 0 = main clock | reset 0x00000106 (OSC48M, enabled) per DS, not the ATDF initval 0 |
| GCLK | PCHCTRLm | 0x80 + 4*m (m=0..42) | CHEN 0x40, GEN 0xF, WRTLOCK 0x80 | peripheral channel enable | read back CHEN as written (Harmony spins until 1); m = ATDF GCLK_ID |
| MCLK | INTFLAG | 0x03 | CKRDY 0x1 | main clock ready after CPUDIV/mask change | 1 (reset 0x01, W1C); set again right after a CPUDIV write; Harmony spins until 1 |
| NVMCTRL | INTFLAG | 0x14 | READY 0x1, ERROR 0x2 | NVM ready for a command | READY=1 when idle; clear on a CTRLA command (CMDEX 0xA5), set on completion; Harmony IsBusy = READY==0 |
| NVMCTRL | PARAM | 0x08 | NVMP 0xFFFF, PSZ 0x70000, DFP 0xFFF00000 | flash geometry (read-only) | 0x08032000 (8192 pages, PSZ=3 = 64 B, 128 data-flash pages); ATDF says RW / 0 |
| NVMCTRL | CTRLB | 0x04 | RWS 0x1E, MANW 0x80, CACHEDIS 0xC0000 | wait states / manual write | store as written (reset 0x00000080) |
| SUPC | STATUS / INTFLAG | 0x0C / 0x08 | BODVDDRDY 0x1, BODVDDDET 0x2, BVDDSRDY 0x4 | BOD ready, BOD detect, BODVDD sync ready | BVDDSRDY=1, BODVDDRDY=1 (BOD enabled by production user row), BODVDDDET=0 unless a brown-out is injected; Harmony polls BVDDSRDY then BODVDDRDY |
| SUPC | BODVDD | 0x10 | ENABLE 0x2, HYST 0x4, ACTION 0x18, LEVEL 0x3F0000 | BOD configuration | reset loaded from user row (production: ENABLE=1, ACTION=1 reset, LEVEL=8), not ATDF 0 |
| WDT | SYNCBUSY | 0x08 | ENABLE 0x2, WEN 0x4, ALWAYSON 0x8, CLEAR 0x10 | WDT register sync | 0 after the write; Harmony spins while SYNCBUSY != 0 |
| WDT | CTRLA / CONFIG / EWCTRL | 0x00 / 0x01 / 0x02 | ENABLE 0x2, WEN 0x4, ALWAYSON 0x80 / PER 0xF, WINDOW 0xF0 / EWOFFSET 0xF | loaded from user row at POR | production: 0x00 / 0xBB / 0x0B; an erased row gives ENABLE + ALWAYSON |
| RSTC | RCAUSE | 0x00 | POR 0x1, BODCORE 0x2, BODVDD 0x4, EXT 0x10, WDT 0x20, SYST 0x40 | last reset cause | 0x01 on QEMU power-on, 0x40 after SYSRESETREQ, 0x20 after a WDT reset (no ATDF initval) |
| PAC | STATUSB / STATUSC | 0x38 / 0x3C | DSU_ 0x2 / SMBIST_ 0x8000000 + unnamed bit 24 | write-protect state | reset 0x00000002 / 0x09000000; a write to a protected peripheral = access error + PAC.INTFLAGn |
| DSU | STATUSB | 0x02 | PROT 0x1, DBGPRES 0x2, CEHL 0x20 | protected / debugger present | PROT from security bit, CEHL from LOCKBIT row, DBGPRES 0 unless a probe is modeled |

## Table: Device identification
| item | register/address | value or format | source |
| Device ID (DSU.DID) | 0x41002018 (DSU+0x18; mirror 0x41002118) | 0x11060X00: PROCESSOR[31:28]=1 CM0+, FAMILY[27:23]=2 5V Industrial, SERIES[21:16]=6 JH00/JH01, DIE[15:12]=0, REVISION[11:8]=1 (B0) or 2 (C0), DEVSEL[7:0]=0x00; so 0x11060100 (B0) / 0x11060200 (C0) | atdf:3406 initval 0x11060100; atdf:1625; atdf:3467-3488 value-groups; edc:15497 mask 0xFFFFF0FF; errata DS80001000H Table 2 |
| DEVSEL map (JH01) | DID[7:0] (+DIE) | 5164JH01100 0x00, 064 0x01, 048 0x02, 032 0x03; 2532JH01100 0x04, 064 0x05, 048 0x06, 032 0x07; 1216JH01048 0x0A and 032 0x0B (DIE=1) | PIC32CM-JH01/atdf/*.atdf property DSU_DID; errata Table 2 |
| Serial number | 0x0080A00C, 0x0080A040, 0x0080A044, 0x0080A048 | 4 x 32-bit words; unique only as all 128 bits; supply per twin instance (absent from ATDF/SVD/EDC) | DS60001632H §10.3 p.51 |
| NVM Software Calibration Row (OTP5) | 0x00806020 (64 bit) | [2:0] ADC0 BIASREFBUF, [5:3] ADC0 BIASCOMP, [8:6] ADC1 BIASREFBUF, [11:9] ADC1 BIASCOMP, [18:12] OSC32K CAL, [40:19] CAL48M, [63:41] reserved; Harmony sets OSC32K.CALIB=(word0>>12)&0x7F and ADCx.CALIB from bits 11:0, and skips CAL48M on JH | DS Table 10-4 p.51; atdf:4633 (bits 11:0 only); include/component/fuses.h:87-100; Harmony plib_clock.c.ftl:116-124, 188; plib_adc.c.ftl:224-229 |
| NVM User Row | 0x00804000-0x00804007 (rest of the 256-B page is user data) | production 0xB15088FB, 0xFFFFF8BB (BOOTPROT 0xB = 4 KB, BODVDD level 8 enabled with reset action, BODCORE cal 0x0A8, WDT off with PER/WINDOW/EWOFFSET 0xB, LOCK 0xFFFF); DFP default 0xFFFFFFFF | DS Table 10-3 p.50-51; atdf:4641; edc:143; xc32 configuration.data:2, 62 |
| OTP1 (NVM geometry) | 0x00806000-0x00806007 | WORD0: NVM_LOCK[7:0], PSZ[9:8], NVMP[29:16]; WORD1: DATAFLASHP[7:0]; drives NVMCTRL.PARAM = 0x08032000 | ATDF FUSES OTP1_WORD_0/1; DS §26.6.3 p.425; NVMCTRL params PAGES/PAGE_SIZE/DATAFLASH_PAGES |
| LOCKBIT row | 0x00802000 | LOCKBIT_WORD_0 NVMCTRL_CEHL[29:24]; security bit set by NVMCTRL SSB (0x45) writing 0x00 to the first lockbit byte; feeds DSU.STATUSB.PROT | ATDF FUSES LOCKBIT_FUSES; ATDF value-group NVMCTRL_CTRLA__CMD |
| CPUID | 0xE000ED00 | Cortex-M0+: 0x410CC601 if r0p1 (ATDF) or 0x410CC600 if r0p0 (SVD); QEMU cortex-m0 default 0x410CC200 | atdf:52; svd:30; target/arm/tcg/cpu-v7m.c:52; target/arm/cpu.c:2504 |
| CoreSight ROM table | 0x41003000-0x41003FFC (DSU+0x1000) | ENTRY0 0x9F0FC002, ENTRY1 0, END 0; PID0-4 = 0xD0, 0xFC, 0x09, 0x00, 0x00; CID0-3 = 0x0D, 0x10, 0x05, 0xB1 | ATDF DSU module initvals |
| CAL / TEMP_LOG rows | 0x00800000 (8 B) / 0x00806030 (8 B) | named in ATDF but contents undocumented; capture from a real part or leave erased | atdf:30, atdf:37; DS Figure 8-1 p.38 |

## Table: Behavior overlay vocabulary (proposed, per IP id/version)
| overlay key | meaning | generator output / QEMU mapping | authoritative source | ATDF hint (if any) |
| w1c: [fields] | write 1 clears, 0 has no effect | MchpRegInfo.w1c; RegisterAccessInfo.w1c | DS bit text 'cleared by writing 1' | atomic-op clear:<same register> (register-wide, over-inclusive) |
| ro: [fields] | read-only bits inside an RW register | removed from wmask; RegisterAccessInfo.ro | DS bit Access row (R) | register-level rw only |
| alias: {storage, op} | SET/CLR/TGL views of shared storage | MCHP_F_ALIAS_*; one backing word | DS register descriptions | atomic-op set:/clear:/toggle:<target> (target can be the register itself, e.g. INTENCLR) |
| enable_protected: all | {all_except} | writable only while ENABLE=0; violation is an access error | enprot mask; MEMTX_ERROR + PAC.INTFLAGn | DS Property 'Enable-Protected (Bits)' + per-bit notes | none |
| sync: {field: SYNCBUSY bit} | write raises a SYNCBUSY bit until done (CTRLB only while enabled) | syncbusy mask; bit set then cleared (zero-latency or timed) | DS notes 'SYNCBUSY.X must be checked' | access=WSYNC/RWSYNC (register-level, inconsistent across packs) |
| write_while_syncbusy: bus_error | write during ongoing sync is an APB error | MCHP_F_BUSY_BUSERR -> MEMTX_ERROR | DS §30.7.10; §12.5.2.4 | none |
| strobe / self_clearing | bits that always read 0 / clear when the action is done | rdz mask | DS 'strobe bits ... read back as zero'; SWRST | none |
| pac_protected | write blocked while PAC protection is set for the instance | MCHP_F_PAC + PAC check keyed by ATDF INSTANCE_ID | DS Property 'PAC Write-Protection' | INSTANCE_ID param; PAC.STATUSx bitfield names |
| hooks: [pre_read, post_write, ...] | side effects (FIFO pop/push, enable, SWRST, WRCONFIG) | callbacks into hand-written per-IP C, reused across devices | DS functional chapters | none |
| dynamic | value computed by the model (SYNCBUSY, ready flags) | MCHP_F_DYNAMIC | DS + firmware polling loops | none |
| reset_overrides: {reg[i]: value} | per-element reset exceptions | per-instance reset table | DS 'Reset:' lines (GENCTRL0 0x106, PMUX15 group0 0x06) | single initval per array |
| reset_from: {fuse field: register field} | reset loaded from user row / OTP / calibration | reset-time loader reading the NVM image | DS Table 10-3 'Related Peripheral Register'; 'Reset: determined from NVM User Row' | USER_FUSES value-group names (WDT_CONFIG__PER, SUPC_BODVDD__ACTION) |
| irq_source: {enable: INTEN} | IRQ = INTFLAG & INTEN, OR-ed into a shared vector where needed | qemu_irq; TYPE_OR_IRQ for vectors 0, 9, 10, 20, 21, 22 | DS | <interrupts> module-instance lists |
| id + versions (+ structural probes) | applicability guard | generator refuses an unlisted version or unknown register/field | ATDF <module id version> | module id/version; Harmony-style probes (e.g. SYNCBUSY register present) |

## Code: ATDF: SERCOM mode selectors (six layouts at one base) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5830-5836)
```xml
      <register-group name="SERCOM" caption="Serial Communication Interface">
        <mode name="I2CM" qualifier="SERCOM.I2CM_CTRLA.MODE" mask="6" value="5" caption="I2C Master Mode"/>
        <mode name="I2CS" qualifier="SERCOM.I2CS_CTRLA.MODE" mask="6" value="4" caption="I2C Slave Mode"/>
        <mode name="SPIS" qualifier="SERCOM.SPIS_CTRLA.MODE" mask="6" value="2" caption="SPI Slave Mode"/>
        <mode name="SPIM" qualifier="SERCOM.SPIM_CTRLA.MODE" mask="6" value="3" caption="SPI Master Mode"/>
        <mode name="USART_EXT" qualifier="SERCOM.USART_CTRLA.MODE" mask="6" value="0" caption="USART EXTERNAL CLOCK Mode"/>
        <mode name="USART_INT" qualifier="SERCOM.USART_CTRLA.MODE" mask="6" value="1" caption="USART INTERNAL CLOCK Mode"/>
```

## Code: ATDF: USART_INT INTFLAG with register-wide W1C hint (wrong for DRE/RXC) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6137-6145)
```xml
        <register modes="USART_INT" name="INTFLAG" offset="0x18" rw="RW" size="1" atomic-op="clear:INTFLAG" initval="0x00" caption="USART_INT Interrupt Flag Status and Clear">
          <bitfield name="DRE" caption="Data Register Empty Interrupt" mask="0x1"/>
          <bitfield name="TXC" caption="Transmit Complete Interrupt" mask="0x2"/>
          <bitfield name="RXC" caption="Receive Complete Interrupt" mask="0x4"/>
          <bitfield name="RXS" caption="Receive Start Interrupt" mask="0x8"/>
          <bitfield name="CTSIC" caption="Clear To Send Input Change Interrupt" mask="0x10"/>
          <bitfield name="RXBRK" caption="Break Received Interrupt" mask="0x20"/>
          <bitfield name="ERROR" caption="Combined Error Interrupt" mask="0x80"/>
        </register>
```

## Code: ATDF: USART_INT STATUS with no W1C marking (datasheet says W1C, CTS read-only) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6183-6191)
```xml
        <register modes="USART_INT" name="STATUS" offset="0x1A" rw="RW" size="2" initval="0x0000" caption="USART_INT Status">
          <bitfield name="PERR" caption="Parity Error" mask="0x1"/>
          <bitfield name="FERR" caption="Frame Error" mask="0x2"/>
          <bitfield name="BUFOVF" caption="Buffer Overflow" mask="0x4"/>
          <bitfield name="CTS" caption="Clear To Send" mask="0x8"/>
          <bitfield name="ISF" caption="Inconsistent Sync Field" mask="0x10"/>
          <bitfield name="COLL" caption="Collision Detected" mask="0x20"/>
          <bitfield name="TXE" caption="Transmitter Empty" mask="0x40"/>
        </register>
```

## Code: Proposed behavior overlay (one per IP id/version), USART_INT section (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/gen/overlays/sercom_u2201.yaml:11-39 (lines 1-10: ip SERCOM, id U2201, versions [3.1.1], modes: USART_INT: registers:))
```yaml
      CTRLA:
        pac_protected: true
        enable_protected: {all_except: [SWRST, ENABLE]}   # "Enable-Protected Bits"
        sync: {SWRST: SWRST, ENABLE: ENABLE}              # field -> SYNCBUSY bit
        self_clearing: [SWRST]
        hooks: [post_write]                               # enable / soft reset
      CTRLB:
        pac_protected: true
        enable_protected: {all_except: [RXEN, TXEN]}
        sync: {RXEN: CTRLB, TXEN: CTRLB, LINCMD: CTRLB}   # only while CTRLA.ENABLE=1
        strobe: [LINCMD]                                  # "strobe bits ... always read back as zero"
        write_while_syncbusy: bus_error                   # "an APB error will be generated"
        hooks: [post_write]
      CTRLC:  {pac_protected: true, enable_protected: all}
      BAUD:   {pac_protected: true, enable_protected: all}
      RXPL:   {pac_protected: true, enable_protected: all}
      INTENCLR: {pac_protected: true, alias: {storage: INTEN, op: clear}}
      INTENSET: {pac_protected: true, alias: {storage: INTEN, op: set}}
      INTFLAG:
        w1c: [TXC, RXS, CTSIC, RXBRK, ERROR]
        ro:  [DRE, RXC]          # DRE cleared by DATA write, RXC by DATA read
        irq_source: {enable: INTEN}
      STATUS:
        w1c: [PERR, FERR, BUFOVF, ISF, COLL, TXE]
        ro:  [CTS]               # reflects CTS pin level
      SYNCBUSY: {dynamic: true}
      DATA:
        hooks: [pre_read, post_write]   # pop RX FIFO / push TX, update DRE/TXC/RXC
      DBGCTRL: {pac_protected: true}
```

## Code: Generator key logic (1/2): interpreting ATDF atomic-op and access hints (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/gen/atdf2qemu.py:84-113)
```python
def atdf_atomic(r, names):
    """Interpret ATDF atomic-op: 'clear:INTFLAG' on INTFLAG = W1C; 'set:DIR' on DIRSET =
    alias onto DIR; 'clear:INTENCLR' on INTENCLR = alias pair (same syntax as W1C!)."""
    if not r["atomic"]:
        return None, None
    op, tgt = r["atomic"].split(":")
    if tgt != r["name"]:
        return "alias", (op, tgt)
    base = r["name"][:-3]
    if r["name"][-3:] in ("SET", "CLR") and {base + "SET", base + "CLR"} <= names:
        return "alias", (op, base)
    return ("w1c", None) if op == "clear" else (None, None)

def merge(regs, ov, warn):
    names = {r["name"] for r in regs}
    for r in regs:
        o = (ov or {}).get(r["name"], {})
        impl = 0
        for m in r["fields"].values():
            impl |= m
        r["impl"] = impl
        r["wmask"] = impl if "W" in (r["rw"] or "") else 0
        # ATDF-derived hints (atomic-op / access) -- used only as defaults
        kind, arg = atdf_atomic(r, names)
        r["w1c"] = impl if kind == "w1c" else 0
        if kind == "alias" and "alias" not in o:
            o = dict(o, alias={"storage": arg[1], "op": arg[0]})
            warn.append(f"{r['name']}: alias {arg[0]}->{arg[1]} inferred from ATDF atomic-op={r['atomic']}")
        if r["access"] in ("WSYNC", "RWSYNC") and "sync" not in o:
            warn.append(f"{r['name']}: ATDF access={r['access']} but no SYNCBUSY mapping in overlay")
```

## Code: Generator key logic (2/2): applying the overlay to masks (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/gen/atdf2qemu.py:114-135)
```python
        if "w1c" in o:
            new = fmask(r, o["w1c"], "w1c")
            if r["w1c"] and r["w1c"] != new:
                warn.append(f"{r['name']}: ATDF atomic-op={r['atomic']} implies w1c=0x{r['w1c']:X}, overlay says 0x{new:X}")
            if not r["w1c"]:
                warn.append(f"{r['name']}: W1C bits 0x{new:X} come only from overlay (ATDF has no atomic-op)")
            r["w1c"] = new
        ro = fmask(r, o.get("ro", []), "ro")
        r["wmask"] &= ~(ro | r["w1c"])
        ep = o.get("enable_protected")
        if ep == "all":
            r["enprot"] = r["wmask"]
        elif isinstance(ep, dict):
            r["enprot"] = r["wmask"] & ~fmask(r, ep["all_except"], "enable_protected")
        else:
            r["enprot"] = 0
        r["sync"] = {f: b for f, b in (o.get("sync") or {}).items()}
        fmask(r, r["sync"].keys(), "sync")
        # read-as-zero: strobes, self-clearing bits (zero-latency model), write-only regs
        r["rdz"] = fmask(r, o.get("strobe", []) + o.get("self_clearing", []), "strobe")
        if r["rw"] == "W":
            r["rdz"] = impl
```

## Code: Generated output (1/3): QEMU registerfields-style constants (actual run output) (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/out/run_jh.txt:96-111)
```c
REG16(SERCOM_USART_INT_STATUS, 0x1A)
    FIELD(SERCOM_USART_INT_STATUS, PERR, 0, 1)
    FIELD(SERCOM_USART_INT_STATUS, FERR, 1, 1)
    FIELD(SERCOM_USART_INT_STATUS, BUFOVF, 2, 1)
    FIELD(SERCOM_USART_INT_STATUS, CTS, 3, 1)
    FIELD(SERCOM_USART_INT_STATUS, ISF, 4, 1)
    FIELD(SERCOM_USART_INT_STATUS, COLL, 5, 1)
    FIELD(SERCOM_USART_INT_STATUS, TXE, 6, 1)
REG32(SERCOM_USART_INT_SYNCBUSY, 0x1C)
    FIELD(SERCOM_USART_INT_SYNCBUSY, SWRST, 0, 1)
    FIELD(SERCOM_USART_INT_SYNCBUSY, ENABLE, 1, 1)
    FIELD(SERCOM_USART_INT_SYNCBUSY, CTRLB, 2, 1)
REG16(SERCOM_USART_INT_DATA, 0x28)
    FIELD(SERCOM_USART_INT_DATA, DATA, 0, 9)
REG8(SERCOM_USART_INT_DBGCTRL, 0x30)
    FIELD(SERCOM_USART_INT_DBGCTRL, DBGSTOP, 0, 1)
```

## Code: Generated output (2/3): register descriptor table, reset routine and ATDF-vs-overlay report (actual run output) (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/out/run_jh.txt:113-140)
```c
static const MchpRegInfo sercom_usart_int_regs[] = {
    /* name       off sz reset       impl        wmask       w1c         rdz         enprot      syncbusy    flags */
    { "CTRLA",    0x00, 4, 0x00000000, 0x7FF3E19F, 0x7FF3E19F, 0x00000000, 0x00000001, 0x7FF3E19C, 0x00000003, MCHP_F_PAC | MCHP_F_HOOK_POST_WRITE },
    { "CTRLB",    0x04, 4, 0x00000000, 0x03032747, 0x03032747, 0x00000000, 0x03000000, 0x03002747, 0x00000004, MCHP_F_PAC | MCHP_F_HOOK_POST_WRITE | MCHP_F_BUSY_BUSERR },
    { "CTRLC",    0x08, 4, 0x00000000, 0x00000F07, 0x00000F07, 0x00000000, 0x00000000, 0x00000F07, 0x00000000, MCHP_F_PAC },
    { "BAUD",     0x0C, 2, 0x00000000, 0x0000FFFF, 0x0000FFFF, 0x00000000, 0x00000000, 0x0000FFFF, 0x00000000, MCHP_F_PAC },
    { "RXPL",     0x0E, 1, 0x00000000, 0x000000FF, 0x000000FF, 0x00000000, 0x00000000, 0x000000FF, 0x00000000, MCHP_F_PAC },
    { "INTENCLR", 0x14, 1, 0x00000000, 0x000000BF, 0x000000BF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, MCHP_F_PAC | MCHP_F_ALIAS_CLEAR },
    { "INTENSET", 0x16, 1, 0x00000000, 0x000000BF, 0x000000BF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, MCHP_F_PAC | MCHP_F_ALIAS_SET },
    { "INTFLAG",  0x18, 1, 0x00000000, 0x000000BF, 0x00000000, 0x000000BA, 0x00000000, 0x00000000, 0x00000000, 0 },
    { "STATUS",   0x1A, 2, 0x00000000, 0x0000007F, 0x00000000, 0x00000077, 0x00000000, 0x00000000, 0x00000000, 0 },
    { "SYNCBUSY", 0x1C, 4, 0x00000000, 0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, MCHP_F_DYNAMIC },
    { "DATA",     0x28, 2, 0x00000000, 0x000001FF, 0x000001FF, 0x00000000, 0x00000000, 0x00000000, 0x00000000, MCHP_F_HOOK_PRE_READ | MCHP_F_HOOK_POST_WRITE },
    { "DBGCTRL",  0x30, 1, 0x00000000, 0x00000001, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, MCHP_F_PAC },
};

static void sercom_usart_int_reset(uint8_t *regs) /* little-endian register file */
{
    for (size_t i = 0; i < ARRAY_SIZE(sercom_usart_int_regs); i++) {
        const MchpRegInfo *ri = &sercom_usart_int_regs[i];
        stn_le_p(regs + ri->offset, ri->size, ri->reset);
    }
}

/* --- generator report (ATDF vs overlay) ---
 * INTFLAG: ATDF atomic-op=clear:INTFLAG implies w1c=0xBF, overlay says 0xBA
 * STATUS: W1C bits 0x77 come only from overlay (ATDF has no atomic-op)
 */
```

## Code: Generated output (3/3): shared write-apply helper emitted with the table (C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-data/out/run_jh.txt:26-36)
```c
/* Store a write of `val` (already lane-merged) to `ri`. Returns false for a
 * "Peripheral Access Error" (DS60001632H 12.5.2.4): changing enable-protected
 * bits while enabled; caller then sets PAC.INTFLAGn and returns MEMTX_ERROR. */
static inline bool mchp_reg_apply_write(const MchpRegInfo *ri, uint32_t *reg,
                                        uint32_t val, bool enabled)
{
    uint32_t old = *reg;
    uint32_t w = ri->wmask & ~(enabled ? ri->enprot : 0);
    *reg = ((old & ~w) | (val & w)) & ~(val & ri->w1c);
    return !(enabled && ((val ^ old) & ri->enprot));
}
```

## Code: QEMU: RegisterAccessInfo already has the overlay's mask vocabulary (include/hw/core/register.h:45-60)
```c
struct RegisterAccessInfo {
    const char *name;
    uint64_t ro;
    uint64_t w1c;
    uint64_t reset;
    uint64_t cor;
    uint64_t rsvd;
    uint64_t unimp;

    uint64_t (*pre_write)(RegisterInfo *reg, uint64_t val);
    void (*post_write)(RegisterInfo *reg, uint64_t val);

    uint64_t (*post_read)(RegisterInfo *reg, uint64_t val);

    hwaddr addr;
};
```

## Code: QEMU: register_write mask semantics (same as mchp_reg_apply_write minus enable-protect) (hw/core/register.c:103-108)
```c
    /* Create the no write mask based on the read only, write to clear and
     * reserved bit masks.
     */
    no_w_mask = ac->ro | ac->w1c | ac->rsvd | ~we;
    new_val = (val & ~no_w_mask) | (old_val & no_w_mask);
    new_val &= ~(val & ac->w1c);
```

## Code: Harmony SERCOM USART error path: relies on STATUS W1C and DATA-read side effect (not in ATDF) (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/sercom_u2201/templates/plib_sercom_usart.c.ftl:128-144)
```c
    USART_ERROR errorStatus = (USART_ERROR) (${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_STATUS & (uint16_t)(SERCOM_${SERCOM_USART_REG_NAME}_STATUS_PERR_Msk | SERCOM_${SERCOM_USART_REG_NAME}_STATUS_FERR_Msk | SERCOM_${SERCOM_USART_REG_NAME}_STATUS_BUFOVF_Msk ));

    if(errorStatus != USART_ERROR_NONE)
    {
<#if USART_INTENSET_ERROR = true>
        /* Clear error flag */
        ${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG = (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_ERROR_Msk;
</#if>
        /* Clear all errors */
        ${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_STATUS = (uint16_t)(SERCOM_${SERCOM_USART_REG_NAME}_STATUS_PERR_Msk | SERCOM_${SERCOM_USART_REG_NAME}_STATUS_FERR_Msk | SERCOM_${SERCOM_USART_REG_NAME}_STATUS_BUFOVF_Msk);

        /* Flush existing error bytes from the RX FIFO */
        while((${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_INTFLAG & (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_RXC_Msk) == (uint8_t)SERCOM_${SERCOM_USART_REG_NAME}_INTFLAG_RXC_Msk)
        {
            u8dummyData = (uint8_t)${SERCOM_INSTANCE_NAME}_REGS->${SERCOM_USART_REG_NAME}.SERCOM_DATA;
        }
    }
```

## Code: Harmony clock init: CAL48M load from the calibration row is skipped on JH (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/clk_sam_c20_c21/templates/plib_clock.c.ftl:116-124)
```c
<#if CONFIG_CLOCK_OSC48M_ENABLE == true>
<#if DeviceFamily != "PIC32CM_JH00_JH01">
    <#if CALIBRATION_ROW == "0">
    uint32_t calibValue = (uint32_t)(((*(uint64_t*)${SW_CALIB_ROW_ADDR}UL) >> 19 ) & 0x3fffffUL);
    <#else>
    uint32_t calibValue = (uint32_t)(((*(uint64_t*)${SW_CALIB_ROW_ADDR}UL) >> 41 ) & 0x3fffffUL);
    </#if>
    OSCCTRL_REGS->OSCCTRL_CAL48M = calibValue;
</#if>
```

## Code: Harmony configurator reads the ATDF directly (precedent for ATDF-driven generation) (https://raw.githubusercontent.com/Microchip-MPLAB-Harmony/csp/master/peripheral/clk_sam_c20_c21/config/clk.py:204-206)
```python
calibRowAddr = ATDF.getNode("/avr-tools-device-file/devices/device/address-spaces/address-space/memory-segment@[name=\"OTP5\"]").getAttribute("start")
swCalibRowAddr = coreComponent.createStringSymbol("SW_CALIB_ROW_ADDR", None)
swCalibRowAddr.setDefaultValue(calibRowAddr)
```

## Open questions
- What does mask='6' on the ATDF SERCOM <mode> elements mean? All six modes carry it, so it cannot discriminate USART_EXT from USART_INT. The ATDF schema (avr_tools_device_file.xsd) is referenced but not shipped in the pack.
- CPU revision conflict: ATDF __CM0P_REV=0x0001 (r0p1) vs SVD <revision>r0p0. Read SCB->CPUID on B0 and C0 silicon to fix the midr override.
- Can Microchip's internal register database (source of the datasheet 'Property:' lines, per-bit notes and the ATDF) export W1C/RO/enable-protect/sync/strobe metadata directly? That would replace hand-mined overlays.
- Contents and consumers of the CAL row (0x00800000) and TEMP_LOG row (0x00806030) are undocumented in DS60001632H. Does any Microchip library read them?
- PA30 reset state is inconsistent in the datasheet: the PINCFG summary says 0x00, but §7.4 says peripheral mode with pull-up and PMUX15 (group 0) = 0x06. What is PINCFG30 on silicon?
- Not verified in this pass: what happens when software touches a peripheral whose MCLK APB/AHB mask bit is cleared (bus error, read-as-zero or stall)?
- PTC has no public register map (0 registers in the ATDF; instance 7a0 vs module 7b0). Is an internal spec available, or must the twin stub PTC and give up QTouch firmware compatibility?
- Which silicon revision should the twin report by default (B0 DID 0x11060100 vs C0 0x11060200), and do any tools check the REVISION nibble beyond the 0xFFFFF0FF mask?
- How much timing fidelity do SYNCBUSY and ready flags need? Zero-latency clearing satisfies Harmony's polling loops, but some code may need to observe SYNCBUSY=1 at least once.
- Tool-support side note: Simulator.emu maps PIC32CM1216JH00032/048/064/100 and PIC32CM1216JH01064/01100 to pic32sim, but DFP 1.3.221 has no ATDF/EDC for them. Are they intentional (another DFP version) or stale entries?
