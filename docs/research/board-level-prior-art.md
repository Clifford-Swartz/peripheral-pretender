# board-level-prior-art

## Summary
Every tool I checked that lets users simulate their own boards uses the same layered recipe: a reusable chip/SoC model, a declarative board file, an extension API for parts the vendor never modeled, and a scripted test/CI runner. The board file instantiates off-chip parts and wires them by bus address or by pin. Examples: Renode (.repl + runtime-compiled C#/IronPython/Verilator + Robot Framework), Wokwi (diagram.json + WASM Custom Chips API + YAML scenarios/wokwi-cli) and upstream QEMU (C machine + `-device ...,bus=...,address=...` + QMP qom-set + functional tests). Board-level is a thin composition layer on top of chip-level fidelity, not an alternative to it. Renode's auto-generated PIC32CM JH01 Curiosity Nano platform passes UART-based Zephyr samples, but it models only the Cortex-M0+ core, NVIC and SERCOM1. PORT, EIC, TC/TCC, ADC, DAC, DMAC, RTC, WDT and the clocks are stubs. Clock registers labelled 'UNK' are faked with constants (the ATDF identifies them as OSCCTRL/OSC32KCTRL status and DPLL registers). As a result, 0 of 29 Microchip SAM/PIC32C boards on the Renode Zephyr dashboard pass blinky, while 21 pass hello_world. Breadth scales by generating board files from existing hardware descriptions: dts2repl maps devicetree compatible strings to models, giving 1,170 boards / 1,711 targets on the October 2026 dashboard (1,063 pass hello_world, only 425 pass blinky). Depth scales only by hand-written peripheral models, and Microchip's ATDF (module IDs, instances, IRQs, registers) is a richer source than DTS for such a generator. The long tail of customer parts is handled by cheap open extension APIs, not vendor-complete libraries: Renode has about 50 sensor models, Wokwi lists 66 parts, and QEMU hw/sensor has 11 BMC-oriented models. Upstream QEMU also cannot user-instantiate on-chip sysbus devices; only bus-attached parts are composable from the CLI. Multi-MCU/multi-board timing varies by tool. Renode uses an in-process global-quantum barrier with deterministic hubs (its RAMN demo runs 4 CAN ECUs with a 10 us quantum). SimBricks uses per-link conservative sync that exploits link latency, but found QEMU's synchronous MMIO broke full determinism. SIL Kit offers virtual-time sync, but its QEMU adapter is plain sockets. Wokwi does not support more than one MCU per project. Automotive formalizes the board-level case as the prostep ivip Level 4 V-ECU: the target binary on an ISS plus 'the model for the corresponding ECU hardware'. It is sold commercially as Synopsys VDKs co-developed with Infineon/NXP/ST/Renesas, VLAB VDMs that 'simulate complete boards (... custom boards ...)', and dSPACE VEOS level-4 integration, and none of these catalogs list Microchip MCUs. Vendor marketing uses 'virtual prototype/platform', and 'digital twin' increasingly means a product- or board-level model (Infineon 2025, Synopsys, Antmicro's STM32H7 board). Several MCU vendors now co-fund free models: Silicon Labs and ADI with Renode, Nordic's bsim models, and Espressif's QEMU fork. Microchip itself already ships Renode in SoftConsole for PolarFire SoC/Mi-V. Safety use of virtual-platform results requires ISO 26262-8 Clause 11 tool-confidence classification and qualification by the user.

## Findings

1. (high) [LOAD-BEARING] [Renode board description format (.repl)] Renode composes boards declaratively. A board .repl includes an SoC .repl with `using`, then adds off-chip parts by registering them on a bus or pin (e.g. `lsm9ds1_imu: Sensors.LSM9DS1_IMU @ twi0 0x6b`, `LED: Miscellaneous.LED @ pioC 10`) and wires GPIO/IRQs with `->`. A customer board is therefore a small text file layered on a vendor SoC file.
   detail: Later entries override earlier ones across `using` chains, so a board file can patch SoC defaults. Registration points can be bus addresses, I2C addresses, chip-selects or GPIO pin numbers. init/preinit/reset attributes run Monitor commands. sam4s_xplained.repl is 9 lines (SoC include, LED on pioC 10, button on pioA 5). arduino_nano_33_ble.repl adds an ArduCAM on SPI+I2C, LSM9DS1 IMU and magnetometer on twi0 at 0x6b and 0x1e, and three LEDs on gpio0. The attach level is bus-transaction (I2C address) plus boolean GPIO, not electrical nets.
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/platform_description_format.md#L65-L83 (entry = `variableName: TypeName registrationInfo` + attributes)
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/platform_description_format.md#L191-L235 (registration points, aliases, `@ none`)
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/platform_description_format.md#L313-L393 (interrupt attributes `-> destination@number`, `|` fan-out, `#index` local IRQs, `-> none`)
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/platform_description_format.md#L527-L577 (`using` dependencies; later entries override earlier ones)
   - ev: https://github.com/renode/renode/blob/master/platforms/boards/sam4s_xplained.repl#L1-L9
   - ev: https://github.com/renode/renode/blob/master/platforms/boards/arduino_nano_33_ble.repl#L1-L20

2. (high) [LOAD-BEARING] [Renode: adding custom parts without rebuilding] Renode users add parts without rebuilding Renode in three ways. (1) C# models compiled at load time by an embedded Roslyn compiler, triggered from a repl `preinit` with `include @MyPeripheral.cs`. (2) IronPython-2 `Python.PythonPeripheral` scripts that receive a `request` object. (3) HDL co-simulation: DPI over TCP (Verilator/Questa/VCS; AHB, APB3, AXI4, AXI4-Lite) or a Verilator shared library (APB3, AXI4, AXI4-Lite, Wishbone, CFU).
   detail: Other access paths are the external control C API and pyrenode3 (Python 3 CLR bindings). The dts2repl-generated platforms use Python stubs such as scripts/pydev/repeater.py and flipflop.py as zero-effort placeholders. This tiered path (stub, then script, then compiled model, then RTL) is what keeps the cost of adding a customer part low.
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/platform_description_format.md#L421-L469 (preinit: 'compiling a peripheral model from C# source'; `include @MyPeripheral.cs`)
   - ev: https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Main/Utilities/AdHocCompiler.cs#L14-L52 (Microsoft.CodeAnalysis.CSharp, LanguageVersion.CSharp12, OutputKind.DynamicallyLinkedLibrary)
   - ev: https://github.com/renode/renode-docs/blob/master/source/basic/using-python.md#L6-L11 (IronPython 'is Python 2 running inside Renode') and #L106-L118 (Python peripherals in .repl)
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/co-simulating-with-an-hdl-simulator.md#L3-L102
   - ev: https://github.com/renode/renode-docs/blob/master/source/tutorials/co-simulating-custom-hdl.md#L224-L244 (HDL clock driven in virtual time; limitBuffer default 1,000,000 ticks)
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/writing-peripherals.md#L155-L166 (peakrdl-renode generates partial C# register classes from SystemRDL)
   - ev: https://github.com/renode/renode-docs/blob/master/source/api-description/external-control.md (External control client API)

3. (high) [LOAD-BEARING] [Renode multi-node / multi-board time synchronization] Renode runs all machines of an emulation in one process under one master time source that grants a quantum. Inter-node traffic (UART/CAN hubs, Ethernet switch, wireless medium, GPIOConnector) is exchanged only in the synchronization phase, which guarantees that two nodes' virtual times never differ by more than the quantum. Users trade accuracy for speed with `emulation SetGlobalQuantum`; the RAMN 4-ECU CAN demo uses 10 us.
   detail: This is a barrier-synchronized (bulk-synchronous) design inside a single process, not distributed PDES. Multiple time domains exist but 'there is no synchronization across different time domains'. AdvanceImmediately runs faster than real time; otherwise Renode throttles to real time. The RAMN script comment reads 'Syncing more freqently improves performance in this senario' (sic).
   - ev: https://github.com/renode/renode-docs/blob/master/source/advanced/time_framework.md#L22-L70 ('It is guaranteed that the difference of virtual time perceived of two slaves is never higher than a quantum of its closest common source')
   - ev: https://github.com/renode/renode-docs/blob/master/source/networking/machine-to-machine-connections.md#L70-L73 (hub bytes 'buffered and delivered to the receiver at set times' to ensure determinism) and #L125-L141 (GPIOConnector between two machines)
   - ev: https://github.com/renode/renode-docs/blob/master/source/networking/wireless.md#L99 (`emulation SetSeed` keeps lossy radio deterministic)
   - ev: https://github.com/renode/renode-infrastructure/blob/master/src/Emulator/Extensions/UserInterface/MonitorTimeExtensions.cs#L43-L61 (SetGlobalQuantum, SetGlobalAdvanceImmediately, SetGlobalSerialExecution)
   - ev: https://github.com/renode/renode/blob/master/scripts/multi-node/ramn.resc#L1-L23
   - ev: https://github.com/renode/renode/blob/master/scripts/multi-node/sam_e70.resc#L1-L18 (two Microchip SAM E70 nodes on an emulated Ethernet switch, Zephyr gPTP)

4. (high) [Renode CI / testing integration] Renode's CI path is `renode-test` driving Robot Framework keywords (UART/LED/network testers, dummy I2C/SPI targets), plus an official GitHub Action. A production example is Google's ChromeOS EC fingerprint-MCU firmware (STM32F412/H743), tested in CI on Renode.
   detail: Renode 1.17.0 was released 2026-09-07. The repo snapshot of 2026-10-09 contains 296 .robot tests and 145 .resc scripts.
   - ev: https://github.com/renode/renode-docs/blob/master/source/introduction/testing.md#L13-L27
   - ev: https://github.com/renode/renode-docs/blob/master/source/basic/renode-testing-api.md#L180-L200
   - ev: https://github.com/antmicro/renode-test-action (Apache-2.0)
   - ev: https://opensource.googleblog.com/2023/08/chromeos-ec-testing-suite-renode-for-consumer-products.html (2023-08-03: 'automated testing of embedded controllers in CI at scale, in a deterministic manner')

5. (high) [LOAD-BEARING] [Scaling breadth: dts2repl and the Renode Zephyr dashboard] Antmicro scales board coverage by generating .repl files from Zephyr devicetrees. dts2repl maps devicetree 'compatible' strings to Renode models in models.json, with 323 per-board/SoC overlay files on top. On the live dashboard (Zephyr and Renode commits of 2026-10-02), 1,170 boards / 1,711 board-SoC-variant targets are generated. 1,063 targets pass hello_world but only 425 pass blinky. Antmicro's 2026-08-12 post reported '926/1513 platforms passing the Zephyr Hello World demo'.
   detail: Compatibles without a model fall back to Tags or Python stubs (e.g. 'atmel,sama5d2-tcb' -> PythonPeripheral flipflop.py). Microchip mappings include atmel,sam0-uart -> SAMD20_UART/SAMD5_UART, atmel,sam0-i2c -> SAMD21_I2C, microchip,mpfs-gpio -> MPFS_GPIO and microchip,enc28j60 -> ENC28J60. The current dashboard config lists 13 samples (the blog says 14). The counting units differ: the blog counts 'platforms', the JSON counts board/SoC/variant targets. The pattern is that console works far more often than GPIO behaviour, because generation gives breadth, not depth.
   - ev: https://github.com/antmicro/dts2repl/blob/main/README.md ('a tool for converting device tree sources into Renode's .repl files'; overlays matched by root/soc compatible)
   - ev: https://github.com/antmicro/dts2repl/blob/main/dts2repl/models.json#L124-L128 and #L227 ('microchip,sercom-g1-uart' -> UART.SAMD5_UART)
   - ev: gh api repos/antmicro/dts2repl tree: 323 files under dts2repl/overlay/ (2026-10-09)
   - ev: https://antmicro.com/blog/2026/08/recent-improvements-to-renode-zephyr-and-u-boot-dashboards ('At the time of writing, we have 926/1513 platforms passing the Zephyr Hello World demo'; 'There are 14 samples')
   - ev: https://zephyr-dashboard.renode.io/zephyr_sim/2428837a26050e11d0d541432a5fa783666c0c8e/8f7a74f51eecc9f5691cc3c42ce6d0deab07b84e/result-aggregated.json (1,170 boards; 1,711 targets; hello_world PASSED 1,063; blinky PASSED 425; 837 boards with at least one hello_world pass)
   - ev: https://zephyrproject.org/improving-data-sources-and-tests-in-the-renode-zephyr-dashboard-and-renodepedia/ (300 -> 470+ passing targets, 2024)

6. (high) [LOAD-BEARING] [Depth gap: Renode on the PIC32CM JH family (the user's part)] Renode already runs PIC32CM JH01 Curiosity Nano/Pro firmware, but only at console level. hello_world, shell_module, philosophers, MicroPython and TFLM pass, yet the generated platform models only the Cortex-M0+ core, NVIC, DWT, memories and SERCOM1 (as SAMD5_UART at 0x42000800, IRQ 10). PORT, EIC, TC0-7, TCC0-2, ADC0/1, AC, DAC, DMAC, RTC, WDT and the clocks are Tags or Python repeaters. Blinky, which needs `sysbus.porta.led0`, therefore cannot pass. Across 29 Microchip SAM/PIC32C boards on the dashboard, 21 pass hello_world and 0 pass blinky, whereas Microchip RISC-V boards with a real MPFS_GPIO model (mpfs_icicle, pic64gx_curiosity_kit) do pass blinky.
   detail: The overlay labels the registers it fakes as UNK1 (0x4000100C = OSCCTRL.STATUS), UNK2 (0x40001030 = OSCCTRL.DPLLSTATUS), UNK3 (0x4000140C = OSC32KCTRL.STATUS) and 'OSCCTRL_STATUS' (0x40001414, actually OSC32KCTRL.XOSC32K, a control register). The repeater stubs at 0x4000101C/20/24 are DPLLCTRLA, DPLLRATIO and DPLLCTRLB. A third-party, devicetree-only generator lacks Microchip register semantics, and that is exactly the gap Microchip's ATDF data (plus its MPLAB SIM peripheral knowledge) would close. Board-level behaviours such as LEDs, buttons, PWM and I2C sensors are all gated by chip-level PORT, EIC, TC/TCC and SERCOM-I2C/SPI models.
   - ev: https://zephyr-dashboard.renode.io/zephyr_sim/2428837a26050e11d0d541432a5fa783666c0c8e/8f7a74f51eecc9f5691cc3c42ce6d0deab07b84e/pic32cm_jh01_cnano/hello_world/hello_world.repl (L1-L44 Tag stubs; L95-L96 sercom1; L103-L110 'microchip,pic32cm overlay' UNK constants); local copy C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/work-board-level-prior-art/dl/zdash/jh01_cnano_hello.txt
   - ev: https://zephyr-dashboard.renode.io/zephyr_sim/2428837a26050e11d0d541432a5fa783666c0c8e/8f7a74f51eecc9f5691cc3c42ce6d0deab07b84e/pic32cm_jh01_cnano/blinky/blinky.robot (L2 `${LED} sysbus.porta.led0`; L12 `Assert LED Is Blinking`); blinky.repl is byte-identical to hello_world.repl
   - ev: https://github.com/antmicro/dts2repl/blob/main/dts2repl/overlay/microchip,pic32cm.repl
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:597 (OSCCTRL @0x40001000), :4860 (STATUS offset 0xC 'Power and Clocks Status'), :4901/:4906/:4910 (DPLLCTRLA/DPLLRATIO/DPLLCTRLB @0x1C/0x20/0x24), :4927 (DPLLSTATUS @0x30)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:584 (OSC32KCTRL @0x40001400), :4761 (STATUS @0xC), :4770 (XOSC32K control @0x14)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:927 (SERCOM1 @0x42000800), :1443 (interrupt index 10), :655 (PORT @0x41000000); module IDs :899 SERCOM U2201 v3.1.1, :653 PORT U2210 v3.1.0
   - ev: https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/microchip/pic32c/pic32cm_jh01_cnano (upstream Zephyr board, last change 2026-09-23)

7. (high) [LOAD-BEARING] [Microchip parts already modeled in Renode] The Renode tree contains hand-written models and platforms for several Microchip parts. SoC platforms: SAM E70, SAM4S (8B/16C) with a SAM4S Xplained board file, ATSAMD21J17D, ATSAMD51G19A, PolarFire SoC/Icicle Kit and Mi-V. Peripheral models: SAMD21 GPIO/I2C/RTC/Timer, SAMD20 and SAMD5 UART, SAM_USART/SAM_TC/SAM_SPI/SAM_PDC, SAM4S_* and MPFS_*. Microchip off-chip parts: PAC1934 and ENC28J60. These sit within a library of 876 peripheral .cs files, 150 CPU/SoC .repl and 81 board .repl.
   detail: The SAMD21/SERCOM-family models (SAMD21_I2C, SAMD5_UART, SAMD21_GPIO, SAMD21_Timer) are the nearest existing starting point for the PIC32CM JH (SERCOM U2201, PORT U2210, TC U2249). I did not test whether their register behaviour matches the JH revisions.
   - ev: https://github.com/renode/renode/tree/master/platforms/cpus (sam_e70.repl, sam4s.repl, sam4s8b.repl, sam4s16c.repl, atsamd21j17d-aft.repl, atsamd51g19a.repl, polarfire-soc.repl, miv.repl, miv_rv32.repl)
   - ev: https://github.com/renode/renode/tree/master/platforms/boards (sam4s_xplained.repl, mpfs-icicle-kit.repl, miv-board.repl, beaglev-fire.repl)
   - ev: https://github.com/renode/renode-infrastructure/tree/master/src/Emulator/Peripherals/Peripherals (GPIOPort/SAMD21_GPIO.cs, I2C/SAMD21_I2C.cs, Timers/SAMD21_Timer.cs, UART/SAMD5_UART.cs, Sensors/PAC1934.cs, Network/ENC28J60.cs; 876 .cs files total, snapshot 2026-10-09 commit 515cc63eea)
   - ev: https://github.com/renode/renode/blob/master/platforms/cpus/atsamd21j17d-aft.repl (Cortex-M0+ SAMD21: rtc, gpio_a/b, 3x SAMD5_UART, 2x SAMD21_Timer; remaining blocks are Tags)
   - ev: https://github.com/renode/renode-docs/blob/master/source/introduction/supported-boards.md#L62-L94 (SAM E70 Xplained, PolarFire SoC HW Dev Platform) and #L174 (Icicle Kit)

8. (medium) [LOAD-BEARING] [Microchip precedent: Renode shipped in SoftConsole] Microchip already distributes a Renode-based virtual platform. Antmicro states Renode 'has been the default virtual development option in Microchip's SoftConsole IDE' for PolarFire SoC and that 'Renode now ships as an integrated part of SoftConsole'. PolarFire SoC was 'originally made available pre-silicon by Microchip through Antmicro's Renode', with FPGA-fabric co-simulation via DPI/Verilator.
   detail: Primary Microchip pages could not be fetched (HTTP 403 to WebFetch and curl), so this rests on Antmicro's statements and search snippets of microchip.com. This should be confirmed internally, including which Renode version SoftConsole bundles.
   - ev: https://antmicro.com/blog/2024/07/open-source-som-with-polarfire-soc (2024-07-08)
   - ev: https://antmicro.com/blog/2022/12/microchip-riscv-support-in-renode (2022-12-29: 'Renode now ships as an integrated part of SoftConsole'; 'Many of Microchip's Cortex-M-class microcontrollers, like the SAM E70, can also be simulated')
   - ev: https://www.microchip.com/en-us/products/fpgas-and-plds/system-on-chip-fpgas/polarfire-soc-fpgas/get-started (returned HTTP 403 to automated fetch; the search snippet says Renode ships inside SoftConsole)

9. (high) [LOAD-BEARING] [Wokwi board composition model and MCU coverage] Wokwi describes a board as diagram.json: a list of parts (id, type, position, attrs) plus pin-to-pin wires such as `["led1:A", "uno:13", "green", []]`. That is net/pin-level wiring rather than bus-address registration. Supported MCUs are AVR (ATmega328P, ATmega2560, ATtiny85), the ESP32 family, STM32C031/L031/F103 and RP2040. Wokwi explicitly does not support multiple MCUs in one project: 'Connecting them together with wires is not possible at this time.'
   detail: The only Microchip parts are the ex-Atmel AVRs, which run on the open-source avr8js engine. There are no SAM, PIC32 or PIC parts. Wokwi docs snapshot: commit 1b91bec815, 2026-10-09.
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/diagram-format.md#L33-L62 (parts) and #L106-L121 (connections)
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/getting-started/supported-hardware.md#L11-L18 (MCU table; ESP32-P4 beta, C5/S31 alpha)
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/faq.md#L43-L51 ('How do I use multiple microcontrollers in a single project? Unfortunately this is currently unsupported.')
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/vscode/project-config.md#L27-L52 (wokwi.toml firmware/elf; .hex/.elf for AVR)

10. (high) [LOAD-BEARING] [Wokwi Custom Chips API (template for a customer-part SDK)] Wokwi's Custom Chips API (beta) is the closest prior art for an SDK that customers use to model their own parts. Chips are written in C (or Rust/AssemblyScript; Verilog experimental) and compiled to WebAssembly. They are declared with `<name>.chip.json` (pins, UI controls) and `[[chip]]` in wokwi.toml. The API covers pins (init/mode/read/write/watch), analog, virtual-time timers (`get_sim_nanos`, `timer_start` in us, `timer_start_ns`), UART, I2C target (7-bit address with connect/read/write/disconnect callbacks), SPI target (no CS: the chip handles it with pin_watch), attributes and framebuffer. The same WASM runs in the browser, VS Code and CI.
   detail: Init-only registration ('can only be called from chip_init()') keeps the model sandboxed and deterministic. Parts attach at pin level, and the simulator handles I2C/SPI/UART bit-level protocol decoding on those pins. The WASM sandbox makes untrusted customer models safe to run in a cloud CI.
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/getting-started.md#L10-L20 ('The Chips API is currently in beta'; WASM languages; 'experimental support for writing custom chips in Verilog')
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/i2c.md#L8-L65
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/spi.md#L23 ('The API does not support a CS/SS pin')
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/time.md#L8-L51
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/chip-json.md#L10-L38
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/guides/custom-chips-to-wasm.md#L12-L38

11. (medium) [Wokwi CI, licensing and partnerships] Wokwi is a freemium SaaS. CI runs on Wokwi's cloud through wokwi-cli or `wokwi/wokwi-ci-action@v1`, with YAML automation scenarios (alpha) and an experimental MCP server. Usage is metered per rolling 30 days: Free 50, Hobby 200, Hobby+ 500, Pro 2000 CI minutes, with session caps of 5/5/10/50 min. Pro costs EUR 20/seat/month (annual), and on-prem CI is available by request. avr8js, rp2040js and wokwi-cli are MIT-licensed. I found no formal chip-vendor partnership announcement; Espressif documents Wokwi integration through pytest-embedded-wokwi and Espressif IDE 2.9.0+.
   detail: The absence of a vendor partnership is a negative search result. Wokwi's alpha support for unreleased Espressif chips (ESP32-C5, ESP32-S31) suggests close cooperation, but that is my inference. I did not establish the licence of the ESP32/STM32 engines (not found as open source).
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/wokwi-ci/getting-started.md#L9-L50
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/wokwi-ci/github-actions.md#L8-L23
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/wokwi-ci/automation-scenarios.md (alpha; steps delay, expect-pin, set-control, wait-serial, write-serial, take-screenshot, touch)
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/wokwi-ci/mcp-support.md
   - ev: https://wokwi.com/pricing (Community EUR 0; Hobby EUR 5.6/mo; Hobby+ EUR 8.1/mo; Wokwi Pro EUR 20/seat/mo 'For commercial projects', 2000 monthly CI minutes; annual billing)
   - ev: gh api: wokwi/avr8js MIT, wokwi/rp2040js MIT, wokwi/wokwi-cli MIT
   - ev: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/third-party-tools/wokwi.html

12. (high) [LOAD-BEARING] [Upstream QEMU board-composition limits] In upstream QEMU 11.1.50 a board is a C machine type. On-chip sysbus devices are not user-creatable because 'device_add can't' wire their MMIO/IRQs. The allowlist (`machine_class_allow_dynamic_sysbus_dev`) is used only by virtual/server machines: 27 call sites in virt, pc, microvm, pnv, spapr, e500plat and xen, none in MCU boards. Off-chip devices on 'real' buses can be added from the CLI (`-device tmp105,bus=aspeed.i2c.bus.3,address=0x4d`), and their environment can be driven through QMP `qom-set` (the tmp105 'temperature' property); QEMU's own functional tests do exactly this. There is no upstream declarative or FDT-generic board file and no remote-port co-simulation.
   detail: For a QEMU-based Microchip twin, customer composition would need the SoC model to expose named SERCOM I2C/SPI buses, a GPIO/pin connection mechanism, and part models that are either compiled in or out-of-process. QTest drives devices without running guest code, so it suits model unit tests, not firmware co-simulation. hw/sensor has 11 models, mostly BMC/PMBus parts.
   - ev: hw/core/sysbus.c:300-310
   - ev: hw/core/machine.c:680-683; call sites in hw/arm/virt.c:4167-4172, hw/i386/pc_q35.c, hw/i386/pc_piix.c, hw/riscv/virt.c, hw/loongarch/virt.c, hw/ppc/pnv.c, hw/ppc/spapr.c, hw/ppc/e500plat.c, hw/i386/microvm.c, hw/xen/*
   - ev: tests/functional/arm/test_aspeed_ast2600_buildroot.py:29-44
   - ev: hw/sensor/tmp105.c:409-416
   - ev: docs/system/arm/aspeed.rst:148-157 (SPI flash parts attached to ssi.0/ssi.1 with cs= from the CLI)
   - ev: system/qtest.c:96-111, 229-262, 710, 747 (clock_step/clock_set and irq_intercept/set_irq_in, only when qtest is enabled)
   - ev: grep for 'generic-fdt|fdt_generic|hw-dtb|remote-port' in hw/, include/, system/, docs/ finds no matches (only Xen 'remote_port' fields)
   - ev: hw/riscv/microchip_pfsoc.c:728,755 (microchip-icicle-kit); hw/avr/arduino.c:81-147 (arduino-duemilanove/uno/mega/mega-2560-v3: ATmega168/328P/1280/2560)

13. (medium) [AMD/Xilinx QEMU fork: description-driven machines + SystemC] AMD/Xilinx's QEMU fork builds the machine from a hardware device tree (`-M arm-generic-fdt -hw-dtb <board>.dtb`) and co-simulates SystemC/TLM-2.0 models and Verilated RTL through remote-port sockets with a sync quantum. It is prior art for a description-driven QEMU board, but it is not upstream.
   detail: Verified through search summaries of the AMD/Xilinx docs, not by reading the fork's source. libsystemctlm-soc's last push was reported as 2024-05.
   - ev: https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/821428366/QEMU+Options+and+Commands
   - ev: https://docs.amd.com/r/en-US/ug1702-vitis-accelerated-reference/Zynq-UltraScale-MPSoC-PS-and-PMU-Arguments-for-QEMU
   - ev: https://github.com/Xilinx/libsystemctlm-soc

14. (high) [LOAD-BEARING] [SimBricks-style modular synchronized co-simulation] SimBricks composes separate simulator processes (QEMU, gem5, Verilator RTL, ns-3, OMNeT++) over shared-memory queues. It synchronizes pairwise with a conservative Chandy-Misra-Bryant-style protocol: each link has latency delta>0 and monotonic timestamps; a simulator may advance to T once every peer has sent a message stamped >=T; SYNC messages every <=delta guarantee liveness. The result is deterministic when the components are, and it scaled to 1000 simulated hosts on 26 servers. However, QEMU's synchronous MMIO forced an abort-and-retry workaround, and 'this QEMU version is no longer fully deterministic even with instruction counting'.
   detail: My inference on applicability: per-link lookahead suits links between boards (CAN, Ethernet, UART), where serialization latency is at least microseconds. Inside a board, MCU-to-FPGA memory-mapped buses have near-zero latency and blocking reads, so conservative sync degenerates to lockstep. That argues for simulating one board in a single process (Renode-style) and using SimBricks- or SIL Kit-style links only between boards.
   - ev: https://arxiv.org/abs/2012.14219 (SIGCOMM'22; HTML https://ar5iv.labs.arxiv.org/html/2012.14219 sections 'Message timestamps ensure correctness', 'Ensuring liveness with sync messages', 'Link latency provides synchronization slack')
   - ev: same paper, section 6.2 QEMU: 'The key challenge is modelling MMIO delays, as QEMU's device interface does not model timing and expects accesses to return immediately'
   - ev: same paper, section 7.4 / abstract: 'scales to 1000 simulated hosts'; 40 to 1000 hosts on 1 to 26 c5.metal servers; Table 3 marks QEMU-timing combinations as not fully deterministic
   - ev: https://github.com/simbricks/simbricks (MIT; pushed 2026-10-08)

15. (medium) [Other composition backbones: SIL Kit, Switchboard] Vector/Synopsys SIL Kit (MIT, v5.0.7 docs) connects participants over CAN/CAN FD/CAN XL, LIN, FlexRay, Ethernet, pub/sub and RPC, with optional synchronized virtual time and an FMU importer. Its QEMU adapter, however, just attaches QEMU socket netdev/chardev backends, and no virtual-time coupling is documented for it. Zero ASIC's Switchboard (Apache-2.0) links prebuilt RTL, C++ and Python simulators through shared-memory SPSC queues with latency-insensitive interfaces and no global barriers.
   detail: SIL Kit is the de facto open glue that vECU tools use for buses between ECUs; Vector vVIRTUALtarget is 'Pre-integrated with the SIL Kit'.
   - ev: https://github.com/vectorgrp/sil-kit (MIT)
   - ev: https://vectorgrp.github.io/sil-kit-docs/ ('Participants can opt to use a virtual simulation time that is synchronized with other participants')
   - ev: https://github.com/vectorgrp/sil-kit-adapters-qemu/blob/main/README.md#L96-L113 ('uses the *socket* backend provided by QEMU'; `-netdev socket`, `-chardev socket`)
   - ev: https://github.com/vectorgrp/sil-kit-fmu-importer (MIT)
   - ev: https://arxiv.org/abs/2407.20537 (Switchboard: 'without fine-grained synchronization or global barriers')
   - ev: https://github.com/zeroasiccorp/switchboard/blob/main/README.md#L18-L20

16. (medium) [Plant / environment co-simulation] Plant and environment coupling options are: FMI 3.0 co-simulation with the FMI-LS-BUS layered standard (v1.0 released 2025-07-21 for CAN/CAN FD/CAN XL; v1.3-beta.1 on 2026-06-25 with LIN, Ethernet and FlexRay in beta); SIL Kit's FMU importer; tool-native sensor feeds (Renode Environment medium plus time-aligned RESD streams, QEMU QOM properties via qom-set, Arm AVH VSI Python streams); and SystemC-AMS (IEEE 1666.1-2016) inside SystemC VPs. Academic work wraps SystemC VPs for FMI, but I found no maintained open-source QEMU-to-FMI bridge.
   detail: No Renode-FMI or Renode-Simulink bridge was found. The absence of a QEMU-FMI bridge is a negative search result.
   - ev: https://fmi-standard.org/news/ (2025-07-21 'Release of FMI Layered Standard for Network Communication v1.0 (FMI-LS-BUS)'; 2026-06-25 v1.3-beta.1)
   - ev: https://www.prostep.org/fileadmin/prod-pay-download-8c1d/PSI_11_V4_SmartSE_Part_F_VECU.pdf pp.12-13 (FMI is 'a good choice for representing level 0 and level 1 V-ECUs' but 'not comprehensive enough' for bus behaviour)
   - ev: https://arxiv.org/abs/2507.18339 (RWTH, 'FMI Meets SystemC', 2025: SystemC VP temperature sensor fed via FMI)
   - ev: https://webthesis.biblio.polito.it/36446/ (2025 thesis 'Connection of QEMU with SystemC and FMI/FMU for Software Defined Vehicles' is classified; no code)
   - ev: https://github.com/renode/renode-docs/blob/master/source/basic/sensors.md#L1-L50 (Environment medium for temperature/pressure)
   - ev: https://github.com/renode/renode-docs/blob/master/source/basic/resd.md#L1-L16 (RESD multi-channel time-stamped sensor data)
   - ev: https://github.com/Arm-Examples/Hello_AVH (VSI Python streaming)
   - ev: https://systemc.org/overview/systemc-ams/

17. (high) [LOAD-BEARING] [Authoritative vECU level definitions] The authoritative vECU taxonomy is prostep ivip's SmartSE Recommendation (PSI 11 V4.0, 2025, Part F). Level 0 is the controller model; Level 1 is application production code at signal level; Level 2 adds simulation BSW (bus level); Level 3 adds production BSW (hardware-independent, host-compiled). Level 4 'Target Binary' 'contains production code compiled for the real target ECU' and requires 'An instruction set simulator and the model for the corresponding ECU hardware'. Level 4 is the unmodified-binary-on-emulated-MCU case, and its definition explicitly includes the ECU (board) hardware model.
   detail: Level 4 quote: 'For this V-ECU type the identical build process is used for simulation and for flashing the real ECU. Almost no code changes ... are required.' prostep also notes Level 4 V-ECUs are 'technically very different' from Levels 1-3, so the recommendation's standardization effort focuses on Levels 1-3 rather than Level 4.
   - ev: https://www.prostep.org/fileadmin/prod-pay-download-8c1d/PSI_11_V4_SmartSE_Part_F_VECU.pdf pp.4-5 (Figure 2 'Overview of V-ECU types and their content'; Level 0-4 definitions)
   - ev: same document p.8-9 (Use Case 6: core-load testing of multicore ECUs 'requires a comprehensive instruction set simulator'; Level 4 'used for simulation and for flashing the real ECU')
   - ev: https://www.wardsauto.com/spons/automotive-software-in-the-loop-sil-testing-and-how-do-i-do-it-right/816295 (dSPACE-sponsored, 2026-04-13: 'According to the official prostep white paper, five distinct V-ECU levels are defined')

18. (medium) [LOAD-BEARING] [Commercial MCU-level (Level 4) vECU offerings] Commercial MCU-level offerings are built by EDA or tool vendors and co-developed with MCU vendors. Examples: Synopsys VDKs for Infineon AURIX TC3x/TC4x, NXP S32G/E/K/N, ST Stellar and Renesas RH850/R-Car (via 'Center of Excellence' programs; 'up to 18 months before silicon'); VLAB VDMs (VLAB Works joined Cadence in 2025), sold by TASKING as 'fast level 4 virtual ECUs' that 'simulate complete boards (ECUs, HPC nodes, custom boards, etc.)' for AURIX TC3xx/TC4x; and dSPACE VEOS, which integrates 'level 4 V-ECUs (instruction set simulation)'. Synopsys Silver (Level 1-3, coupled to Virtualizer for binary stacks) and Vector vVIRTUALtarget (Level 1/3, FMU export with FMI 3.0 + ls-bus, SIL Kit) cover the host-compiled levels. No Microchip MCU appeared in any of these catalogs.
   detail: From search summaries only (lower confidence): Siemens PAVE360 targets SoC-level digital twins (Arm Zena CSS) rather than MCUs; ETAS ISOLAR-EVE runs AUTOSAR stacks on a PC (Level 3-like). Renesas' Embedded Target for RH850 Virtual Platform lists ASTC VLAB and MathWorks as partners. The common pattern is that the MCU vendor supplies specifications and early RTL access, the EDA vendor builds and sells the models, and both market them jointly.
   - ev: https://news.synopsys.com/2019-06-24-Synopsys-Expands-Center-of-Excellence-with-Infineon-to-Deliver-Virtualizer-Development-Kit-for-AURIX-TC4x-Automotive-Microcontroller ('up to 18 months before silicon availability'; TC3x VDKs 'deployed successfully at multiple tier 1 and OEM companies')
   - ev: https://synopsys.com/blogs/chip-design/enabling-electronics-digital-twins-with-nxp-s32n-processor-vdks.html (2024-04-10: VDKs for 'S32G, S32E, S32K, and the newly announced S32N'; NXP uses VDKs internally to develop SDKs)
   - ev: https://news.synopsys.com/2019-04-18-Synopsys-Establishes-Center-of-Excellence-with-STMicroelectronics-to-Speed-Development-of-Automotive-Electronic-Systems
   - ev: https://www.tasking.com/solutions/debug-solutions/vlab/ ('fast level 4 virtual ECUs'; 'simulate complete boards (ECUs, HPC nodes, custom boards, etc.)'; supported VDMs: AURIX TC3xx, TC4x; same CI scripts on real and virtual ECUs)
   - ev: https://www.design-reuse.com/blog/56171-cadence-welcomes-vlab-works/ (2025-06-22)
   - ev: https://www.synopsys.com/verification/virtual-prototyping/virtual-ecu/silver.html ('Level 1 through Level 3 vECU'; coupling with Virtualizer for 'partial and full binary SW stack integration')
   - ev: https://www.vector.com/en/product/vvirtualtarget/ ('vECUs can be exported as FMU according to FMI 3.0 and ls-bus'; 'Pre-integrated with the SIL Kit')
   - ev: https://www.wardsauto.com/spons/automotive-software-in-the-loop-sil-testing-and-how-do-i-do-it-right/816295 (VEOS integrates 'level 4 V-ECUs (instruction set simulation)')
   - ev: https://electropages.com/2022/04/new-environment-fast-automotive-software-development-and-evaluation ('R-Car VPF is based on Virtualizer Development Kits (VDKs) from Synopsys')

19. (medium) [ISO 26262 tool confidence for virtual platforms] If virtual-platform results are used as ISO 26262 verification evidence, the platform is a software tool under ISO 26262-8 Clause 11 ('Confidence in the use of software tools'). The user classifies Tool Impact (TI1/TI2) and Tool error Detection (TD1-TD3) to obtain a TCL (TI2 with TD3 gives TCL3). TCL2/3 tools must be qualified by increased confidence from use, evaluation of the tool development process, validation, or development to a safety standard. Vendors reduce the effort with certificates and kits: dSPACE VEOS is TUV SUD-certified for all ASILs (2023), and Lauterbach offers a TRACE32 ISS qualification kit for TriCore, noting 'ISO 26262 permits processor emulators like the TRACE32 Instruction Set Simulator as tools for software unit and module integration testing'.
   detail: I did not access the standard text itself; the clause numbers come from credible secondary sources (Synopsys cites the 2011 edition, itemis the 2018 edition). The practical implication is that a board-level twin sold for safety work needs a documented validation story for its MCU models, such as differential tests against silicon.
   - ev: https://www.synopsys.com/content/dam/synopsys/solutions/documents/automotive-tool-confidence-iso-26262-wp.pdf (clause 11 of part 8; TI/TD -> TCL table; 'The user of the software tool is responsible for the TCL value assessment')
   - ev: https://www.itemis.com/en/glossary/tool-qualification/ (ISO 26262-8:2018 11.4.5 classification, 11.4.6 qualification methods)
   - ev: https://support.lauterbach.com/news/posts/pdf/trace32-instruction-set-simulator-and-iso-26262 (2022-12-15; 'Confidence in the use can be justified by comparing the test results ... with reference tests')
   - ev: https://www.dspace.com/en/inc/home/news/dspace_pressroom/press/dspace-tools-achieve-certifica.cfm (2023-05-09; VEOS usable 'according to ISO 26262:2018 for all automotive safety integrity levels')

20. (high) [LOAD-BEARING] [Terminology: 'digital twin' vs 'virtual prototype/platform/hardware' vs 'V-ECU'] 'Digital twin' is used loosely and increasingly for product- or board-level models. Infineon (2025) says its RISC-V AURIX 'virtual prototype will evolve into a full-fledged digital twin'. Synopsys calls S32 VDK deployments 'electronics digital twins'. Antmicro calls its STM32H753 devkit model (IMU, environmental sensors, CAN, Ethernet, QSPI) 'a digital twin emulated in Renode'. Arm uses 'Arm Virtual Hardware' and warns 'These models are not performance accurate'. Renesas and ADI say 'virtual platform' or 'virtual development environment', and automotive standards say 'V-ECU'.
   detail: For positioning: a 'digital twin of the PIC32CM JH' reads as chip-level, while the market's 'digital twin of my product' implies a board-level model with sensors and buses.
   - ev: https://www.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html (2025-03-06; starter kit 'based on the tool suite of Infineon's strategic partner Synopsys'; partners IAR, Elektrobit, Green Hills, HighTec, Lauterbach, PLS, Synopsys, Tasking)
   - ev: https://synopsys.com/blogs/chip-design/enabling-electronics-digital-twins-with-nxp-s32n-processor-vdks.html
   - ev: https://antmicro.com/blog/2026/07/stm32h7-renode-reference-platform (2026-07-16: 'a technology demonstrator board available as a physical devkit with a digital twin emulated in Renode')
   - ev: https://www.arm.com/products/development-tools/simulation/virtual-hardware
   - ev: https://antmicro.com/blog/2026/09/how-adi-is-leveraging-renode-to-accelerate-embedded-intelligence ('virtual platform', 'virtual-first development approach')

21. (medium) [LOAD-BEARING] [MCU vendors supplying free/open models (2023-2026)] Several MCU vendors now supply or co-fund free/open models instead of, or alongside, paid VDKs. Silicon Labs and Antmicro added EFR32 Series 2 radio simulation to Renode (Aug 2025; Series 3 in progress). ADI pairs Renode with its SystemC VPs for Arm-based products (Aug/Sep 2026). Nordic engineers maintain Apache-2.0 nRF52833/5340/54L hardware models for Zephyr's native_sim/BabbleSim targets (host-compiled, not ISS). Espressif maintains a QEMU fork (ESP32, -S3, -C3, -C6; based on QEMU 9.2.2; `idf.py qemu`) and integrates Wokwi. NXP's i.MX RT700 Renode support came from an Antmicro customer project seeded by dts2repl. No TI-supplied MCU simulator was found.
   detail: The TI 'none found' result is low-confidence; MathWorks' C2000 blockset models C2000 peripherals in Simulink, which is not an ISS. Espressif's fork tracks an older QEMU base (9.2.2) and is not upstreamed, which shows the maintenance cost of a vendor fork.
   - ev: https://antmicro.com/blog/2025/08/silabs-series-2-in-renode/ (2025-08-22: 'thanks to the collaboration between Antmicro and Silicon Labs'; EFR32MG26/MG24/xG22)
   - ev: https://antmicro.com/blog/2026/09/how-adi-is-leveraging-renode-to-accelerate-embedded-intelligence (originally on ADI Developer Portal 2026-08-17)
   - ev: https://github.com/BabbleSim/ext_nRF_hw_models (docs/README.md: models for nRF52833, nRF5340, nRF54L15/LM20/LS05; docs/LICENSE Apache-2.0; src/HW_models/NHW_RADIO.c header 'Copyright (c) 2023 Nordic Semiconductor ASA')
   - ev: https://docs.zephyrproject.org/latest/boards/native/nrf_bsim/doc/nrf54l15bsim.html
   - ev: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html ('Espressif maintains a fork of the QEMU emulator')
   - ev: https://github.com/espressif/qemu (hw/xtensa/esp32.c, esp32s3.c; hw/riscv/esp32c3.c, esp32c6.c; release esp-develop-9.2.2-20260417)
   - ev: https://antmicro.com/blog/2026/03/renode-support-for-the-nxp-i-mx-rt700-mcu ('In a recent customer project...'; 'We used our dts2repl tool to automatically generate a Renode platform file')
   - ev: https://antmicro.com/blog/2025/02/renode-support-for-nxp-s32k

22. (high) [LOAD-BEARING] [Off-chip part long tail] Off-chip part libraries are small compared with customer BOMs. Renode has 50 sensor model files (e.g. LSM9DS1, BMP180, ADXL345, ICM20948, LPS25HB, SI70xx, TMP103/108, INA228, PAC1934). Wokwi lists 66 parts across sensors, input, LEDs, displays, motors, comms, logic and other. Upstream QEMU hw/sensor has 11 models aimed at BMC/PMBus. Scalability to customer boards therefore comes from the extension API and cheap stubs, not from a vendor-complete library.
   detail: Microchip's own off-chip catalog (memories, sensors, CAN/Ethernet PHYs, power monitors) could seed a part library, but customer boards will always include non-Microchip parts.
   - ev: https://github.com/renode/renode-infrastructure/tree/master/src/Emulator/Peripherals/Peripherals/Sensors (50 .cs files, 2026-10-09)
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/getting-started/supported-hardware.md#L20-L124
   - ev: hw/sensor/ (adc128d818.c, adm1266.c, adm1272.c, dps310.c, emc141x.c, isl_pmbus_vr.c, lsm303dlhc_mag.c, max31785.c, max34451.c, tmp105.c, tmp421.c)

23. (high) [Per-peripheral fidelity disclosure practice] Mature tools publish per-peripheral fidelity. Wokwi's STM32C031 table marks I2C and SPI 'Master mode only', DMA/RTC/IWDG/PWR/SYSCFG as not implemented and RCC as partial; its ATtiny85 model lacks Timer1 and the analog comparator. The Renode dashboard reports per-sample PASSED / GENERATED / NO UART / PANICKED states.
   detail: Even commercial engines ship partial MCU models and say so per peripheral.
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/parts/board-st-nucleo-c031c6.md#L27-L47
   - ev: https://github.com/wokwi/wokwi-docs/blob/main/docs/parts/wokwi-attiny85.md#L92-L112
   - ev: https://zephyr-dashboard.renode.io/ (status legend in bundled UI code: PASSED, BUILT, GENERATED, GENERATED_MISSING_UART, PANICKED, NOT BUILT)

24. (medium) [Schematic-level whole-board simulators with Microchip parts] Schematic-native simulators already let users of Microchip 8/16-bit parts simulate whole boards. Proteus VSM 'blends mixed-mode SPICE simulation with world leading fast microcontroller simulation' and lists PIC10/12/16/18/24, dsPIC33, Atmel AVR and Atmel Cortex-M3 (no PIC32 listed). SimulIDE (AGPLv3) supports PIC, AVR and Arduino.
   detail: These tools target education and small designs. Fidelity is SPICE-plus-ISS rather than full peripheral register models for modern 32-bit parts.
   - ev: https://www.labcenter.com/microprocessors/
   - ev: https://simulide.com/p/

25. (low) [LOAD-BEARING] [No netlist-to-board-simulation generator found] I found no tool that converts a customer's KiCad or Altium netlist/schematic into a Renode or QEMU board description. dts2repl (devicetree compatible strings) is the closest generator, and Proteus sidesteps the problem by being schematic-native. Customers today hand-write board files (Renode .repl), draw them (Wokwi editor) or use a schematic-native simulator.
   detail: Negative search result. For Microchip, possible machine-readable sources of board-level pinout are Zephyr board DTS (exists for pic32cm_jh01_cnano) and MCC/Harmony pin-manager output (not verified).
   - ev: https://github.com/antmicro/dts2repl/blob/main/README.md
   - ev: https://www.labcenter.com/microprocessors/
   - ev: web search 2026-10-09 ('generate simulator board description from KiCad schematic netlist ...') returned only netlist parsers (kinparse, kinet2pcb) and no simulator generator

## Table: Board-composition approaches
| tool | board description format | custom-part extension mechanism | multi-board sync | CI support | Microchip parts? | URL |
| Renode 1.17 (Antmicro, MIT) | Text .repl: `name: Type @ bus addr`; `using` SoC file; `->` GPIO/IRQ wiring; init/preinit/reset; .resc scripts; dts2repl auto-generates .repl from Zephyr DTS | C# compiled at load time (Roslyn, `include @X.cs`); IronPython-2 PythonPeripheral; HDL co-sim (DPI/TCP or Verilator .so/.dll); PeakRDL-renode scaffolds; external control API, pyrenode3 | In-process global-quantum barrier; UART/CAN hubs, Ethernet switch, wireless medium and GPIOConnector deliver only at sync points; `emulation SetGlobalQuantum`; skew <= quantum | renode-test + Robot Framework; antmicro/renode-test-action; Zephyr dashboard (1,170 boards) | Yes: SAM E70, SAM4S (+Xplained), ATSAMD21J17D, ATSAMD51G19A, PolarFire SoC/Icicle, Mi-V; dashboard: 21 SAM/PIC32C boards incl. PIC32CM JH01 at UART level only; PAC1934, ENC28J60 | https://renode.readthedocs.io/en/latest/advanced/platform_description_format.html |
| Wokwi (SaaS, VS Code, CLI) | diagram.json parts {id,type,left,top,attrs} + pin-to-pin connections; wokwi.toml (firmware, elf, [[chip]]) | Custom Chips API (beta): C/Rust/AssemblyScript to WASM (Verilog experimental); pin, analog, timer, UART, I2C, SPI, attributes, framebuffer; chip.json pins/controls | None: one MCU per project; wiring MCUs together unsupported | wokwi-cli; wokwi/wokwi-ci-action@v1; YAML scenarios (alpha); MCP server; cloud minute quotas; on-prem by request | AVR only: ATmega328P, ATmega2560, ATtiny85 | https://docs.wokwi.com/diagram-format |
| QEMU upstream 11.1.50 | Board = C machine type; CLI `-device part,bus=<i2c/ssi/sd bus>,address=..` for bus-attached parts only; sysbus devices not user-creatable | New C (or Rust) model compiled into QEMU; QOM properties via QMP qom-set for environment values | None for multi-instance virtual time; QTest clock_step only with qtest accel (no guest CPU) | In-tree qtest + Python functional tests; scriptable in any CI | microchip-icicle-kit (PolarFire SoC); arduino-uno/mega/mega-2560-v3/duemilanove (ATmega328P/1280/2560/168) | hw/core/sysbus.c:300-310; tests/functional/arm/test_aspeed_ast2600_buildroot.py:29-44 |
| AMD/Xilinx QEMU fork + libsystemctlm-soc | `-M arm-generic-fdt -hw-dtb <board>.dtb`: machine generated from a hardware device tree | SystemC/TLM-2.0 models and Verilated RTL over remote-port sockets | remote-port with -sync-quantum | PetaLinux/Vitis flows | None (Zynq/Versal) | https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/821428366/QEMU+Options+and+Commands |
| SimBricks (MIT, research) | Python orchestration of separate simulator processes joined at PCIe/Ethernet interfaces | Adapters: QEMU, gem5, Verilator RTL, ns-3, OMNeT++, Tofino | Pairwise conservative sync using link latency delta + SYNC messages; 1,000 hosts on 26 servers | Scripted experiments | None | https://arxiv.org/abs/2012.14219 |
| SIL Kit (Vector/Synopsys, MIT) | No board format; participants on a registry exchange CAN/LIN/FlexRay/Ethernet, pub/sub, RPC | Adapters: QEMU (socket netdev/chardev), FMU importer, SocketCAN, TAP | Optional synchronized virtual time for coordinated participants (none documented for the QEMU adapter) | Pre-integrated in vECU tools (e.g., vVIRTUALtarget) | n/a | https://github.com/vectorgrp/sil-kit |
| Synopsys Virtualizer/VDK; VLAB VDM (Cadence; sold by TASKING) | SystemC/TLM virtual prototypes; VLAB: 'simulate complete boards (ECUs, HPC nodes, custom boards, etc.)' | SystemC models; couplings to Simulink/Saber/CANoe, Silver, VEOS, FMI | Vendor tools; rest-bus/network via Silver, VEOS, SIL Kit | VLAB winIDEA SDK: same CI scripts on real and virtual ECUs | None found (AURIX, S32, Stellar, RH850/R-Car) | https://www.tasking.com/solutions/debug-solutions/vlab/ |
| Zephyr native_sim / nRF bsim | Devicetree + Kconfig of a host-compiled board with nRF HW models linked in | Zephyr emulators (I2C, SPI, MSPI, UART, GPIO, ADC, EEPROM...) built with the firmware | BabbleSim models the shared radio medium | Used as integration test benches for Zephyr/NCS | None | https://docs.zephyrproject.org/latest/boards/native/doc/bsim_boards_design.html |
| Proteus VSM (Labcenter, commercial) | Schematic capture is the board description; mixed-mode SPICE + MCU models | SPICE/VSM component models | n/a | n/a found | PIC10/12/16/18/24, dsPIC33, Atmel AVR, Atmel Cortex-M3 (no PIC32 listed) | https://www.labcenter.com/microprocessors/ |
| SimulIDE (AGPLv3) | Circuit schematic + subcircuits | Subcircuits | n/a | n/a | PIC, AVR, Arduino | https://simulide.com/p/ |
| Arm Virtual Hardware | Fixed models: Corstone FVPs, Cortex-M cores; 3rd-party boards (RPi 4, i.MX 93, i.MX 8M, STM32U5 IoT Discovery) | Virtual Streaming Interface (VSI) Python scripts for data I/O | n/a | Free FVPs (evaluation/open source/non-commercial), AWS AMI, paid subscription | None | https://www.arm.com/products/development-tools/simulation/virtual-hardware |

## Table: vECU levels
| level | definition | what's simulated | source |
| Level 0 - Controller Model | 'comprises only the controller model itself (e.g., as an MATAB [sic] Simulink model) or the C code generated from the controller model (e.g., as an FMU)' | Control algorithm only; no production code, no BSW | prostep ivip SmartSE Recommendation 2025, PSI 11 V4.0 Part F, p.4 (https://www.prostep.org/fileadmin/prod-pay-download-8c1d/PSI_11_V4_SmartSE_Part_F_VECU.pdf) |
| Level 1 - Application Level | 'contains production code of the application software', supplemented by lower-layer functions (RTE, OS) created for the V-ECU; 'communicates on signal level, it does not use bus or network communication' | Production application SW-Cs on host with generated RTE/OS stubs | same, p.4 |
| Level 2 - Simulation BSW | Level 1 plus 'emulated basic software (BSW) functionalities' (e.g., COM stack, NVRAM, diagnostics) built for simulation; communicates 'at bus or network level' | Application code + simulated BSW; simulated CAN/Ethernet | same, pp.4-5 |
| Level 3 - Production BSW | 'include not only production application software but also production basic software'; must be hardware independent (AUTOSAR: everything above MCAL and the OS) | Host-compiled production stack without MCAL/OS/hardware-dependent driver parts; V-ECU networks; rest-bus models in HIL | same, p.5 |
| Level 4 - Target Binary | 'contains production code compiled for the real target ECU'; may include MCAL, OS and complex device drivers; 'An instruction set simulator and the model for the corresponding ECU hardware is needed' | Unmodified target binary on an ISS plus MCU peripheral and ECU (board) hardware models; same build as for flashing | same, p.5 |

## Table: MCU vendor simulation offerings
| vendor | offering | framework | models supplied by | cost model | URL |
| Microchip | Renode integrated in SoftConsole for PolarFire SoC & Mi-V ('default virtual development option'; PolarFire SoC available pre-silicon); Renode also models SAM E70/SAM4S/SAMD21 | Renode | Antmicro in cooperation with Microchip | Free, open source (MIT) | https://antmicro.com/blog/2024/07/open-source-som-with-polarfire-soc |
| NXP | VDKs for S32G/S32E/S32K/S32N ('electronics digital twins'); AVH models of i.MX 93 / i.MX 8M; Renode S32K388 (2025) and i.MX RT700 (2026, Antmicro customer project) | Synopsys Virtualizer (SystemC); Arm AVH; Renode | Synopsys (Center of Excellence; NXP uses VDKs for SDK development); Arm; Antmicro | VDK commercial; AVH free tier + paid; Renode free | https://synopsys.com/blogs/chip-design/enabling-electronics-digital-twins-with-nxp-s32n-processor-vdks.html |
| Infineon | VDKs for AURIX TC3x/TC4x ('up to 18 months before silicon'); RISC-V AURIX virtual prototype starter kit (Embedded World 2025) to 'evolve into a full-fledged digital twin'; VLAB VDMs for AURIX TC3xx/TC4x | Synopsys tool suite; VLAB | Synopsys (Center of Excellence) + ecosystem partners; VLAB Works (Cadence) | Commercial (starter kit for ecosystem partners) | https://www.infineon.com/cms/en/about-infineon/press/press-releases/2025/INFATV202503-067.html |
| Renesas | R-Car Virtual Platform (VPF) with R-Car virtual IP models + R-Car SDK (2022); Embedded Target for RH850 Virtual Platform (Simulink vHILS; V2.01.00, 2025-12-05) | Synopsys VDK; ASTC VLAB + MathWorks Simulink | Synopsys; Renesas (IP models/SDK, code generation); partners | Commercial; ET-VPF annual license with 3-month free evaluation | https://www.renesas.com/en/software-tool/model-based-development-environment-rh850-r-car-family-embedded-target-virtual-platform-embedded |
| ST | VDK for Stellar automotive MCUs (2019; tier-1s developed software 18 months ahead of samples); AVH model of STM32U5 IoT Discovery; Wokwi supports STM32C031/L031/F103 (no ST announcement found) | Synopsys Virtualizer; Arm AVH | Synopsys; Arm | Commercial; AVH free tier | https://news.synopsys.com/2019-04-18-Synopsys-Establishes-Center-of-Excellence-with-STMicroelectronics-to-Speed-Development-of-Automotive-Electronic-Systems |
| TI | No TI-supplied MCU virtual platform found for MSPM0/C2000 (2023-2026); C2000 peripheral behaviour modelled in the MathWorks C2000 blockset (Simulink) | Simulink (not an ISS) | MathWorks | Commercial (MATLAB) | https://www.mathworks.com/help/ti-c2000/model-based-design.html (low confidence) |
| Nordic | HW models of nRF52833, nRF5340, nRF54L15/LM20/LS05 for Zephyr nrf*_bsim targets | Zephyr native simulator + BabbleSim (host-compiled) | Nordic (Apache-2.0; copyright Nordic Semiconductor ASA / Oticon) | Free, open source | https://github.com/BabbleSim/ext_nRF_hw_models |
| Silicon Labs | EFR32 Series 2 (MG26/MG24/xG22) incl. 2.4 GHz multiprotocol radio in Renode; Series 3 in progress (Aug 2025) | Renode | Antmicro in collaboration with Silicon Labs | Free, open source | https://antmicro.com/blog/2025/08/silabs-series-2-in-renode/ |
| ADI (additional) | Renode combined with ADI SystemC virtual platforms for Arm-based products (2026) | Renode + SystemC | ADI (SystemC VPs); Antmicro (Renode, services) | Renode free; services paid | https://antmicro.com/blog/2026/09/how-adi-is-leveraging-renode-to-accelerate-embedded-intelligence |
| Espressif (additional) | QEMU fork for ESP32/-S3/-C3/-C6 (`idf.py qemu`); Wokwi integration via pytest-embedded-wokwi and Espressif IDE | QEMU 9.2.2-based fork; Wokwi | Espressif (QEMU); Wokwi | Free (QEMU); Wokwi freemium | https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/tools/qemu.html |

## Table: Renode Zephyr dashboard: Microchip boards (Zephyr + Renode commits of 2026-10-02)
| board | hello_world | blinky | shell_module | note |
| pic32cm_jh01_cnano | PASSED | GENERATED (not passed) | PASSED | Generated REPL: core + NVIC + SERCOM1 UART; PORT is a Tag |
| pic32cm_jh01_cpro | PASSED | GENERATED | PASSED | Same family as user's DFP |
| samc21n_xpro | PASSED | GENERATED | PASSED | Closest SAM relative of PIC32CM JH |
| samd21_xpro | PASSED | GENERATED | PASSED | Hand-written SAMD21_GPIO exists in Renode but is not used by the generated platform |
| sam4s_xplained | PASSED | GENERATED | PASSED | Hand-written sam4s_xplained.repl with LED exists but is not used by the dashboard |
| sam_e70_xplained | PASSED | GENERATED | PASSED | Also used in multi-node gPTP demo |
| pic32cz_ca80_cult | PASSED | GENERATED | PASSED |  |
| pic32ck_sg01_cult | GENERATED_MISSING_UART | GENERATED_MISSING_UART | GENERATED_MISSING_UART | No UART model mapped |
| mpfs_icicle (u54) | PASSED | PASSED | PASSED | Real MPFS_GPIO model |
| pic64gx_curiosity_kit (u54) | PASSED | PASSED | PASSED | RISC-V; MPFS-derived models |
| All 29 SAM/PIC32C boards | 21 boards PASSED | 0 boards PASSED | - | Dashboard-wide: 1,063 targets pass hello_world, 425 pass blinky |

## Code: Renode board file = SoC include + off-chip parts + GPIO wiring (SAM4S Xplained) (https://github.com/renode/renode/blob/master/platforms/boards/sam4s_xplained.repl#L1-L9)
```none
using "platforms/cpus/sam4s16c.repl"

LED: Miscellaneous.LED @ pioC 10

pioC:
    10 -> LED@0

Button: Miscellaneous.Button @ pioA 5
    -> pioA@5
```

## Code: Renode board with I2C/SPI sensors registered at bus addresses (https://github.com/renode/renode/blob/master/platforms/boards/arduino_nano_33_ble.repl#L1-L20)
```none
using "platforms/cpus/nrf52840.repl"

gpio0:                // PinName in ArduinoIDE
    23 -> camera@0    // D7
    24 -> led_red@0   // LED_RED
    16 -> led_green@0 // LED_GREEN
    6  -> led_blue@0  // LED_BLUE

camera: Sensors.ArduCAMMini2MPPlus @ {
         spi2;
         twi0 0x30
    }

lsm9ds1_imu: Sensors.LSM9DS1_IMU @ twi0 0x6b

lsm9ds1_mag: Sensors.LSM9DS1_Magnetic @ twi0 0x1e

led_red: Miscellaneous.LED @ gpio0 24
led_green: Miscellaneous.LED @ gpio0 16
led_blue: Miscellaneous.LED @ gpio0 6
```

## Code: Renode multi-node: 4 ECUs on a CAN hub with a global sync quantum (https://github.com/renode/renode/blob/master/scripts/multi-node/ramn.resc#L1-L23)
```none
:name: RAMN
:description: Runs the RAMN firmware (https://github.com/ToyotaInfoTech/RAMN) on 4 ECUs connected over CAN

emulation CreateCANHub "canHub"

set global.name "ECUA"
include @scripts/single-node/ramn.resc 
connector Connect sysbus.fdcan1 canHub

set global.name "ECUB"
include @scripts/single-node/ramn.resc 
connector Connect sysbus.fdcan1 canHub

set global.name "ECUC"
include @scripts/single-node/ramn.resc 
connector Connect sysbus.fdcan1 canHub

set global.name "ECUD"
include @scripts/single-node/ramn.resc 
connector Connect sysbus.fdcan1 canHub

# Syncing more freqently improves performance in this senario
emulation SetGlobalQuantum "0.00001"
```

## Code: Renode Python (IronPython) peripheral stub declared inline in a board file (https://github.com/renode/renode/blob/master/platforms/boards/ramn.repl#L3-L12)
```none
pwr: Python.PythonPeripheral @ sysbus 0x40007000
    size: 0x400
    initable: false
    script: '''
if request.IsRead:
    if request.Offset == 0x10:  # PWR_SR1
        request.Value = 1 << 15 # SMPSHPRDY
    else:
        request.Value = 0
'''
```

## Code: Renode HDL co-simulated block (Verilator/DPI over TCP) declared in a .repl (https://github.com/renode/renode-docs/blob/master/source/advanced/co-simulating-with-an-hdl-simulator.md#L36-L39)
```none
block: CoSimulated.CoSimulatedPeripheral @ sysbus <0x20000000, +0x100000>
    address: "127.0.0.1"
    SimulationPath: build/verilated
    SimulationContext: "--custom_features_enabled +RENODE_RECEIVER_PORT={0} +RENODE_SENDER_PORT={1} +RENODE_ADDRESS={2}"
```

## Code: dts2repl: devicetree compatible -> Renode model map (real models vs Python stub fallback) (https://github.com/antmicro/dts2repl/blob/main/dts2repl/models.json#L124-L128)
```json
    "atmel,sam-usart": "UART.SAM_USART",
    "atmel,sam-uart": { "type": "UART.SAM_USART", "uartOnlyMode": true },
    "atmel,sam0-uart": { "atmel,samd20": "UART.SAMD20_UART", "_": "UART.SAMD5_UART" },
    "atmel,sam0-i2c": { "type": "I2C.SAMD21_I2C", "irq_mappings": ["", null, null, null] },
    "atmel,sama5d2-tcb": { "type": "Python.PythonPeripheral", "size": 0x4000, "initable": true, "filename": "scripts/pydev/flipflop.py" },
```

## Code: Auto-generated Renode platform for PIC32CM JH01 Curiosity Nano: only core, NVIC, one SERCOM UART are real; clock regs faked as UNK (https://zephyr-dashboard.renode.io/zephyr_sim/2428837a26050e11d0d541432a5fa783666c0c8e/8f7a74f51eecc9f5691cc3c42ce6d0deab07b84e/pic32cm_jh01_cnano/hello_world/hello_world.repl#L83-L110)
```none
// autogenerated

nvic0: IRQControllers.NVIC @ {
    sysbus new Bus.BusPointRegistration { address: 0xe000e000; cpu: cpu0 }
}
    -> cpu0@0
    systickFrequency: 48000000

cpu0: CPU.CortexM @ sysbus
    cpuType: "cortex-m0+"
    nvic: nvic0

sercom1: UART.SAMD5_UART @ sysbus 0x42000800
    IRQ->nvic0@10

// cortex-m overlay

dwt: Miscellaneous.DWT @ sysbus 0xE0001000
    frequency: 72000000

// microchip,pic32cm overlay

sysbus:
    init add:
        Tag <0x4000100C 0x4> "UNK1" 0x11
        Tag <0x40001030 0x4> "UNK2" 0x03
        Tag <0x4000140C 0x4> "UNK3" 0x03
        Tag <0x40001414 0x4> "OSCCTRL_STATUS" 0x3
```

## Code: Dashboard blinky test needs a board-level LED on a PORT model that the generated PIC32CM platform lacks (https://zephyr-dashboard.renode.io/zephyr_sim/2428837a26050e11d0d541432a5fa783666c0c8e/8f7a74f51eecc9f5691cc3c42ce6d0deab07b84e/pic32cm_jh01_cnano/blinky/blinky.robot#L1-L12)
```robotframework
*** Variables ***
${LED}                          sysbus.porta.led0

*** Test Cases ***
blinky on pic32cm_jh01_cnano
    ${x}=                       Execute Command         include @${CURDIR}/blinky.resc
    Create Terminal Tester      sysbus.sercom1    timeout=15  defaultPauseEmulation=true
    Create LED Tester           ${LED}

    Wait For Line On Uart       *** Booting Zephyr OS build 2428837a2605 ***
    Wait For Line On Uart       LED state: (ON|OFF)      treatAsRegex=true
    Assert LED Is Blinking      testDuration=4  onDuration=1  offDuration=1
```

## Code: Wokwi diagram.json part entry (https://github.com/wokwi/wokwi-docs/blob/main/docs/diagram-format.md#L53-L61)
```json
{
  "id": "led1",
  "type": "wokwi-led",
  "left": 100,
  "top": 50,
  "attrs": {
    "color": "red"
  }
}
```

## Code: Wokwi diagram.json pin-to-pin connection (net-level wiring) (https://github.com/wokwi/wokwi-docs/blob/main/docs/diagram-format.md#L120)
```json
  ["led1:A", "uno:13", "green", []],
```

## Code: Wokwi Custom Chips API: I2C target device (https://github.com/wokwi/wokwi-docs/blob/main/docs/chips-api/i2c.md#L36-L64)
```c
bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  // `address` parameter contains the 7-bit address that was received on the I2C bus.
  // `read` indicates whether this is a read request (true) or write request (false).
  return true; // true means ACK, false NACK
}

uint8_t on_i2c_read(void *user_data) {
  return 0; // The byte to be returned to the microcontroller
}

bool on_i2c_write(void *user_data, uint8_t data) {
  // `data` is the byte received from the microcontroller
  return true; // true means ACK, false NACK
}

void on_i2c_disconnect(void *user_data) {
  // This method is optional. Useful if you need to know when the I2C transaction has concluded.
}

static const i2c_config_t i2c1 {
  .address = 0x22,
  .scl = pin_init("SCL", INPUT_PULLUP),
  .sda = pin_init("SDA", INPUT_PULLUP),
  .connect = on_i2c_connect,
  .read = on_i2c_read,
  .write = on_i2c_write,
  .disconnect = on_i2c_disconnect,
  .user_data = chip,
};
```

## Code: Upstream QEMU: composing I2C parts onto a board from the CLI and driving a sensor via QMP qom-set (tests/functional/arm/test_aspeed_ast2600_buildroot.py:29-44)
```python
        self.vm.add_args('-device',
                         'tmp105,bus=aspeed.i2c.bus.3,address=0x4d,id=tmp-test')
        self.vm.add_args('-device',
                         'ds1338,bus=aspeed.i2c.bus.3,address=0x32')
        self.vm.add_args('-device',
                         'i2c-echo,bus=aspeed.i2c.bus.3,address=0x42')
        self.do_test_arm_aspeed_buildroot_start(image_path, '0xf00',
                                                'ast2600-evb login:')

        exec_command_and_wait_for_pattern(self,
             'echo lm75 0x4d > /sys/class/i2c-dev/i2c-3/device/new_device',
             'i2c i2c-3: new_device: Instantiated device lm75 at 0x4d')
        exec_command_and_wait_for_pattern(self,
                             'cat /sys/class/hwmon/hwmon1/temp1_input', '0')
        self.vm.cmd('qom-set', path='/machine/peripheral/tmp-test',
                    property='temperature', value=18000)
```

## Code: Upstream QEMU: why on-chip (sysbus) devices cannot be composed with -device (hw/core/sysbus.c:300-310)
```c
    /*
     * device_add plugs devices into a suitable bus.  For "real" buses,
     * that actually connects the device.  For sysbus, the connections
     * need to be made separately, and device_add can't do that.  The
     * device would be left unconnected, and will probably not work
     *
     * However, a few machines can handle device_add/-device with
     * a few specific sysbus devices. In those cases, the device
     * subclass needs to override it and set user_creatable=true.
     */
    k->user_creatable = false;
```

## Open questions
- Does Microchip already hold pre-silicon or MPLAB SIM peripheral models (SystemC/TLM or C) for SERCOM, PORT/EIC, TC/TCC, ADC and DMAC that could seed chip-level twins quickly? This cannot be verified from public sources.
- Licensing: Renode is MIT, while QEMU device models linked into QEMU fall under GPLv2. Whether Microchip may ship closed peripheral models in each ecosystem needs legal review; I did not research it.
- Which customer segment drives the requirement? Automotive (Level 4 vECU, ISO 26262 qualification, FMI-LS-BUS/SIL Kit integration), IoT/industrial CI (Renode/Wokwi style) and education (Proteus/Wokwi/SimulIDE) imply different board formats and fidelity targets.
- Could MCC/Harmony pin-manager output or the Zephyr board DTS (which exists for pic32cm_jh01_cnano) auto-generate the board-level description, as dts2repl does? I found no netlist-to-simulator generator.
- How well do Renode's existing SAMD21_GPIO, SAMD21_I2C, SAMD21_Timer and SAMD5_UART models match PIC32CM JH register semantics (SERCOM U2201 v3.1.1, PORT U2210 v3.1.0, TC U2249)? This was not tested.
- Would Antmicro (dts2repl/Renode) or Wokwi accept Microchip-contributed, ATDF-generated models, and on what commercial terms? I found no Wokwi chip-vendor partnership terms.
- microchip.com pages returned HTTP 403 to automated fetch, and the developerhelp MPLAB X simulator page timed out. The SoftConsole/Renode bundling and version should be confirmed internally.
- For multi-MCU boards (e.g., MCU + FPGA over SPI or parallel bus), what synchronization granularity is acceptable? SimBricks found QEMU's synchronous MMIO hard to keep deterministic; the QEMU icount determinism question belongs to the separate internals research run.
- The exact ISO 26262 clause wording (Part 8 Clause 11 tables; Part 6 test-environment notes) was not read from the standard itself, only from secondary sources.
