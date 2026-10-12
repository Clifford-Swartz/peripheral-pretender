# microchip-board-inputs

## Summary
Microchip already has machine-readable data that fully describes the MCU side of any customer board. In the PIC32CM JH ATDFs, each peripheral instance has a <signals> table (pad, function letter, PAD index), and each file has one package <pinout>, the PORT PMUX/PINCFG encodings, and SERCOM pad-role value groups; no <pins> element exists. The EDC .PIC file in the same DFP has an equivalent PinList. Together these let a twin work out at runtime which pads carry which SERCOM signal, and route traffic to parts wired to those pads. MCC Harmony projects save the customer's pin choices per package pin in components/core.yml and in a generated pin_configurations.csv. When customers use MCC driver components (AT24/AT25/SST26/PAC193x...), the config also records part type, address and PLIB binding. MCC never records the netlist: most attached parts, GPIO-level wiring (CS/INT/RESET), pull-ups and shared buses are invisible (the JH01 i2c_eeprom demo hardcodes the EEPROM address 0x54 in main.c). Harmony BSPs only describe LEDs and switches. The richest public board+parts description is Zephyr devicetree, written by Microchip for its PIC32C boards with pinctrl auto-generated from the ATDF, plus overlays/shields where each part is a child node with compatible/reg/gpios (~43 Microchip/Atmel off-chip bindings, 35 MIKROE Click shields). Antmicro's dts2repl already turns these devicetrees into Renode machines (926/1513 Zephyr platforms passed Hello World in Aug 2026), and it has a microchip,pic32cm overlay that only fakes clock-status registers. So shallow third-party PIC32CM simulation already exists, and a Microchip twin must stand out on fidelity and part models. I found no MCU simulator that imports KiCad/Altium netlists. The board-level simulators that exist (Proteus VSM, which Microchip resells as an 8/16-bit training edition; Wokwi; PICSimLab) use their own formats, and among Microchip MCUs they cover only 8/16-bit PIC and AVR. Upstream QEMU has few Microchip off-chip models: EMC1413/1414, a generic AT24C EEPROM without page-wrap or busy behavior, SST25/AT25/AT45/25CSM04 flash entries, and SMSC LAN9118+PHY. Renode adds ENC28J60, PAC1934, KS8851 and AT86RF233. Neither has models for MCP9808, MCP7940N, MCP23xxx, MCP3xxx/MCP4xxx, MCP2515/2518FD, LAN9250, KSZ8863, USB2514, MTCH6102, ATECC608, RN4870 or WINC/WILC. QEMU can already attach I2C/SPI part models to named buses from the command line and change their state through QMP qom-set. GPIO wiring (CS/INT/LED/button) still needs C code, and both QEMU and Renode buses ignore pin-mux, so a board-description loader and a pinmux-aware router would be new work. mikroBUS sockets are on Curiosity Ultra boards and the PIC32CM GC00/SG00 Curiosity Pro. The PIC32CM JH01 Curiosity Pro has three Xplained Pro EXT headers plus an Arduino header instead, Curiosity Nano boards need the AC164162 base, and Xplained Pro boards need ATMBUSADAPTER-XPRO. With 2,000 Click boards, ClickID manifests and Zephyr shields, a virtual Click works well as a packaging layer on top of a part-model library, not as the core library unit. MPLAB SIM (pin/SFR stimulus, register injection, SCL), Data Visualizer (COM port/DGI input; QEMU can bind a UART to a Windows COM port) and Mindi (SIMetrix/SIMPLIS analog) complement a twin rather than compete with it. The only full-system simulator Microchip ships that I found is Renode inside SoftConsole for PolarFire SoC, and I found no 2023–2026 Microchip announcement of an MCU digital twin or simulation-as-a-service. Customer-value data is thin. In AspenCore's 2023 study (n=655), 11% use QEMU today, and 37% / 31% / 24% expect simulation / emulation / virtual prototyping to become more important. Having a virtual prototype available was a weak processor-selection factor (40%) compared with software tools (85%). I found no verified survey that measures hardware-availability bottlenecks for MCU teams.

## Findings

1. (high) [LOAD-BEARING] [(a) ATDF pin-mux data (local, verified)] The PIC32CM JH ATDFs hold all the static data needed to decode pad-to-peripheral-signal routing: a per-instance <signals> table (group/index/function letter/pad), one <pinout> per package (position→pad, tied to an order code through <variant package pinout>), PORT PMUX/PINCFG register definitions with a letter→code value group (A=0x0…K=0xA), and SERCOM CTRLA value groups whose captions give PAD roles (DOPO/DIPO/TXPO/RXPO). None of the 24 JH ATDFs has a <pins> element.
   detail: Pad roles exist only as free-text captions, so a twin would need a small hand-curated role table per SERCOM version (I2C uses PAD0=SDA, PAD1=SCL per the datasheet; the ATDF does not mark which pads are I2C-capable). There is no electrical or board data. Under Apache-2.0 the ATDF data can be reused in an open-source twin.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:900-915 (SERCOM0 signals: PAD0 = PA04/D, PA08/C, PB24/C ...)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:7416-7435 (<pinouts><pinout name="CMJH100"> ... position 17 = PA04, 18 = PA05)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5425-5434 (PMUX count=16 PMUXE/PMUXO; PINCFG count=32 PMUXEN/INEN/PULLEN/DRVSTR)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5445-5457 (value-group PORT_PMUX__PMUXE A..K)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6331-6342 (SPIM DIPO/DOPO captions, e.g. 'PAD[0]=DO, PAD[1]=SCK, PAD[2]=SlaveSS')
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6391-6415 (USART RXPO/TXPO captions)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1-19 (Apache-2.0 header; schema-version 4.4; variant package=TQFP100 pinout=CMJH100)
   - ev: grep over PIC32CM-JH00/atdf and PIC32CM-JH01/atdf: 0 of 24 files contain '<pins'; pinouts CMJH32/48/64/100 plus RTB_T5X/U5B_Y8X/_5LX_V2X

2. (high) [(a) EDC .PIC PinList (local, verified)] The MPLAB X EDC device file in the same DFP has an equivalent pin-ordered PinList (edc:ppsflavor="atmel"). Each pin carries VirtualPins with name, ppsfunction letter and owning peripheral, consistent with the ATDF (PA00 function D = SERCOM1 PAD0). So the device database that MPLAB SIM tooling already uses could also feed a board twin.
   detail: Useful if the twin's device mapping is meant to reuse the .emu/EDC pipeline the user's team already maintains, instead of parsing ATDF separately.
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:25236-25245
   - ev: PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC tag counts: 100 <edc:Pin>, 592 <edc:VirtualPin>
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:926-931 (SERCOM1 PAD index 0 function D pad PA00)

3. (medium) [LOAD-BEARING] [(a) Runtime pinmux-aware routing (design inference, data points verified)] A router driven by firmware register writes is feasible. Steps: (1) watch PINCFG.PMUXEN, PMUX and SERCOM CTRLA (mode plus DOPO/DIPO/TXPO/RXPO); (2) map pad→(instance, PAD n) with the ATDF signals and PAD n→role with the CTRLA captions; (3) attach the board parts wired to those pads to the SERCOM's logical I2C/SPI/UART endpoint, or detach them; (4) drive GPIO-level nets (CS/INT/RESET/LED/button) from PORT OUT/DIR and into EIC. A real-board cross-check agrees on every point. Neither QEMU nor Renode routes by pad today.
   detail: This would let the twin reproduce bring-up bugs that a chip-only twin cannot show: wrong function letter, PMUXEN left clear, DOPO/DIPO not matching the board, missing I2C pull-ups (PINCFG.PULLEN or a board resistor), two drivers on one net. It needs board-side net data (which part pin sits on which pad) that MCC does not hold.
   - ev: zephyr boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.dts:196-205 (sercom4 rxpo=<3> txpo=<1>) and pic32cm_jh01_cpro-pinctrl.dtsi:10-13 (PB10D_SERCOM4_PAD2, PB11D_SERCOM4_PAD3)
   - ev: PIC32CM JH01 Curiosity Pro UG DS70005482B Table 4-33: PB10 SERCOM4/PAD[2] UART TX, PB11 SERCOM4/PAD[3] UART RX (copy at https://store.comet.bg/download-file.php?id=29529; text extracted to C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-board-inputs/dl-jh01-ug/ug.txt)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6410-6415 (TXPO 0x1 = 'PAD[2]=TxD, PAD[3]=XCK')
   - ev: qemu hw/ssi/ssi.c:30-43 (peripheral selected by cs_index on a logical bus); hw/i2c/core.c:22 (I2C 'address' prop)
   - ev: renode platforms/cpus/atsamd21j17d-aft.repl (SAMD21_GPIO ports and SAMD5_UART at fixed addresses; no PORT mux model) https://github.com/renode/renode

4. (high) [LOAD-BEARING] [(a) MCC Harmony project files (verified on GitHub)] MCC with Harmony content saves pin configuration in machine-readable form. Per-component YAML lives at <proj>.X/<cfg>/components/<component>.yml (core.yml holds PIN_<package#>_FUNCTION_TYPE / _PERIPHERAL_FUNCTION / _DIR / _LAT / _PULLEN / _INEN symbols). mcc-config.mc4 is YAML listing modules and generated-file hashes. mcc-manifest-autosave.yml / -generated-success.yml record MCC/core/content versions, the device and the DFP version. A generated src/config/<cfg>/pin_configurations.csv lists Pin Number, Pin ID, Custom Name, Function, Mode, Direction, Latch, Pull Up, Pull Down and Drive Strength.
   detail: The CSV is the simplest import target (pad plus SERCOMn_PADm per pin). Pins are keyed by package pin number, which resolves to a pad through the ATDF <pinout>. The YAML is verbose but complete (it includes the peripheral function letter).
   - ev: https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01 apps/sercom/i2c/master/i2c_eeprom/firmware/pic32cm_jh01_curiosity_pro.X/i2c_eeprom_pic32cm_jh01_curiosity_pro_pic32cm_jh01_curiosity_pro/components/core.yml:457-486 (PIN_52_FUNCTION_TYPE SERCOM1_PAD0; PIN_52_PERIPHERAL_FUNCTION C)
   - ev: same app src/config/pic32cm_jh01_curiosity_pro/pin_configurations.csv:1,20,45,46
   - ev: same app mcc-manifest-autosave.yml: mcc_version 5.6.2, mcc_core_version 5.8.2, device PIC32CM5164JH01100, packs PIC32CM-JH_DFP 1.5.236, csp v3.23.0
   - ev: repo tree: 38 projects each with mcc-config.mc4, mcc-manifest-autosave.yml, components/core.yml, dfp.yml, cmsis.yml

5. (medium) [(a) MCC Classic and Melody .mc3 formats (sample-based)] Classic .mc3 files are XML with register-level 'Pin Module' OptionKey/CustomKey entries (registerAlias LATA, settingAlias LATA0, IOC/ANSEL keys). Melody .mc3 files are an XML envelope (deviceLibraryClass com.microchip.mcc.melody.Library) holding HTML-escaped JSON module state, where pin-manager payloads map signals to pins (e.g. UART1 U1RX→RD10, QEI1 QEIA1→RD5) and store per-pin row states.
   detail: Microchip does not document either format publicly; these conclusions come from public sample projects. The Microchip VS Code video says newer Harmony projects migrate .mc3 settings into yml files.
   - ev: gh code search hits: rasteri/SC1000 firmware/MyConfig.mc3; tinfever/FW-Dyson-BMS firmware/MCC_config.mc3; PaulKlinger/tinypc code/MyConfig.mc3 (Classic OptionKey/CustomKey lines)
   - ev: https://github.com/robertobucher/pysimCoder CodeGen/dsPIC/pysimC.mc3 (device dsPIC33AK512MPS512, coreVersion 5.8.4, packages @mchp-mcc/dspic33a-pin-manager 1.0.5 and @mchp-mcc/pin-content-processor 3.11.0; JSON decoded locally)
   - ev: https://developerhelp.microchip.com/xwiki/bin/view/software-tools/mcc/config/ (.mc3 = saved MCC configuration; search snippet)

6. (high) [LOAD-BEARING] [(a) MCC driver components as a partial parts list] When customers use Harmony driver components, the MCC config records part type, bus binding and parameters. For example, drv_at24 has DRV_AT24_PLIB, I2C_EEPROM_ADDDRESS (default 0x57), EEPROM_PAGE_SIZE, EEPROM_FLASH_SIZE and EEPROM_ADDR_LEN. Harmony core ships MCC drivers for AT24, AT25, AT25DF, SST26, MX25L, W25, SST38/39, MCP16502, PAC193x and SDSPI. Apps that call the PLIB directly hide the part: the JH01 i2c_eeprom demo's MCC modules are only cmsis/core/dfp/evsys/nvmctrl/sercom1, and the EEPROM address lives in main.c. MCC also exposes MCP tools that make its capabilities machine-readable.
   detail: Auto-wiring coverage therefore depends on how customers write firmware. MCC can seed a board description (pads, SERCOM modes, some parts), but a parts/netlist step is still required. MCP tools (mcp__mplab-mcc__* exist in this session) are an API path for extraction; they were not exercised here.
   - ev: https://github.com/Microchip-MPLAB-Harmony/core driver/i2c_eeprom/at24/config/drv_at24.py:82-121
   - ev: Harmony core repo tree: driver/i2c_eeprom/at24, spi_eeprom/at25, spi_flash/at25df, sqi_flash/{sst26,mx25l,w25}, parallel_prom/{sst38,sst39}, mcp16502, pac193x, sdspi
   - ev: csp_apps_pic32cm_jh00_jh01 apps/sercom/i2c/master/i2c_eeprom/firmware/src/main.c:58 (#define APP_AT24MAC_DEVICE_ADDR 0x0054); main.c:79,174,182 (ACK-poll state APP_STATE_EEPROM_CHECK_INTERNAL_WRITE_STATUS)
   - ev: same app mcc-config.mc4 modules: cmsis, core, dfp, evsys, nvmctrl, sercom1
   - ev: https://developerhelp.microchip.com/xwiki/bin/view/software-tools/ides/extensions/ai-coding-assistant/using-mcp-tools-to-integrate-with-mcc/ ('restricted to MPLAB Harmony v3'; 'limited to Arm Cortex 32-bit MCU devices'; search snippet)

7. (high) [(a) Harmony 3 BSPs] Harmony BSPs describe only LEDs and switches (active level, pull, initial latch), as MCC 'core' symbols keyed by package pin number. A PIC32CM JH01 Curiosity Pro BSP exists (added 2020-07-31): PIN_21 (PC05) LED_AL and PIN_67 (PB19) SWITCH_AL, which matches the ATDF CMJH100 positions 21=PC05 and 67=PB19. BSP files carry Microchip's 'exclusively with Microchip products' license.
   detail: BSPs are too thin to be a board-description source, but they show Microchip already keys board facts by package pin number in MCC.
   - ev: https://github.com/Microchip-MPLAB-Harmony/bsp boards/pic32cm_jh01_curiosity_pro/config/bsp.py:26-37
   - ev: boards/sam_c21n_xpro/config/bsp.py; boards/config/bsp_common.py (pinTypes LED_AH/LED_AL/SWITCH_AH/SWITCH_AL)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:7438 (position 21 = PC05), :7484 (position 67 = PB19)
   - ev: bsp repo README table lists 95 boards; the jh01 entry is found by code search but missing from the README summary

8. (high) [LOAD-BEARING] [(a) Zephyr devicetree as a board + parts description] Microchip-authored Zephyr files for the PIC32CM JH01 Curiosity Pro describe console UART pads and roles, I2C/SPI SERCOM pads, cs-gpios, LEDs/buttons and clocks. The pinctrl macros come from an 'Autogenerated' hal_microchip header that matches the ATDF, and hal_microchip vendors the ATDFs. Zephyr has ~43 bindings for Microchip/Atmel off-chip parts and 35 MIKROE Click shields. In these, a part is a child node with compatible + reg + gpios, which amounts to a parts list with wiring.
   detail: The board DTS covers only on-board items, and the JH01 CPro DTS omits the TA100 and the CAN/LIN transceivers. Off-board parts come through overlays or shields. A devicetree overlay is the most practical open input format for 'my custom board' if customers will author one. Zephyr has no binding for MCP3008, SST26 (generic jedec,spi-nor), KSZ8863, USB2514, MTCH6102, ATECC608 or RN4870.
   - ev: zephyr boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro.dts:38-39 (led0 portc 5 ACTIVE_LOW), :57 (button portb 19 PULL_UP|ACTIVE_LOW), :196-205 (sercom4 UART), :281-289 (sercom5 SPI, cs-gpios portb 3)
   - ev: pic32cm_jh01_cpro-pinctrl.dtsi:9-22; commits through 2026-10-08
   - ev: zephyrproject-rtos/hal_microchip include/dt-bindings/pic32c/pic32cm_jh/pic32cm_jh01/pic32cm5164jh01100-pinctrl.h:1-8 ('Autogenerated file'), :126 PA4D_SERCOM0_PAD0, :261 PA8C_SERCOM0_PAD0, :1422 PB24C_SERCOM0_PAD0; packs/pic32c/pic32cm_jh/pic32cm_jh01/atdf/PIC32CM5164JH01100.atdf
   - ev: zephyr dts/bindings: 46 microchip,/atmel, off-chip files incl. 3 base files (at24, at25, at45, winc1500, enc28j60, enc424j600, lan8742, lan9250, lan865x, ksz8081, mcp230xx/23sxx family, mcp2515, mcp251xfd, mcp3204/3208, mcp4725/4728, mcp7940n, pac194x, tcn75a ...) plus jedec,jc-42.4-temp
   - ev: https://docs.zephyrproject.org/latest/boards/microchip/pic32c/pic32cm_jh01_cpro/doc/index.html (last source update Jul 22 2026)

9. (high) [LOAD-BEARING] [(a) dts2repl/Renode: existing precedent for devicetree→simulator] Antmicro's dts2repl already converts Zephyr DTS, including Microchip compatibles, into runnable Renode machines. It maps microchip,sercom-g1-uart→UART.SAMD5_UART, atmel,sam0-i2c→I2C.SAMD21_I2C and microchip,enc28j60→Network.ENC28J60, plus about 25 sensor compatibles. It has a microchip,pic32cm overlay (bring-up 2026-04-01, clock fixes 2026-08-03) that fakes GCLK/OSCCTRL status registers. In August 2026, 926/1513 Zephyr platforms passed Hello World.
   detail: This shows that (1) devicetree works as an automatic composition input and (2) shallow third-party PIC32CM simulation will exist whatever Microchip does. A Microchip twin would stand out on peripheral fidelity (real clock tree, PORT mux, SERCOM modes) and on datasheet-accurate Microchip part models. I could not check the dashboard's pass status for pic32cm_jh01_cpro because the page is rendered in JavaScript.
   - ev: https://github.com/antmicro/dts2repl dts2repl/models.json:124-127, :227, :1247 (Apache-2.0)
   - ev: dts2repl/overlay/microchip,pic32cm.repl (Tag OSCCTRL_STATUS, Python repeater peripherals for GCLK); commits 'pic32cm bringup' 2026-04-01 and 'Fix {G,M}CLK and OSCCTRL spoofs' 2026-08-03
   - ev: dts2repl/overlay/atmel,samc21n18a.repl (ApplySVD ATSAMC21N18A.svd)
   - ev: https://antmicro.com/blog/2026/08/recent-improvements-to-renode-zephyr-and-u-boot-dashboards ('926/1513 platforms passing the Zephyr Hello World demo', Aug 12 2026)

10. (medium) [LOAD-BEARING] [(a) Schematic/netlist import] I found no MCU firmware simulator that imports KiCad or Altium netlists. Board-level simulators use their own formats: Proteus schematic capture; Wokwi diagram.json (parts plus four-element connections such as ["led1:A","uno:13","green",[]]) with a beta Custom Chips API in C/WASM (experimental Verilog); PICSimLab boards plus 'spare parts'; Renode .repl. KiCad can export s-expression or XML netlists (kicadsexpr/kicadxml) whose nets list nodes (ref/pin/pinfunction). That is enough for auto-wiring if component fields (e.g. MPN) map to models.
   detail: This is an absence-of-evidence claim, and Altium was not specifically checked. Node attribute names in the KiCad netlist were not checked against a real export. A netlist alone gives connectivity, not part behavior, so part→model mapping (MPN or compatible) is the hard part.
   - ev: https://docs.wokwi.com/diagram-format
   - ev: https://docs.wokwi.com/chips-api/getting-started ('The Chips API is currently in beta')
   - ev: https://www.labcenter.com/simulation/ (mixed-mode SPICE + MCU co-simulation in Proteus' own schematic)
   - ev: https://forum.kicad.info/t/how-to-transfer-my-kicad-work-to-proteus-interface/47068 (no KiCad→Proteus importer; reply suggests asking Labcenter)
   - ev: https://docs.kicad.org/doxygen/job__export__sch__netlist_8h_source.html (netlist formats incl. KICADXML, KICADSEXPR)
   - ev: https://lcgamboa.github.io/picsimlab_docs/stable/SpareParts.html

11. (high) [LOAD-BEARING] [(b) QEMU's existing Microchip-family off-chip models (local grep)] Upstream QEMU (11.1.50, 602e4f2da1) has dedicated types for EMC1413/EMC1414 (QOM temperatureN properties plus a qtest), a generic AT24C I2C EEPROM, SST25VF/SST25WF and Atmel AT25/AT26/AT45 flash plus AT25128A/AT25256A non-JEDEC EEPROM entries and a Microchip 25CSM04 entry in m25p80, and the SMSC LAN9118 MAC plus its PHY. It has nothing for MCP9808, 24AA/24LC by name, 25AA/25LC, SST26, MCP7940/79410, MCP23xxx, MCP3008/3201, MCP4725/4921, MCP2515/2518FD, ATA6563, LAN8742, LAN9250, KSZ, ENC28J60, USB2514, PAC193x, MTCH6102, ATECC, RN4870 or WINC/WILC (each grep -ril returned 0 files in hw/include/tests/docs).
   detail: The QEMU I2C-slave catalog is about 21 files (TMP105/421, DS1338, M41T80, PCA9552/9554, PCF8574, AT24C, EMC141x, ADC128D818, DPS310 ...). The SSI-peripheral catalog outside hw/ssi is only m25p80, ssd0323 and ssi-sd. Analog models for the missing parts exist, though: tmp105→MCP9808, ds1338/m41t80→MCP7940N, pca9554/pcf8574→MCP23008.
   - ev: hw/sensor/emc141x.c:50, :53-90 (get/set temperature), :308 'emc1413', :314 'emc1414'; include/hw/sensor/emc141x_regs.h; tests/qtest/emc141x-test.c:18-36 (qom-get/qom-set temperature0)
   - ev: hw/nvram/eeprom_at24c.c:30 (TYPE 'at24c-eeprom'), :231-234 (rom-size, address-size, writable, drive)
   - ev: hw/block/m25p80.c:178-196 (Atmel AT25/AT26/AT45 + at25128a/at25256a-nonjedec), :313-321 (sst25vf*/sst25wf*), :371-372 ('25csm04' under /* Microchip */)
   - ev: hw/net/lan9118.c:267, :1278 (embeds TYPE_LAN9118_PHY), :1324; hw/net/lan9118_phy.c (222 lines, SMSC PHY IDs; also used by imx_fec)
   - ev: hw/arm/aspeed_ast2500_g220a.c:35-47 (emc1413 instantiated at 0x4c)

12. (high) [LOAD-BEARING] [(b) QEMU board-composition mechanics (local)] QEMU can attach I2C/SPI part models to named buses from the command line and change their state at runtime through QMP qom-set; upstream functional tests do exactly this. SPI flash can be attached with cs and drive from the CLI. GPIO wiring (CS/INT/LED/button) has no CLI/QMP equivalent and is done in C machine code, as in sabrelite with its own TODO. CAN nets are '-object can-bus', and the host SocketCAN bridge builds only on Linux. The qtest protocol's set_irq_in/irq_intercept_out are tools for testing device models.
   detail: The qapi/ directory is not checked out, so a QMP GPIO-connect command cannot be fully ruled out; none appears in system/ or docs/. I believe the qtest accelerator does not run guest code, which would make qtest a validation harness rather than a stimulus channel during firmware runs (medium confidence on that nuance). In practice a board twin needs a machine that reads a board-description file (JSON or DTS) and does the GPIO wiring in C.
   - ev: tests/functional/arm/test_aspeed_ast2500_buildroot.py:23-36 ('-device tmp105,bus=aspeed.i2c.bus.3,address=0x4d' then qom-set temperature=18000, observed by guest Linux)
   - ev: docs/system/arm/aspeed.rst:150-157 ('-device mx66u51235f,bus=ssi.0,cs=0x0,drive=fmc0')
   - ev: docs/specs/tpm.rst:392 ('-device tpm-tis-i2c,...,bus=aspeed.i2c.bus.12,address=0x2e')
   - ev: hw/i2c/core.c:22 (address prop); hw/ssi/ssi.c:112 (cs prop), :95-105 (SSI_GPIO_CS input)
   - ev: hw/arm/sabrelite.c:74-101 (sst25vf016b on spi1; CS from gpio[2] line 19 via qdev_connect_gpio_out)
   - ev: system/qdev-monitor.c:813-822 (gpio lists printed only)
   - ev: docs/system/devices/can.rst:40-56; net/can/meson.build:3-5 (can_socketcan.c only if host_os == 'linux')
   - ev: system/qtest.c:234-262, :379, :434

13. (high) [LOAD-BEARING] [(b) Renode part models (GitHub, renode-infrastructure master, 2026-09-28; Renode 1.17.0 released 2026-09-07)] Renode has Microchip-relevant models for ENC28J60 (ISPIPeripheral, 26.9 KB), PAC1934 (II2CPeripheral, 12.5 KB), KS8851 (Micrel/Microchip), AT86RF233, generic I2C EEPROM, generic SPI flash, a generic EthernetPhysicalLayer, UsbHub (deprecated API), TCA6416 (GPIO expander analog), an AtCommandModem base (DA16200, Quectel), Silabs_SecureElement (98.6 KB, a precedent for vendor SE models), MCAN, and CANHub/LINHub/UARTHub board-level nets. It has on-chip SAMD21/SAMD5 models and Microchip platforms (atsamd21j17d-aft, atsamd51g19a, sam4s, sam_e70, mpfs-icicle-kit, miv). It has no MCP-series parts.
   detail: Renode's board .repl attaches parts to controller objects and wires GPIOs (LED/Button) in text, which is the composition model a QEMU twin would need to replicate. Sensor stimulus comes through RESD data files; QEMU's equivalent is QOM properties.
   - ev: https://github.com/renode/renode-infrastructure src/Emulator/Peripherals/Peripherals/Network/ENC28J60.cs:21 ('class ENC28J60 : ISPIPeripheral, IMACInterface')
   - ev: src/Emulator/Peripherals/Peripherals/Sensors/PAC1934.cs:18 ('class PAC1934 : II2CPeripheral')
   - ev: tree sizes: AtCommandModem.cs 20365 B, MCAN.cs 61015 B, GenericI2cEeprom.cs 6347 B, TCA6416.cs 4100 B, LED.cs 1534 B, Button.cs 2988 B, Silabs_SecureElement.cs 98597 B, KS8851.cs 8634 B, AT86RF233.cs 13504 B
   - ev: https://github.com/renode/renode platforms/cpus/atsamd21j17d-aft.repl (added 2024-10-16), platforms/boards/sam4s_xplained.repl:1-9
   - ev: gh api repos/renode/renode/releases/latest → v1.17.0 2026-09-07

14. (medium) [LOAD-BEARING] [(b) Part-model effort and fidelity (estimate anchored to existing model sizes)] Most popular Microchip attach parts are cheap to model at functional level: I2C/SPI sensors, EEPROMs, RTCCs, GPIO expanders, ADCs/DACs and power monitors are register-file devices (QEMU analogs are 244–511 lines: ds1338 244, at24c 266, emc141x 326, pca9554 357, tmp105 511). CAN-FD/Ethernet controllers and Wi-Fi/BLE modules are large (QEMU sja1000 986 lines, lan9118 1350; Renode ENC28J60 27 KB, MCAN 61 KB). Secure elements are large and sensitive. Value depends on datasheet-faithful edge cases. QEMU's AT24C model wraps the address at the end of the array rather than at the page, always ACKs (no tWR busy NACK) and has no WP. Firmware bugs at page boundaries, or a missing ACK-poll, would pass in the twin and fail on hardware.
   detail: Complexity grades are my judgment from datasheet scope plus these size anchors. An MCU twin's SERCOM model fidelity and part-model fidelity are both needed for board-level value.
   - ev: qemu wc -l: hw/rtc/ds1338.c 244, hw/nvram/eeprom_at24c.c 266, hw/sensor/emc141x.c 326, hw/gpio/pca9554.c 357, hw/sensor/tmp105.c 511, hw/usb/dev-hub.c 704, hw/net/can/can_sja1000.c 986, hw/net/lan9118.c 1350, hw/block/m25p80.c 1956
   - ev: hw/nvram/eeprom_at24c.c:62-87 (event handler returns 0 on START_SEND/FINISH), :113-134 (ee->cur = (ee->cur + 1u) % ee->rsize)
   - ev: Harmony i2c_eeprom main.c:79,174,182 (firmware relies on ACK polling)

15. (medium) [(b) Secure elements (ATECC608, TA100) need policy review] Secure-element models should be command-level only, with fixed test keys, and reviewed internally. SparkFun's ATECC608A listing says the complete datasheet requires an NDA. The ATECC608B has a public summary datasheet (DS40002239A) and a TrustFLEX data sheet online. CryptoAuthLib is public. Microchip's own JH01 Curiosity Pro carries a TA100 on SERCOM5 SPI (shared with EXT2), so a faithful twin of that kit runs into this issue directly.
   detail: I found no CryptoAuthLib software emulator. The AWS FreeRTOS 'Windows simulator' guide still requires a real ATECC608A Click.
   - ev: https://www.sparkfun.com/products/15838 (NDA statement for ATECC608A complete datasheet; via search snippet)
   - ev: onlinedocs ATECC608B-TFLXTLS CryptoAuthentication Data Sheet (search result; onlinedocs returned 503 to direct fetch)
   - ev: UG DS70005482B feature list 'Trust Anchor (TA100) Secure Element' and EXT2 rows PB00/PB01/PB02 shared with TA100
   - ev: renode Silabs_SecureElement.cs (98,597 B) precedent

16. (high) [LOAD-BEARING] [(b) Microchip's own dev kit as a bounded board-level target] The PIC32CM JH01 Curiosity Pro user guide (DS70005482B, 2022) lists a Microchip 'Total System Solutions' BOM: 2× ATA6561 CAN transceivers (CAN0 PA24/PA25, CAN1 PB14/PB15) with jumpers for loopback between the two CANs, an ATA663211 LIN transceiver, a TA100, MIC5504/MIC5209 LDOs, and EDBG (AT32UC3A4256) providing VCOM on SERCOM4 PB10/PB11. It also has three Xplained Pro EXT headers whose extension boards are auto-identified through ID chips in MPLAB X, an Arduino Uno header, and a driven-shield touch button.
   detail: A twin of Microchip's own kit is a first board-level target that Microchip fully controls (data plus every model), and it already shows the attach-part ('TSS') story. Renode's CANHub/LINHub and QEMU's can-bus object are the relevant net abstractions.
   - ev: UG DS70005482B features list (Two CAN transceivers with jumpers ... loopback; LIN transceiver; Three extension headers; Arduino uno connector; Auto-ID for daughter board identification in MPLAB X IDE)
   - ev: UG Table 1-2 'Microchip Total System Solutions (TSS)'; Tables 4-21/4-22 (CAN0/CAN1 → ATA6561); Table 4-33 (VCOM)
   - ev: UG section 3 paragraph: 'Each extension board has an identification (ID) chip ... read and displayed on the MPLAB X IDE window'

17. (medium) [LOAD-BEARING] [(c) mikroBUS on Microchip boards] Zephyr board docs mention mikroBUS for the PIC32CK GC01/SG01 Curiosity Ultra, PIC32CM GC00/SG00 Curiosity Pro, PIC32CX SG41 Curiosity Ultra, PIC32CZ CA80/CA90 Curiosity Ultra, SAM E54 Curiosity Ultra, SAMA5D27-SOM1-EK1 and SAMA7D65 Curiosity. They do not mention it for the PIC32CM JH01 Curiosity Pro, the JH01/PL10 Curiosity Nano or any Xplained Pro board. The JH01 Curiosity Pro user guide lists three Xplained Pro EXT headers plus an Arduino Uno connector; 'mikroBUS' appears only in the family-level intro sentence. Curiosity Nano boards need the AC164162 base (3 mikroBUS sockets), and Xplained Pro boards need ATMBUSADAPTER-XPRO (with an ATSHA204A ID chip). The 2017 MCC/Click press release says Curiosity, Xpress, Explorer and PICDEM Lab boards have mikroBUS.
   detail: Among Microchip boards, only sama7g54_ek defines the Zephyr 'mikro-bus' nexus (mikrobus_header) in its DTS. Zephyr Click shields therefore cannot be dropped onto Microchip Curiosity Ultra/Pro boards without adding nodelabels; a twin could own that mapping.
   - ev: zephyr boards/microchip/pic32c/{pic32ck_gc01_cult,pic32ck_sg01_cult,pic32cm_gc00_cpro,pic32cm_sg00_cpro,pic32cx_sg41_cult,pic32cz_ca80_cult,pic32cz_ca90_cult}/doc/index.rst; boards/microchip/sam/{sam_e54_cult,sama5d27_som1_ek1,sama7d65_curiosity}/doc/index.rst (grep 'mikro')
   - ev: zephyr pic32cm_jh01_cpro, pic32cm_jh01_cnano, pic32cm_pl10_cnano, atmel/sam0/* Xpro docs: 0 mentions; pic32cx_sg61_cult: 0 mentions (absence in docs only)
   - ev: UG DS70005482B line 14 (family boilerplate) vs. features list (three EXT headers, Arduino uno connector)
   - ev: AC164162: Microchip/Mouser listings ('Three mikroBUS Click board', 'One Xplained Pro Extension header'); ATMBUSADAPTER-XPRO: https://www.microchip.com/en-us/development-tool/ATMBUSADAPTER-XPRO (search snippet)
   - ev: https://ir.microchip.com/news-events/press-releases/detail/475/microchip-technology-supporting-mikroelektronika-click-boards-in-mplab-code-configurator (2017)

18. (medium) [LOAD-BEARING] [(c) Virtual Click catalog as a library unit] A virtual Click works well as a packaging and demo layer on top of part models: a part model plus socket-relative wiring. Ready-made descriptions exist. MIKROE reached 2,000 Click boards in July 2026. The MikroElektronika/click_id repo has 144 .mnfs manifests (driver string, protocol, address, mikroBUS pin states), e.g. THERMO 8 → 'mcp980x' @0x18, ADC → mcp3204, RTC 6 → mcp7941x. Zephyr has 35 MIKROE shields relative to mikrobus_spi/i2c/header, including CAN SPI (mcp2515), MCP2518FD (mcp251xfd), ETH (enc28j60), ETH 3 (lan9250) and EERAM 3.3V (47L16 as at24). The core library unit should still be the part model, because production custom boards do not carry Click boards.
   detail: Caveat: ClickID driver strings are Linux-centric and community-curated. THERMO 8 carries an MCP9808, yet its manifest names the LM75-family 'mcp980x' driver with header comment CONFIG_SENSORS_LM75, so manifests cannot select models without curation. Upstream status of the Linux mikroBUS/ClickID driver was not verified (BeaglePlay uses a patched kernel).
   - ev: https://www.hackster.io/news/mikroe-s-click-ecosystem-grows-to-2-000-boards-with-the-launch-of-the-nxp-powered-rtc-27-click-8603023162a4 and https://www.businesswire.com/news/home/20260715528020/en/ (via search; direct fetch 403)
   - ev: https://github.com/MikroElektronika/click_id clicks/THERMO-8/manifest/THERMO-8-CLICK.mnfs:1-5,38-44; clicks/ADC, clicks/RTC-6 manifests
   - ev: Thermo 8 Click = MCP9808 (MIKROE-3290, Mouser/RS listings via search)
   - ev: zephyr boards/shields/mikroe_* (35 dirs); mikroe_eth_click.overlay:5-17; mikroe_eth3_click.overlay; mikroe_eeram_33v_click.overlay; mikroe_mcp251x_click/mikroe_can_spi_click.overlay; mikroe_mcp251xfd_click/mikroe_mcp2518fd_click.overlay; dts/bindings/gpio/mikro-bus.yaml

19. (medium) [LOAD-BEARING] [(d) MPLAB SIM and SCL] The MPLAB X simulator supports asynchronous stimulus (one-shot pin or UART-RX changes from the GUI) and synchronous stimulus (scripted pin/SFR/GPR sequences), register injection (e.g. UART RX fed from a message file containing 'wait 4 ms' style delays), UART IO to files, and SCL scripts that inject values onto pins or data memory. SCL can be generated from the synchronous stimulus tabs, and the SCL User's Guide ships in MPLAB X Help. Peripheral support on Arm SAM parts is partial: a 2021 report on ATSAMD21J18A shows warnings for partial PORT_GROUP0/1 and TC7 support and 'We don't simulate the clock selection'. The JH DFP declares a simulator profile for all devices.
   detail: The relationship is complementary. SIM stays the in-IDE, single-chip, cycle/register-level debugger with stimulus. A board twin adds execution speed, external part models, host I/O bridges (COM/TCP/CAN), headless CI and multi-device scenarios. SCL's pin/register-over-time semantics map naturally onto a board-level stimulus API (QOM properties / pin set), which gives SIM users a migration path; the EDC/.emu mapping knowledge transfers directly.
   - ev: https://onlinedocs.microchip.com/oxy/GUID-240F27AA-C615-4705-B68C-6E434B126B47-en-US-1/GUID-1B6DFBC0-09DB-4C7A-9DD8-C212048A0E93.html (SCL) and .../GUID-310471A3-B3EE-4633-AC8D-4348B11BB2DB.html (Using Stimulus), .../GUID-2744AB60-F5BA-4D38-A278-36765E33C430.html (USART) — content via search snippets; direct fetch returned 503
   - ev: https://www.yorku.ca/professor/drsmith/2021/04/15/simulating-micros-from-command-line/
   - ev: PIC32CM-JH01/hwtools/sdm/sdmSimulator.xml (devicename="*" profile="sim" supported; runtimewatch/trace/analyzer/registertrace streams)

20. (medium) [(d) MPLAB Data Visualizer] Data Visualizer takes input from serial COM ports (CDC) and DGI-capable debuggers, decodes the Data Stream Protocol, and runs in MPLAB X, standalone, and (since the June 2024 early access) inside VS Code. Microchip states that over a COM port 'any target or board can be supported, also user-specific boards'. QEMU on Windows can bind a guest UART to a host COM port, so with a virtual null-modem pair DV could consume a twin's UART. DGI (EDBG SPI/I2C/GPIO/power) cannot realistically be emulated.
   detail: I did not verify whether DV accepts TCP/socket input. The virtual COM pair would be third-party software (e.g. a com0com-class driver), which has deployment implications on locked-down customer PCs.
   - ev: https://www.microchip.com/mplab/avr-support/data-visualizer and https://microchip.com/en-us/tools-resources/debug/mplab-data-visualizer (via search snippets; direct fetch 403)
   - ev: https://www.microchip.com/en-us/about/news-releases/products/early-access-to-mplab-extensions-for-vs-code (DV in VS Code; via eetasia/electronicspecifier coverage)
   - ev: qemu chardev/char.c:451-455 ('COM' → serial backend), chardev/char-serial.c:42-55 (win_chr_serial_init)

21. (medium) [(d) MPLAB Mindi] Mindi is a free, locally installed analog/power circuit simulator built on the SIMetrix (enhanced SPICE) and SIMPLIS (piecewise-linear) engines, preloaded with Microchip analog models (more than 300 products as of the 2017 update: DEPA controllers, linear regulators, MOSFET and LED drivers). A Microchip help page from 2023 references Mindi 8.50. It does not simulate MCU firmware, so it complements a twin.
   detail: A plausible integration is to export Mindi/SIMetrix waveforms as stimulus for the twin's ADC/AC inputs. I did not verify that Mindi exports waveforms.
   - ev: https://audioxpress.com/news/microchip-updates-mplab-mindi-analog-simulator-to-include-over-300-device-models-and-schematics (Oct 20 2017)
   - ev: https://www.microchip.com/content/dam/mchp/documents/OTH/ProductDocuments/Brochures/20005617A.pdf (brochure; via search)
   - ev: https://developerhelp.microchip.com/xwiki/bin/view/software-tools/analog/mindi/mindi-intro/welcome-page (last modified 2023-11-09; via search)

22. (medium) [LOAD-BEARING] [(d) Microchip simulation precedents and announcements] Microchip already distributes simulators. Renode 'ships as an integrated part of SoftConsole' (Dec 2022) and has been 'the default virtual development option in Microchip's SoftConsole IDE' for PolarFire SoC, with FPGA co-simulation through Verilator; PFSoC 2 enablement was under way (Jul 2024). Microchip lists Proteus VSM 'RTC Edition' bundles (SW500220 8-bit, SW500221 16-bit) for its training classes, integrated with MPLAB X as virtual hardware. The QEMU icicle-kit machine is 'Odd Fixes' with no microchip.com maintainer. I found no 2023–2026 Microchip announcement of an MCU digital twin, simulation-as-a-service, or QEMU/Renode support for PIC32C/SAM MCUs or PIC64GX, and the MPLAB VS Code early access (June 2024) did not list a simulator.
   detail: These are absence claims based on web search; Microchip-hosted pages were blocked (403/503) from this environment. Re-checking with the mplab-docs MCP server once it reconnects is advisable.
   - ev: https://antmicro.com/blog/2022/12/microchip-riscv-support-in-renode (Dec 29 2022)
   - ev: https://antmicro.com/blog/2024/07/open-source-som-with-polarfire-soc (Jul 8 2024)
   - ev: https://www.microchip.com/en-us/development-tool/SW500221 and /sw500220 (via search snippets; direct fetch 403)
   - ev: qemu MAINTAINERS:1795-1799 (Conor Dooley <conor@kernel.org>, Sebastian Huber; S: Odd Fixes)
   - ev: https://www.eetasia.com/microchip-launches-early-access-version-of-mplab-extensions-for-vs-code/
   - ev: web searches for Microchip + Renode/QEMU/digital twin 2023–2026 and PIC64GX Renode/QEMU returned no such announcement

23. (medium) [LOAD-BEARING] [(a/d) Board-level simulator comparators] None of the board-level simulators I found covers Microchip's 32-bit Arm MCUs or provides a Microchip part library. Wokwi supports ATmega328P/2560 and ATtiny85, but no SAM/PIC; its parts list has no Microchip parts besides those AVRs. Labcenter lists PIC10/12/16/18/24 and dsPIC33 plus Atmel AVR, Cortex-M3 and 8051, and no PIC32/SAM. PICSimLab (GPL-2.0, active 2026-10) emulates boards including Microchip's Curiosity, Curiosity HPC and Xpress, integrates with the MPLAB X debugger, and offers MCP23017/MCP23S17/24CXXX among its parts, using QEMU forks only for STM32/ESP32.
   detail: This gap (board-level, 32-bit Arm Microchip MCUs, Microchip attach-part models, MPLAB integration) is the space a Microchip board twin would occupy.
   - ev: https://docs.wokwi.com/getting-started/supported-hardware
   - ev: https://www.labcenter.com/microprocessors/
   - ev: https://github.com/lcgamboa/picsimlab (README; pushed 2026-10-04)
   - ev: https://lcgamboa.github.io/picsimlab_docs/stable/Boards.html; .../SpareParts.html

24. (high) [LOAD-BEARING] [(e) Customer-value data: AspenCore 2023 Embedded Markets Study (verified from PDF)] Of 655 respondents (fielded Feb 9 – Mar 3 2023), 11% currently use QEMU as a system-level design tool (MATLAB 32%, FPGA prototypes 22%, SystemC 22%, Simulink 17%, Cadence VSP 9%, Synopsys Virtualizer 4%; none 26%). Respondents expect these to become more important: software test 44%, simulation 37%, emulation 31%, virtual prototyping 24%. 'Virtual prototype/TLM model available' scored 40% as a processor-selection factor, against 85% for software development tools and 78% for hardware development tools. The debugging process (41%) and testing/systems integration (40%) were among the top current challenges; meeting schedules scored 32%.
   detail: These are data, not opinion. The Q14/Q47 percentages are the slides' "'Very Important' Summary" metric. The data says simulation interest is real but not a purchase driver, and test/debug pain is high. It does not distinguish chip-level from board-level simulation.
   - ev: https://www.embedded.com/wp-content/uploads/2023/05/Embedded-Market-Study-For-Webinar-Recording-April-2023.pdf p.2 (methodology), p.11 (Q14), p.28 (Q47), p.37 (Q60), p.38 (Q62); local copy C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-microchip-board-inputs/dl-ems2023/ems2023.pdf

25. (medium) [(e) Other evidence (data separated from opinion)] Case evidence: Google's ChromeOS EC/FPMCU testing moved to Renode because reflashing a physical MCU for 'tens of binaries' does not scale; simulation enabled 'automated testing ... in CI at scale, in a deterministic manner' (Aug 2023). Renode dashboards simulate about 1,000 devicetree-generated platforms (2026). Weaker data: the Memfault/VDC 2024 survey (775+ participants) reports slow defect handling (about half need more than a week to root-cause) but nothing on simulation; VDC via EDN reported 28% using virtual prototyping against 92% physical, but that is from 2007; Perforce's 2026 automotive report says 59% have implemented or are implementing shift-left, down 10%, which is not simulation-specific. Claims that HIL is a bottleneck appear in vendor and arXiv sources and are opinion.
   detail: I found no verified survey quantifying how often MCU teams are blocked by board or hardware availability, or what share of customers would build a board model. A Microchip customer survey or field interviews would be needed to size board-level demand.
   - ev: https://opensource.googleblog.com/2023/08/ ('ChromeOS EC testing suite in Renode for consumer products', Aug 3 2023)
   - ev: https://antmicro.com/blog/2026/08/recent-improvements-to-renode-zephyr-and-u-boot-dashboards
   - ev: https://electronicspecifier.com/iot/state-of-iot-survey-exposes-status-quo-is-no-longer-adequate (Apr 25 2024)
   - ev: https://www.edn.com/?p=4071953 (Jun 13 2007)
   - ev: https://www.perforce.com/resources/sca/2026-state-automotive-software-development-report/shift-left-adoption
   - ev: https://arxiv.org/abs/2602.18142 (opinion/research framing)

## Table: Board description sources
| artifact | owner/tool | what it encodes | format/location | usable for auto-wiring? | notes |
| ATDF <signals>/<pinouts> + PORT/SERCOM value-groups | Microchip DFP (also vendored in Zephyr hal_microchip) | Per-instance pad / function letter / PAD index; package position→pad; PMUX letter→code; SERCOM DOPO/DIPO/TXPO/RXPO pad roles | XML: <DFP>/<family>/atdf/*.atdf (Apache-2.0) | Yes — MCU side (static capability + runtime decode) | No <pins> element; roles only as caption text; no electrical or I2C-capable-pad data |
| EDC .PIC PinList | MPLAB X device DB (DFP edc/) | Pin-ordered VirtualPins: name, ppsfunction letter, owning peripheral | XML: <DFP>/<family>/edc/*.PIC | Yes — MCU side (equivalent to ATDF) | Same data path as MPLAB SIM; ppsflavor="atmel" |
| SVD | DFP svd/ | Registers/bitfields only | CMSIS-SVD XML | No | 0 pad/signal entries in PIC32CM5164JH01100.svd |
| MCC Harmony project | MCC + Harmony content | Per-pin function (e.g. SERCOM1_PAD0), PMUX letter, dir/latch/pulls; component list; driver params (AT24 addr/size/PLIB) | YAML: <proj>.X/<cfg>/components/*.yml, mcc-config.mc4, mcc-manifest-*.yml; generated src/config/<cfg>/pin_configurations.csv | Partial — MCU side + parts backed by MCC drivers | No nets; parts driven straight from the PLIB are invisible (JH01 i2c_eeprom demo hardcodes 0x54 in main.c) |
| MCC Melody .mc3 | MCC Melody | Signal→pin (PPS) maps per module; module dependency graph | XML envelope + HTML-escaped JSON state | Partial — MCU side | Format undocumented; device-specific @mchp-mcc/*-pin-manager packages |
| MCC Classic .mc3 | MCC Classic | Register-level 'Pin Module' settings (LATx, ANSEL, IOC) | XML OptionKey/CustomKey | Weak | Mostly 8/16-bit PIC projects |
| MCC MCP tools | MCC (VS Code) | Machine-readable MCC capabilities/symbols | MCP server | Possible extraction API | Harmony v3 + Arm Cortex 32-bit only per Microchip page; not exercised here |
| Harmony 3 BSP | Microchip-MPLAB-Harmony/bsp | LEDs/switches: package pin #, active level, pull, latch | Python bsp.py setting 'core' symbols | Minimal | Microchip-only license; JH01 CPro: PIN_21 LED_AL, PIN_67 SWITCH_AL |
| Zephyr board DTS + pinctrl | Zephyr (Microchip-authored for PIC32C) | Console/I2C/SPI SERCOM pads + roles, cs-gpios, LEDs/buttons, clocks | DTS/DTSI; pinctrl macros autogenerated from ATDF (hal_microchip) | Yes — best public format | On-board items only; JH01 CPro DTS omits TA100 and CAN/LIN transceivers |
| Zephyr shields / DT overlays | Zephyr, MIKROE, users | Off-board parts: compatible, reg/address, int/cs gpios relative to a connector | DT overlay | Yes | ~43 Microchip/Atmel off-chip bindings; 35 MIKROE shields; mikro-bus nexus defined only on sama7g54_ek among Microchip boards |
| ClickID manifest | MIKROE | Click identity, Linux driver string, protocol, address, mikroBUS pin states | Greybus-style .mnfs in on-board EEPROM; 144 in GitHub repo | Yes at Click level (needs curation) | Driver strings are Linux-centric (THERMO 8 = MCP9808 but uses 'mcp980x') |
| Xplained Pro / mikroBUS-XPRO ID chip | Microchip EDBG / MPLAB X | Extension identity, features, serial number, doc links | ID chip on extension (ATSHA204A on ATMBUSADAPTER-XPRO) | Auto-detects dev-kit extensions only | Data format not verified publicly |
| Dev-kit user guide + design files | Microchip | Header/pin/net tables; BOM incl. 'Total System Solutions' | PDF + design-file package | Semi-manual | e.g. DS70005482B Tables 4-4..4-33 |
| KiCad netlist | Customer CAD | Components (ref/value/fields) + nets (node ref/pin/pinfunction) | kicad-cli sch export netlist (kicadsexpr/kicadxml) | Yes, with a part→model map | No MCU-simulator importer found; Altium not checked |
| Renode .repl (via dts2repl) | Antmicro | Peripherals, bus attachment, GPIO connections | Text .repl; generated from DTS | Precedent | microchip,pic32cm overlay spoofs clock status |
| Wokwi diagram.json | Wokwi | Parts + pin-to-pin connections | JSON | Precedent | Microchip coverage limited to AVR (ATmega328P/2560, ATtiny85) |
| Proteus schematic | Labcenter (Microchip lists RTC Edition bundles) | Full schematic incl. MCU + peripherals | Proprietary | Precedent (closed) | Lists PIC10–dsPIC33 and Atmel AVR/Cortex-M3; no PIC32/SAM listed |

## Table: Off-chip part models
| part | interface | model complexity | existing QEMU model? | existing Renode model? | notes |
| MCP9808 | I2C (JC-42.4 regs) | Low | No (analog: tmp105, 511 lines) | No (analogs TMP103/TMP108/AS6221) | Zephyr jedec,jc-42.4-temp; Thermo 8 Click |
| EMC1414 (EMC1413) | I2C/SMBus | Low | Yes — emc1413/emc1414 (hw/sensor/emc141x.c), QOM temperatureN + qtest | No | Only current Microchip attach part with a dedicated QEMU type |
| 24AA/24LC | I2C | Low (but page-wrap, tWR ACK-poll NACK, WP) | Generic at24c-eeprom (no page wrap, always ACKs) | GenericI2cEeprom | Harmony drv_at24; Zephyr atmel,at24 / at24mac402 |
| 25AA/25LC | SPI | Low | Partial: m25p80 at25128a/at25256a-nonjedec EEPROMs + Microchip 25csm04; none named 25AA/25LC | No dedicated model | Zephyr atmel,at25; Harmony drv_at25 |
| SST26 (SQI flash) | SPI/SQI | Medium (quad I/O, SFDP, block protection/ULBPR) | No (m25p80 has SST25VF/SST25WF only) | No dedicated (GenericSpiFlash, MT25Q, MX25R) | Harmony drv_sst26 |
| MCP7940N / MCP79410 | I2C | Low-Medium (BCD, ST/OSCRUN, alarms, MFP; 79410 adds EEPROM + UID address) | No (analogs ds1338/m41t80/rs5c372) | No (analogs ABRTCMC, RV8803) | Zephyr microchip,mcp7940n; RTC 6 Click manifest 'mcp7941x' |
| MCP23008 / MCP23017 / MCP23S17 | I2C / SPI | Low-Medium (IOCON.BANK, INTCAP/INTF, INT pins) | No (analogs pca9554/pca9552/pcf8574) | No (analog TCA6416) | Zephyr binds the whole family; PICSimLab has MCP23017/MCP23S17 |
| MCP3008 / MCP3201 | SPI | Low (bit-framed SPI; needs analog stimulus) | No | No | Zephyr has mcp3204/3208, not 3008; ADC Click manifest 'mcp3204' |
| MCP4725 / MCP4921 | I2C / SPI | Low (output = analog net value) | No | No (analog AD56x1_DAC) | Zephyr microchip,mcp4725 |
| MCP2515 | SPI + CAN | Medium-High (instruction set, 3 TX/2 RX buffers, filters, modes, INT) | No (can-bus object + SJA1000/CTU CAN FD/FlexCAN exist; SocketCAN bridge Linux-only) | No (MCAN, CANHub exist) | Zephyr shield mikroe_can_spi_click |
| MCP2518FD | SPI + CAN FD | High (RAM FIFOs, TEF, timestamps, CRC/ECC SPI) | No | No | Zephyr microchip,mcp251xfd; mikroe_mcp2518fd_click |
| ATA6563 (ATA6561 on JH01 CPro) | CAN PHY (TXD/RXD/STBY) | Trivial (pass-through + standby gate) | No (not needed beyond the can-bus object) | No (CANHub) | Needed for a twin of Microchip's own kit (2× ATA6561 with loopback jumpers) |
| LAN8742A | RMII + MDIO | Low-Medium (clause-22 regs, link state) | No dedicated; lan9118_phy generic SMSC PHY (222 lines) | Generic EthernetPhysicalLayer | Only relevant for MCUs with a MAC (not PIC32CM JH); Zephyr microchip,lan8742 |
| LAN9250 | SPI/SQI/HBI | High | No (lan9118 memory-mapped sibling, 1350 lines; reuse unverified) | No | Zephyr microchip,lan9250; ETH 3 Click shield |
| KSZ8863 | MII/RMII + SPI/I2C/SMI mgmt | High (3-port switch) | No | No (KS8851 is a different part) | No Zephyr binding found in main |
| ENC28J60 | SPI | Medium-High | No | Yes — ENC28J60.cs (26.9 KB); dts2repl maps microchip,enc28j60 | Zephyr shield mikroe_eth_click |
| USB2514 | USB + SMBus config | Low for SMBus config; USB side needs a host-controller model | No (generic usb-hub dev-hub.c, 704 lines) | No (deprecated generic UsbHub) | PIC32CM JH has no USB |
| PAC1934 | I2C | Low-Medium (accumulators, REFRESH, block read; needs V/I traces) | No | Yes — PAC1934.cs (12.5 KB) | Harmony drv_pac193x; Zephyr has pac194x only |
| MTCH6102 | I2C | Medium (touch/gesture registers; driven by stimulus) | No | No | Zephyr has mtch9010 (different part) |
| ATECC608 / TA100 | I2C or SWI / SPI (TA100) | High + sensitive | No | No (Silabs_SecureElement precedent, 98.6 KB) | Test keys, command-level only; ATECC608A full datasheet NDA-gated; TA100 on JH01 CPro SERCOM5 |
| RN4870 | UART ASCII commands | Medium (behavioral) | No | No (AtCommandModem base; DA16200/Quectel precedents) | Host side is just a SERCOM USART |
| WINC1500 | SPI (HIF protocol) | High | No | No | Zephyr atmel,winc1500 |
| ATWILC1000/3000 | SPI/SDIO | High | No | No | Host-driver protocol model; not checked further |

## Table: Adjacent Microchip tools
| tool | what it does | relation to twin | source |
| MPLAB SIM (MPLAB X simulator) + SCL | Instruction simulation with per-device (partial) peripheral models; async/sync stimulus on pins/SFR/GPR; register injection (e.g. UART RX from files); SCL scripts; UART IO | Complement: SIM stays the in-IDE single-chip debugger; the twin adds speed, board parts, host I/O bridges, CI. Reuse EDC pin data and map SCL-style stimulus onto the twin's stimulus API | onlinedocs Simulator help GUID-240F27AA... (via search); yorku.ca 2021 MDB/SAMD21 report; DFP hwtools/sdm/sdmSimulator.xml |
| MPLAB Data Visualizer | Plots/terminal from a COM port (CDC) or DGI; Data Stream Protocol; MPLAB X plugin, standalone, VS Code | Consumer: twin UART → Windows COM (QEMU '-serial COMn') via a virtual null-modem pair; DGI not emulatable; TCP input unverified | microchip.com DV pages (via search); qemu chardev/char.c:451-455 |
| MPLAB Mindi | SIMetrix/SIMPLIS SPICE + piecewise-linear analog/power simulation with Microchip analog models (300+ in 2017) | Complement: analog/power front-end; could provide ADC/AC stimulus waveforms (export path unverified) | audioxpress.com 2017 article; Microchip brochure 20005617A; developerhelp Mindi page |
| Proteus VSM RTC Edition (SW500220 8-bit / SW500221 16-bit) | Labcenter schematic + MCU co-simulation bundles that Microchip lists for its training classes; acts as virtual hardware with MPLAB X | Closest existing board-level tool distributed through Microchip; no PIC32C/SAM coverage found | microchip.com/en-us/development-tool/SW500221 (via search); labcenter.com/microprocessors |
| SoftConsole + Renode (PolarFire SoC / Mi-V) | Full-system virtual platform; FPGA fabric co-simulation via Verilator | Precedent for Microchip shipping an open-source simulator as the default dev option | antmicro.com/blog/2022/12 and /2024/07 |
| MCC (Harmony/Melody) + MCC MCP tools | Pin/peripheral/driver configuration and code generation; machine-readable API | Main MCU-side input for an auto-generated board description; driver components also name some attached parts | github csp_apps_pic32cm_jh00_jh01; developerhelp MCP-tools page |
| Harmony 3 core drivers | MCC components for AT24, AT25, AT25DF, SST26, MX25L, W25, SST38/39, MCP16502, PAC193x, SDSPI | Natural first set of part models (they match parts customers already configure in MCC) | github.com/Microchip-MPLAB-Harmony/core driver/ |
| MPLAB Extensions for VS Code | VS Code integration; DV inside VS Code; MCC planned (early access June 2024) | Host for a twin launcher/debug configuration; no simulator listed at early access | Microchip June 2024 news release (via eetasia/electronicspecifier) |

## Table: Microchip boards — mikroBUS availability (as documented)
| board | mikroBUS? | evidence |
| PIC32CM JH01 Curiosity Pro (EV81X90A) | No socket listed — 3 Xplained Pro EXT headers + Arduino Uno (mikroBUS only in family boilerplate) | UG DS70005482B features/connectors; Zephyr doc 0 mentions |
| PIC32CM GC00 / SG00 Curiosity Pro | Yes ('mikroBUS Headers') | Zephyr board docs |
| PIC32CK GC01 / SG01 Curiosity Ultra | Yes ('MikroBUS socket') | Zephyr board docs |
| PIC32CX SG41 Curiosity Ultra | Yes ('mikroBUS header connector') | Zephyr board docs (SG61: not mentioned) |
| PIC32CZ CA80 / CA90 Curiosity Ultra | Yes ('MikroBus Socket') | Zephyr board docs |
| SAM E54 Curiosity Ultra | Yes ('mikroBUS interfaces') | Zephyr board docs |
| SAMA5D27-SOM1-EK1 / SAMA7D65 Curiosity | Yes | Zephyr board docs; only sama7g54_ek DTS defines the mikro-bus nexus |
| Xplained Pro boards (SAM C21N/D21/E54/L21 ...) | No — use ATMBUSADAPTER-XPRO on an EXT header | Zephyr docs 0 mentions; Microchip ATMBUSADAPTER-XPRO listing |
| Curiosity Nano (e.g. PIC32CM JH01/PL10) | No — use AC164162 Curiosity Nano Base (3 sockets) | AC164162 listings; Zephyr docs 0 mentions |
| Curiosity / Curiosity HPC / MPLAB Xpress / Explorer / PICDEM Lab (8/16-bit) | Yes (per 2017 press release) | ir.microchip.com press release 475 (2017) |

## Code: ATDF per-instance SERCOM0 pad signals (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:900-915 (PIC32CM-JH_DFP 1.3.221))
```xml
          <instance name="SERCOM0">
            <register-group name="SERCOM0" name-in-module="SERCOM" address-space="base" offset="0x42000400"/>
            <signals>
              <signal group="PAD" index="0" function="D" pad="PA04"/>
              <signal group="PAD" index="0" function="C" pad="PA08"/>
              <signal group="PAD" index="0" function="C" pad="PB24"/>
              <signal group="PAD" index="1" function="D" pad="PA05"/>
              <signal group="PAD" index="1" function="C" pad="PA09"/>
              <signal group="PAD" index="1" function="C" pad="PB25"/>
              <signal group="PAD" index="2" function="D" pad="PA06"/>
              <signal group="PAD" index="2" function="C" pad="PC24"/>
              <signal group="PAD" index="2" function="C" pad="PA10"/>
              <signal group="PAD" index="3" function="D" pad="PA07"/>
              <signal group="PAD" index="3" function="C" pad="PA11"/>
              <signal group="PAD" index="3" function="C" pad="PC25"/>
            </signals>
```

## Code: ATDF PORT PMUX/PINCFG registers (runtime mux state the twin must decode) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5425-5434)
```xml
        <register name="PMUX" offset="0x30" rw="RW" size="1" count="16" initval="0x00" caption="Peripheral Multiplexing">
          <bitfield name="PMUXE" caption="Peripheral Multiplexing for Even-Numbered Pin" mask="0xF" values="PORT_PMUX__PMUXE"/>
          <bitfield name="PMUXO" caption="Peripheral Multiplexing for Odd-Numbered Pin" mask="0xF0" values="PORT_PMUX__PMUXO"/>
        </register>
        <register name="PINCFG" offset="0x40" rw="RW" size="1" count="32" initval="0x00" caption="Pin Configuration">
          <bitfield name="PMUXEN" caption="Peripheral Multiplexer Enable" mask="0x1"/>
          <bitfield name="INEN" caption="Input Enable" mask="0x2"/>
          <bitfield name="PULLEN" caption="Pull Enable" mask="0x4"/>
          <bitfield name="DRVSTR" caption="Output Driver Strength Selection" mask="0x40"/>
        </register>
```

## Code: ATDF SERCOM SPI DOPO captions (PAD-role mapping lives in caption text) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6337-6342)
```xml
      <value-group name="SERCOM_SPIM_CTRLA__DOPO">
        <value name="0x0" caption="PAD[0]=DO, PAD[1]=SCK, PAD[2]=SlaveSS" value="0x0"/>
        <value name="0x1" caption="PAD[2]=DO, PAD[3]=SCK, PAD[1]=SlaveSS" value="0x1"/>
        <value name="0x2" caption="PAD[3]=DO, PAD[1]=SCK, PAD[2]=SlaveSS" value="0x2"/>
        <value name="0x3" caption="PAD[0]=DO, PAD[3]=SCK, PAD[1]=SlaveSS" value="0x3"/>
      </value-group>
```

## Code: EDC .PIC PinList (MPLAB X device DB equivalent of ATDF signals) (PIC32CM-JH01/edc/PIC32CM5164JH01100.PIC:25236-25245)
```xml
  <edc:PinList edc:ppsflavor="atmel">
    <edc:Pin>
      <edc:VirtualPin edc:name="PA00" edc:ppsfunction="default" ltx:memberofperipheral="PORT"/>
      <edc:VirtualPin edc:name="EXTINT0" edc:ppsfunction="A" ltx:memberofperipheral="EIC"/>
      <edc:VirtualPin edc:name="EXTWAKE0" edc:ppsfunction="A" ltx:memberofperipheral="RSTC"/>
      <edc:VirtualPin edc:name="PAD0" edc:ppsfunction="D" ltx:memberofperipheral="SERCOM1"/>
      <edc:VirtualPin edc:name="WO0" edc:ppsfunction="E" ltx:memberofperipheral="TCC2"/>
      <edc:VirtualPin edc:name="WO0" edc:ppsfunction="F" ltx:memberofperipheral="TC7"/>
      <edc:VirtualPin edc:name="CMP2" edc:ppsfunction="H" ltx:memberofperipheral="AC"/>
      <edc:VirtualPin edc:name="XIN32" edc:ppsfunction="XIN32" ltx:memberofperipheral="OSC32KCTRL"/>
```

## Code: MCC Harmony generated pin table (selected lines 1, 20, 45, 46) (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01 apps/sercom/i2c/master/i2c_eeprom/firmware/src/config/pic32cm_jh01_curiosity_pro/pin_configurations.csv)
```csv
Pin Number,Pin ID,Custom Name,Function,Mode, Direction,Latch,Pull Up,Pull Down,Drive Strength
21,PC05,LED,GPIO,Digital,Out,High,,,NORMAL
52,PA16,,SERCOM1_PAD0,Digital,High Impedance,n/a,No,No,NORMAL
53,PA17,,SERCOM1_PAD1,Digital,High Impedance,n/a,No,No,NORMAL
```

## Code: MCC Harmony component YAML: pin 52 function and PMUX letter (https://github.com/Microchip-MPLAB-Harmony/csp_apps_pic32cm_jh00_jh01 apps/sercom/i2c/master/i2c_eeprom/firmware/pic32cm_jh01_curiosity_pro.X/i2c_eeprom_pic32cm_jh01_curiosity_pro_pic32cm_jh01_curiosity_pro/components/core.yml:457-486)
```yaml
    PIN_52_FUNCTION_TYPE:
      attributes:
        id: PIN_52_FUNCTION_TYPE
      children:
      - children:
        - attributes:
            value: SERCOM1_PAD0
          type: User
        type: Values
      type: String
    PIN_52_MODE:
      attributes:
        id: PIN_52_MODE
      children:
      - children:
        - attributes:
            value: ''
          type: User
        type: Values
      type: String
    PIN_52_PERIPHERAL_FUNCTION:
      attributes:
        id: PIN_52_PERIPHERAL_FUNCTION
      children:
      - children:
        - attributes:
            value: C
          type: User
        type: Values
      type: String
```

## Code: Harmony 3 BSP for PIC32CM JH01 Curiosity Pro (LED/switch only) (https://github.com/Microchip-MPLAB-Harmony/bsp boards/pic32cm_jh01_curiosity_pro/config/bsp.py:26-37)
```python
    # LED 0: PC05
    Database.setSymbolValue("core", "PIN_21_FUNCTION_TYPE", "LED_AL")
    Database.setSymbolValue("core", "PIN_21_FUNCTION_NAME", "LED")
    Database.setSymbolValue("core", "PIN_21_LAT", "High")
    Database.setSymbolValue("core", "PIN_21_DIR", "Out")

    # SW0: PB19
    Database.setSymbolValue("core", "PIN_67_FUNCTION_TYPE", "SWITCH_AL")
    Database.setSymbolValue("core", "PIN_67_FUNCTION_NAME", "SWITCH")
    Database.setSymbolValue("core", "PIN_67_PULLEN", "True")
    Database.setSymbolValue("core", "PIN_67_LAT", "High")
    Database.setSymbolValue("core", "PIN_67_INEN", "True")
```

## Code: MCC Harmony AT24 driver component records part address/size/PLIB (https://github.com/Microchip-MPLAB-Harmony/core driver/i2c_eeprom/at24/config/drv_at24.py:115-121)
```python
    at24EEPROMAddress = at24Component.createHexSymbol("I2C_EEPROM_ADDDRESS", None)
    at24EEPROMAddress.setLabel("EEPROM Address")
    at24EEPROMAddress.setHelp(drv_at24_mcc_helpkeyword)
    at24EEPROMAddress.setVisible(True)
    at24EEPROMAddress.setDefaultValue(0x57)
    at24EEPROMAddress.setMin(0x0)
    at24EEPROMAddress.setMax(0x7f)
```

## Code: Zephyr pinctrl for PIC32CM JH01 Curiosity Pro (macros autogenerated from ATDF) (https://github.com/zephyrproject-rtos/zephyr boards/microchip/pic32c/pic32cm_jh01_cpro/pic32cm_jh01_cpro-pinctrl.dtsi:9-22)
```dts
&pinctrl {
	sercom4_uart_default: sercom4_uart_default {
		group1 {
			pinmux = <PB10D_SERCOM4_PAD2>,
				 <PB11D_SERCOM4_PAD3>;
		};
	};

	sercom3_i2c_default: sercom3_i2c_default {
		group1 {
			pinmux = <PA22C_SERCOM3_PAD0>,  /* SDA */
				 <PA23C_SERCOM3_PAD1>;  /* SCL */
		};
	};
```

## Code: Zephyr Click shield: off-board Microchip part described relative to a mikroBUS socket (https://github.com/zephyrproject-rtos/zephyr boards/shields/mikroe_eth_click/mikroe_eth_click.overlay:5-17)
```dts
&mikrobus_spi {
	status = "okay";

	eth_click_mikroe_eth_click: eth_click@0 {
		compatible = "microchip,enc28j60";
		reg = <0x0>;
		full-duplex;
		local-mac-address = [00 00 00 01 02 03];
		/* Errata B7/1 specifies min 8Mhz, 20MHz max according to RM */
		spi-max-frequency = <10000000>;
		int-gpios = <&mikrobus_header 7 GPIO_ACTIVE_LOW>;	/* INT */
	};
};
```

## Code: QEMU board file: wiring a Microchip SST25 SPI flash and its GPIO chip-select requires C (hw/arm/sabrelite.c:74-101 (QEMU 11.1.50, 602e4f2da1))
```c
        /*
         * TODO: Ideally we would expose the chip select and spi bus on the
         * SoC object using alias properties; then we would not need to
         * directly access the underlying spi device object.
         */
        /* Add the sst25vf016b NOR FLASH memory to first SPI */
        Object *spi_dev;

        spi_dev = object_resolve_path_component(OBJECT(&s->soc), "spi1");
        if (spi_dev) {
            SSIBus *spi_bus;

            spi_bus = (SSIBus *)qdev_get_child_bus(DEVICE(spi_dev), "spi");
            if (spi_bus) {
                DeviceState *flash_dev;
                qemu_irq cs_line;
                DriveInfo *dinfo = drive_get(IF_MTD, 0, 0);

                flash_dev = qdev_new("sst25vf016b");
                if (dinfo) {
                    qdev_prop_set_drive_err(flash_dev, "drive",
                                            blk_by_legacy_dinfo(dinfo),
                                            &error_fatal);
                }
                qdev_realize_and_unref(flash_dev, BUS(spi_bus), &error_fatal);

                cs_line = qdev_get_gpio_in_named(flash_dev, SSI_GPIO_CS, 0);
                qdev_connect_gpio_out(DEVICE(&s->soc.gpio[2]), 19, cs_line);
```

## Code: QEMU: attach an I2C part from the command line and change its state via QMP (upstream test) (tests/functional/arm/test_aspeed_ast2500_buildroot.py:23-36)
```python
        self.vm.add_args('-device',
                         'tmp105,bus=aspeed.i2c.bus.3,address=0x4d,id=tmp-test')
        self.do_test_arm_aspeed_buildroot_start(image_path, '0x0',
                                                'ast2500-evb login:')

        exec_command_and_wait_for_pattern(self,
             'echo lm75 0x4d > /sys/class/i2c-dev/i2c-3/device/new_device',
             'i2c i2c-3: new_device: Instantiated device lm75 at 0x4d')
        exec_command_and_wait_for_pattern(self,
                             'cat /sys/class/hwmon/hwmon1/temp1_input', '0')
        self.vm.cmd('qom-set', path='/machine/peripheral/tmp-test',
                    property='temperature', value=18000)
        exec_command_and_wait_for_pattern(self,
                             'cat /sys/class/hwmon/hwmon1/temp1_input', '18000')
```

## Code: Renode board description: GPIO-level parts on a Microchip SAM4S Xplained (https://github.com/renode/renode platforms/boards/sam4s_xplained.repl:1-9)
```text
using "platforms/cpus/sam4s16c.repl"

LED: Miscellaneous.LED @ pioC 10

pioC:
    10 -> LED@0

Button: Miscellaneous.Button @ pioA 5
    -> pioA@5
```

## Code: dts2repl overlay for PIC32CM: clock-status spoofing (shallow fidelity) (https://github.com/antmicro/dts2repl dts2repl/overlay/microchip,pic32cm.repl:1-11)
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
```

## Code: ClickID manifest (THERMO 8 Click = MCP9808; note Linux-centric driver string), selected lines 1-5 and 38-44 (https://github.com/MikroElektronika/click_id clicks/THERMO-8/manifest/THERMO-8-CLICK.mnfs)
```ini
;
; THERMO 8 CLICK
; https://www.mikroe.com/thermo-8-click
; CONFIG_SENSORS_LM75
;
[device-descriptor 1]
driver-string-id = 3
protocol = 0x3
reg = 0x18

[string-descriptor 3]
string = mcp980x
```

## Open questions
- Can MCC (Harmony/Melody) export pin and driver configuration headlessly (CLI/API) beyond the MCP tools, and do the MCP tools expose the PIN_n_* and driver symbols (e.g. drv_at24 I2C_EEPROM_ADDDRESS)? The mcp__mplab-mcc__* tools exist in this session but were not exercised.
- What share of customer projects configure attached parts through MCC driver components rather than raw PLIB calls? This sets how much of a board description MCC can auto-fill; no data found.
- Which peripherals does MPLAB SIM model today for PIC32CM JH (SimPeripheralSupport by device)? onlinedocs and microchip.com returned 503/403 here; the user's .emu mapping knowledge or the mplab-docs MCP server (currently disconnected) should settle this.
- Does MPLAB Data Visualizer accept TCP/socket input as well as COM/DGI? Unverified.
- What format does the Xplained Pro extension ID chip (read by EDBG/MPLAB X) use, and could the twin read or emit it to auto-identify virtual extensions?
- Do Microchip dev-board design-file packages include native CAD netlists (Altium/KiCad) that could seed a 'twin of the Microchip kit' automatically?
- What is Microchip's internal policy on publicly modeling ATECC608/TA100 command behavior (NDA datasheet content, export control), and does that rule out a faithful twin of the JH01 Curiosity Pro?
- Is the Linux mikroBUS/ClickID driver upstream, and are ClickID manifests maintained well enough to drive model selection? THERMO 8 (MCP9808) uses the LM75-family 'mcp980x' driver string.
- How deep does the Renode Zephyr dashboard's microchip,pic32cm simulation go (pass/fail per sample)? The dashboard page is rendered in JavaScript and could not be read.
- Does QEMU's qapi/ (not checked out) contain any QMP command for GPIO wiring that would remove the need for a C-coded board loader?
- Customer demand for board-level vs. chip-level simulation is unmeasured; no verified survey quantifies hardware-availability blocking for MCU teams, so field interviews or a Microchip customer survey would be needed.
