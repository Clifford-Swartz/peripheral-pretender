# qemu-internals

## Summary
In QEMU master (VERSION 11.1.50, commit 602e4f2da1), a Cortex-M MCU is layered as follows. A MachineClass board instantiates an SoC device, which is a TYPE_SYS_BUS_DEVICE subclass that owns the flash/SRAM MemoryRegions and every peripheral child. The SoC realizes a TYPE_ARMV7M container (CPU + NVIC + SysTick), which needs a 'cpuclk' Clock, treats 'refclk' as optional and re-exports the NVIC lines as GPIO inputs. Peripherals are SysBusDevices built from MemoryRegionOps, qemu_irq outputs, Clock inputs and chardev/I2C/SPI backends. Every M-profile board must call armv7m_load_kernel(). There is no cortex-m0plus CPU type, only 'cortex-m0' (CPUID 0x410cc200, no MPU regions by default, no unprivileged extension). The Armv6-M NVIC is capped at 32 external IRQs with 2 priority bits by default, which fits PIC32CM JH01 (28-32 IRQs, 2 priority bits, VTOR present); the gaps are the CPUID value, the MPU and CONTROL.nPRIV. The recommended template is hw/arm/stm32l4x5_soc.c + b-l475e-iot01a.c. It has a Clock-API clock controller that drives armv7m and every peripheral, one SoC subclass per flash size, OR-gate fan-in for shared vectors, full unimplemented-device coverage, careful errp handling, five per-peripheral qtests and an active maintainer. The CPU/NVIC settings and the rom_device flash pattern should come from nrf51_soc.c, the only in-tree Cortex-M0 SoC. Modern device idioms are OBJECT_DECLARE_* + DEFINE_TYPES, class_init(ObjectClass*, const void*), and an instance_init with no side effects (MMIO/IRQ/clock/QOM properties) versus a realize that can fail (clock_has_source, chardev handlers, timers). Other idioms: DEVICE_LITTLE_ENDIAN ops with .valid/.impl, where an invalid size gives a LOG_INVALID_MEM log plus MEMTX_DECODE_ERROR, which becomes a HardFault on v6-M, and smaller accesses are widened to the .impl size without read-modify-write or realignment. Others are 'static const Property[]' with no END_OF_LIST, three-phase Resettable reset (the legacy reset is deprecated), VMState including VMSTATE_CLOCK, trace-events, and qemu_log_mask(LOG_GUEST_ERROR/LOG_UNIMP). Headers now live under include/hw/core/ and system/memory.h, and the chardev frontend type is CharFrontend. Build integration is Kconfig (board 'default y, depends on TCG && ARM, select SOC'; SoC selects ARM_V7M/OR_IRQ/UNIMP/devices), meson source sets, docs and MAINTAINERS. Out-of-tree device modules are not viable (modules are off by default, unavailable on Windows, limited to a fixed in-tree list and resolved through a build-time modinfo table), so a fork or upstreaming is required. Upstream QEMU currently declines AI-generated contributions. Tests are libqtest C programs under '-accel qtest' (MMIO via the CPU address space, IRQ intercept/injection, clock_step, qom-get/set, serial sockets) plus Python functional tests that boot pinned firmware assets. Rust is optional and off by default (rustc >= 1.83, bindgen >= 0.60, meson >= 1.12). Its bindings cover sysbus MMIO, IRQs, clocks, timers, chardev and VMState, but not ptimer or boards, and only the pl011 and hpet devices exist, so C is the pragmatic choice. Without icount, the virtual clock follows host time and the CPU runs as fast as TCG can. '-icount shift=N[,sleep=off]' gives deterministic time at a power-of-two 2^N ns per instruction, which is not cycle-accurate, and enables record/replay and reverse debugging. Peripheral timing therefore has to be modelled explicitly with QEMU_CLOCK_VIRTUAL events derived from each device's Clock input. Runtime control already exists: QMP qom-get/set with custom setters, HMP mtree/qtree, the gdbstub with M-profile registers, tracing, TCG plugins and the qtest socket. What is missing is QMP pin-change events and any generic sysbus co-simulation bridge, so external I/O for the digital twin should go through chardev sockets or custom QOM properties.

## Findings

1. (high) [LOAD-BEARING] [Layering: Machine -> SoC -> armv7m -> peripherals] A current Cortex-M board is a MachineClass whose init instantiates an SoC device (subclass of TYPE_SYS_BUS_DEVICE). The SoC owns the flash/SRAM MemoryRegions and embeds all peripherals plus a TYPE_ARMV7M container (CPU + NVIC + SysTick + optional bitband). Peripheral IRQs are wired with sysbus_connect_irq(dev, n, qdev_get_gpio_in(armv7m, irq)), because armv7m re-exports the NVIC's unnamed GPIO inputs together with the named 'NMI' input and 'SYSRESETREQ' output.
   detail: Children are embedded in the SoC state struct and created with object_initialize_child() in instance_init. Properties are set, clocks connected, and children realized/mapped (sysbus_mmio_map) in the SoC realize. The board creates the SoC (object_initialize_child, or qdev_new + object_property_add_child), realizes it, then calls armv7m_load_kernel(). armv7m's 'memory' link chooses the MemoryRegion the CPU sees: system memory (stm32l4x5, max78000, msf2) or an SoC container layered over board memory (nrf51).
   - ev: hw/arm/b-l475e-iot01a.c:73-86
   - ev: hw/arm/b-l475e-iot01a.c:113-137
   - ev: hw/arm/stm32l4x5_soc.c:136-164
   - ev: hw/arm/stm32l4x5_soc.c:166-213
   - ev: hw/arm/stm32l4x5_soc.c:470-494
   - ev: hw/arm/armv7m.c:377-379
   - ev: include/hw/arm/armv7m.h:39-64
   - ev: hw/arm/nrf51_soc.c:86-92

2. (high) [LOAD-BEARING] [armv7m clock inputs] armv7m refuses to realize unless 'cpuclk' has a source; 'refclk' is optional. Both clocks only feed the SysTick devices and do not change the instruction rate. If refclk is unconnected, SysTick forces CSR.CLKSOURCE=1 and reports SYST_CALIB.NOREF, and it follows cpuclk frequency changes through ClockUpdate callbacks.
   detail: A guest-programmable clock controller (GCLK/MCLK/OSCCTRL on PIC32CM) that drives armv7m cpuclk, as the stm32l4x5 RCC does with 'cortex-fclk-out', keeps SysTick correct when firmware reconfigures clocks. nrf51 and max78000 leave refclk unconnected.
   - ev: hw/arm/armv7m.c:276-277
   - ev: hw/arm/armv7m.c:292-296
   - ev: hw/arm/armv7m.c:455-489
   - ev: hw/timer/armv7m_systick.c:36-43
   - ev: hw/timer/armv7m_systick.c:101-102
   - ev: hw/timer/armv7m_systick.c:142-144
   - ev: hw/timer/armv7m_systick.c:206-208
   - ev: hw/timer/armv7m_systick.c:217-239
   - ev: hw/arm/stm32l4x5_soc.c:205-208
   - ev: hw/arm/nrf51_soc.c:69-84

3. (high) [LOAD-BEARING] [Firmware loading] Every M-profile board must call armv7m_load_kernel(cpu, kernel_filename, base, size). It loads an ELF (otherwise a raw image at base) into the CPU address space and always registers the CPU reset handler. At reset the CPU fetches SP/PC from the vector table at init-svtor/VTOR, taken from a ROM blob if one is present. '-device loader,file=' also accepts ELF, uImage and Intel HEX; raw images are capped at the machine ram_size.
   detail: XC32 produces ELF and HEX, so both '-kernel app.elf' and '-device loader,file=app.hex' fit. The microbit functional test boots a .hex through the loader. Gotcha: b-l475e sets default_ram_size=0, so a raw .bin passed through -device loader would exceed the max-size check; use -kernel for raw binaries. The max78000 board loads its image at 0 although its flash is at 0x10000000, and relies on the ROM-blob vector fetch.
   - ev: hw/arm/armv7m.c:604-643
   - ev: target/arm/cpu.c:552-580
   - ev: hw/core/generic-loader.c:150-171
   - ev: hw/core/loader.c:144-167
   - ev: hw/arm/b-l475e-iot01a.c:84-86
   - ev: hw/arm/b-l475e-iot01a.c:124-125
   - ev: tests/functional/arm/test_microbit.py:21-24
   - ev: hw/arm/max78000fthr.c:33-35
   - ev: include/hw/arm/max78000_soc.h:24-27

4. (high) [LOAD-BEARING] [Cortex-M0+ support gap] There is no 'cortex-m0plus' CPU type. The only Armv6-M CPU is 'cortex-m0' (MIDR/CPUID 0x410cc200), which has no MPU regions by default and no Unprivileged/Privileged extension: ARM_FEATURE_M_UNPRIV is implied only for M_MAIN cores, so writes to CONTROL.nPRIV are ignored. VTOR (0xE000ED08) is implemented unconditionally, so vector relocation works.
   detail: armv7m forwards 'mpu-ns-regions' to the CPU property 'pmsav7-dregion', which is added for all M-profile CPUs; the comment there says the PMSAv7 state also models the Armv6-M MPU layout. An 8-region v6-M MPU is therefore probably configurable on cortex-m0, but this was not built or run here. A real Cortex-M0+ reports CPUID part number 0xC60 (from Arm documentation, not verified locally). A small target/arm patch adding a cortex-m0plus type would only be needed if firmware checks CPUID or uses nPRIV/MPU. The only M0+ mention in QEMU is a comment in the NVIC.
   - ev: target/arm/tcg/cpu-v7m.c:45-75
   - ev: target/arm/tcg/cpu-v7m.c:325-339
   - ev: target/arm/cpu.c:1465-1472
   - ev: target/arm/cpu.c:1667-1676
   - ev: target/arm/cpu.c:2340-2345
   - ev: target/arm/tcg/m_helper.c:2778-2781
   - ev: hw/intc/armv7m_nvic.c:1055-1056
   - ev: hw/intc/armv7m_nvic.c:1109-1110
   - ev: hw/intc/armv7m_nvic.c:1638-1640
   - ev: hw/intc/armv7m_nvic.c:1378-1400
   - ev: hw/arm/armv7m.c:349-355
   - ev: hw/intc/armv7m_nvic.c:2770

5. (high) [LOAD-BEARING] [NVIC v6-M limits vs PIC32CM JH01] The QEMU NVIC rejects more than 32 external IRQs on Armv6-M and defaults to 2 priority bits. PIC32CM JH01 parts fit: PERIPH_MAX_IRQn is 27 on PIC32CM1216JH01032 and 31 on the 2532/5164 variants, with __NVIC_PRIO_BITS 2, __VTOR_PRESENT 1, __MPU_PRESENT 1 and core_cm0plus.h.
   detail: Several vectors are shared: IRQ0 (MCLK/OSCCTRL/OSC32KCTRL/PAC/PM/SUPC), 9 (SERCOM0/6), 10 (SERCOM1/7) and 20-22 (TC0/5, TC1/6, TC2/7). Each needs a TYPE_OR_IRQ in the SoC.
   - ev: hw/intc/armv7m_nvic.c:2758-2783
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:84-127
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:238-247
   - ev: PIC32CM-JH01/include/pic32cm1216jh01032.h:110
   - ev: PIC32CM-JH01/include/pic32cm1216jh01032.h:206-207
   - ev: PIC32CM-JH01/include/pic32cm2532jh01048.h:125

6. (high) [LOAD-BEARING] [Only in-tree Armv6-M SoC] hw/arm/nrf51_soc.c is the only in-tree Cortex-M0 SoC; every other MCU SoC uses an M3, M4 or M33-class CPU. It sets cpu-type cortex-m0 and num-irq 32, fixes sysclk at 16 MHz internally, leaves refclk unconnected, and models flash as a rom_device in hw/nvram/nrf51_nvm.c.
   detail: The microbit board has a qtest (UART, TWI, NVMC, GPIO via set_irq_in, timer via clock_step), a MicroPython functional test, and a mini-kernel in boot-serial-test. Its style is older: type_init/type_register_static and legacy reset in nrf51_uart.
   - ev: hw/arm/nrf51_soc.c:69-92
   - ev: hw/arm/nrf51_soc.c:186-191
   - ev: hw/nvram/nrf51_nvm.c:285-306
   - ev: hw/nvram/nrf51_nvm.c:336-347
   - ev: docs/system/arm/nrf.rst:21
   - ev: tests/qtest/microbit-test.c:335-373
   - ev: tests/qtest/microbit-test.c:505-520
   - ev: tests/qtest/boot-serial-test.c:92-108
   - ev: hw/char/nrf51_uart.c:312

7. (high) [LOAD-BEARING] [Recommended template] Copy hw/arm/stm32l4x5_soc.c + hw/arm/b-l475e-iot01a.c as the template for a Microchip Cortex-M0+ SoC. Take the CPU/NVIC parameters (cortex-m0, num-irq <= 32, default 2 priority bits, no bitband) from nrf51_soc.c and the flash self-programming pattern from nrf51_nvm.c.
   detail: Reasons:
(1) An end-to-end Clock API tree: the RCC device owns oscillators, PLLs and muxes and exports per-peripheral clocks via qdev_alias_clock. This maps directly onto PIC32CM OSCCTRL/OSC32KCTRL -> GCLK generators/channels -> MCLK masks.
(2) An abstract SoC with one subclass per flash size, carried in a class field. This maps onto the JH01 1216/2532/5164 variants (128/256/512 KB flash, 16/32/64 KB RAM, 28/32 IRQs).
(3) TYPE_OR_IRQ fan-in for shared NVIC lines.
(4) A GPIO -> SYSCFG -> EXTI -> NVIC chain wired with qdev GPIOs, with qdev_pass_gpios so the board can attach external parts (the DM163 via SPLIT_IRQ). This mirrors PORT -> EIC -> NVIC plus board LEDs/buttons.
(5) Unimplemented-device coverage of every block, careful errp handling, DEFINE_TYPES, three-phase reset, VMState with clocks, and trace-events.
(6) Five per-peripheral qtests as a model for SERCOM/PORT/EIC/GCLK tests.
(7) The maintainer status is 'Maintained', while MAX78000 is 'Orphan'.
Differences to apply: cortex-m0, num-irq 28/32, enable-bitband left false, and no flash alias (PIC32CM flash is at 0x0). Also add an alias for the PORT IOBUS at 0x60000000, use a rom_device for NVMCTRL, and select UNIMP in the SoC Kconfig.
   - ev: hw/arm/stm32l4x5_soc.c:136-164
   - ev: hw/arm/stm32l4x5_soc.c:166-213
   - ev: hw/arm/stm32l4x5_soc.c:255
   - ev: hw/arm/stm32l4x5_soc.c:264-300
   - ev: hw/arm/stm32l4x5_soc.c:361-435
   - ev: hw/arm/stm32l4x5_soc.c:449-494
   - ev: include/hw/arm/stm32l4x5_soc.h:37-72
   - ev: hw/misc/stm32l4x5_rcc.c:47-107
   - ev: hw/misc/stm32l4x5_rcc.c:1276-1290
   - ev: hw/arm/b-l475e-iot01a.c:73-111
   - ev: MAINTAINERS:1091-1102
   - ev: MAINTAINERS:904-910
   - ev: tests/qtest/meson.build:245-250
   - ev: PIC32CM-JH01/include/pic32cm1216jh01032.h:425-445
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:503-580

8. (high) [Other reference SoCs] No other in-tree Cortex-M SoC is a better template. max78000 (2025) is the newest but is Orphan, ignores several sysbus_realize() results, reuses one Error*, and its UART blocks on qemu_chr_fe_write_all. msf2 is Microchip's SmartFusion2 (M3); it derives refclk = m3clk/32 with the Clock API but uses a generic 16550 and an integer timer clock-frequency. stm32f405/f100 derive refclk = sysclk/8 while their peripherals take integer frequencies. mps2 has machine-level SYSCLK/REFCLK and CMSDK devices with Clock inputs, but no SoC object. ASPEED AST10x0/AST1040 (Cortex-M4, 2026) are tied to the ASPEED framework. stellaris has a guest-programmable SYSCLK but is a legacy board.
   detail: stellaris and netduino2 set ignore_memory_transaction_failures, which new boards must not do. msf2 shows that Microchip silicon (SmartFusion2 / Emcraft board) is already upstream, together with AVR ATmega boards and the PolarFire SoC.
   - ev: hw/arm/max78000_soc.c:62-65
   - ev: hw/arm/max78000_soc.c:97-147
   - ev: hw/char/max78000_uart.c:175-186
   - ev: MAINTAINERS:904-910
   - ev: hw/arm/msf2-soc.c:91-133
   - ev: hw/arm/msf2-soc.c:147-163
   - ev: hw/arm/stm32f405_soc.c:101-122
   - ev: hw/arm/stm32f405_soc.c:198
   - ev: hw/arm/stm32f100_soc.c:76-122
   - ev: hw/arm/mps2.c:152-157
   - ev: hw/arm/mps2.c:240-266
   - ev: hw/arm/mps2.c:377-395
   - ev: hw/arm/aspeed_ast1040.c:148-161
   - ev: hw/arm/stellaris.c:295-310
   - ev: hw/arm/stellaris.c:1131-1132
   - ev: hw/arm/stellaris.c:1422
   - ev: hw/arm/netduino2.c:65

9. (high) [LOAD-BEARING] [Unmapped/unmodelled addresses] New boards must leave mc->ignore_memory_transaction_failures unset. With it unset, an access to an unassigned address, or one a device rejects, becomes a synchronous external abort -> BusFault, which on v6-M (BusFault vector never enabled) escalates to HardFault. Every unmodelled peripheral range should be covered with create_unimplemented_device(name, base, size), which maps at priority -1000 so real models can overlay it; it is read-as-zero/writes-ignored and logs under -d unimp.
   - ev: include/hw/core/boards.h:225-233
   - ev: accel/tcg/cputlb.c:1291-1301
   - ev: target/arm/tcg/tlb_helper.c:316-354
   - ev: hw/intc/armv7m_nvic.c:185-188
   - ev: hw/intc/armv7m_nvic.c:612-650
   - ev: system/memory.c:1331-1340
   - ev: include/hw/misc/unimp.h:28-52
   - ev: hw/misc/unimp.c:21-49

10. (high) [LOAD-BEARING] [QOM registration and device life cycle] Declare types with OBJECT_DECLARE_SIMPLE_TYPE or OBJECT_DECLARE_TYPE in the header. Register them with 'static const TypeInfo[]' + DEFINE_TYPES, or with type_init + type_register_static, or with OBJECT_DEFINE_SIMPLE_TYPE. class_init takes (ObjectClass *, const void *data). instance_init cannot fail and must not have external side effects; it is where MMIO regions, IRQs, GPIOs, clocks, QOM properties and child objects are created. realize may fail through errp; it validates properties and clock wiring, installs chardev handlers, creates timers and creates size-dependent regions.
   detail: The generic qom-test and device-introspect-test walk every machine and every device type, so side effects in instance_init or crashing property getters break 'make check'. The docs require clocks to be created during init.
   - ev: include/qom/object.h:234
   - ev: include/qom/object.h:254
   - ev: include/qom/object.h:429
   - ev: include/qom/object.h:1006
   - ev: docs/devel/qom.rst:315-360
   - ev: include/hw/core/qdev.h:26-45
   - ev: hw/char/stm32l4x5_usart.c:544-555
   - ev: hw/char/stm32l4x5_usart.c:586-610
   - ev: hw/char/stm32l4x5_usart.c:633-657
   - ev: hw/char/max78000_uart.c:278-292
   - ev: docs/devel/clocks.rst:79-80
   - ev: tests/qtest/qom-test.c:221-251
   - ev: tests/qtest/device-introspect-test.c:39
   - ev: tests/qtest/device-introspect-test.c:160

11. (high) [Header and API renames (11.x)] All generic hw headers now live in include/hw/core/ (sysbus.h, irq.h, qdev.h, qdev-properties.h, qdev-clock.h, clock.h, boards.h, or-irq.h, split-irq.h, ptimer.h, registerfields.h, register.h); no *.h files remain directly under include/hw/. The memory API is in system/memory.h. class_init receives 'const void *data'. The chardev frontend struct is 'CharFrontend'; the old CharBackend name is gone from include/chardev/char-fe.h.
   detail: Tutorials written for older QEMU (hw/sysbus.h, exec/memory.h, CharBackend, DEFINE_PROP_END_OF_LIST) will not compile against master.
   - ev: include/hw/core/ (directory listing; no top-level include/hw/*.h)
   - ev: hw/char/stm32l4x5_usart.c:18-32
   - ev: hw/char/stm32l4x5_usart.c:600-601
   - ev: include/chardev/char-fe.h:10-25
   - ev: include/hw/core/register.h:14-16

12. (high) [LOAD-BEARING] [MemoryRegionOps semantics] MemoryRegionOps provides .read/.write (or *_with_attrs returning MemTxResult), .endianness, .valid (guest-visible size/alignment limits plus an optional .accepts hook) and .impl (sizes the callbacks implement). An access that violates .valid is logged under LOG_INVALID_MEM (-d invalid_mem), returns MEMTX_DECODE_ERROR and faults the CPU. An access narrower than .impl.min is widened at the same offset with no read-modify-write; a wider one is split.
   detail: Use DEVICE_LITTLE_ENDIAN. DEVICE_NATIVE_ENDIAN is a legacy API being compiled out target by target via TARGET_NOT_USING_LEGACY_NATIVE_ENDIAN_API; stm32l4x5_usart still uses it, while max78000_uart and nrf51_uart use LITTLE. A valid.max of 0 means 'all sizes valid'.
   - ev: include/system/memory.h:30-36
   - ev: include/system/memory.h:292-351
   - ev: system/memory.c:1342-1381
   - ev: system/memory.c:1421-1423
   - ev: system/memory.c:1470-1472
   - ev: system/memory.c:516-571
   - ev: include/qemu/log.h:40
   - ev: util/log.c:532-533
   - ev: configs/targets/alpha-softmmu.mak:4
   - ev: hw/char/stm32l4x5_usart.c:519-538
   - ev: hw/char/max78000_uart.c:204-210

13. (medium) [LOAD-BEARING] [Sub-word register access (SAM-style register maps)] QEMU neither realigns nor read-modify-writes accesses narrower than .impl.min_access_size: a 1-byte write at offset 0x16 with impl.min=4 reaches the callback as a 4-byte write at 0x16. The table-driven register API only matches exact register start addresses. PIC32CM/SAM register maps (8/16/32-bit registers, byte/halfword firmware accesses) therefore need impl.min_access_size=1 with byte-lane merging in the callback, or registers modelled at their exact ATDF widths.
   detail: stm32l4x5_usart sets valid.min=2 and impl.min=4, so a 16-bit access to the upper half of a register would hit its 'Bad offset' path. The docs' wording about 'naturally aligned accesses' is not what access_with_adjusted_size does ('FIXME: support unaligned access?').
   - ev: system/memory.c:558-571
   - ev: system/memory.c:475-494
   - ev: include/system/memory.h:337-350
   - ev: hw/core/register.c:179-205
   - ev: hw/char/stm32l4x5_usart.c:523-537
   - ev: hw/char/stm32l4x5_usart.c:449-452

14. (high) [LOAD-BEARING] [IRQ and GPIO idioms] Create IRQ outputs with sysbus_init_irq in init. After every state change, recompute level = (flags & enables) != 0 and call qemu_set_irq (or qemu_irq_raise/lower). GPIO inputs use qdev_init_gpio_in(dev, handler(opaque, line, level), n); outputs use qdev_init_gpio_out; named variants exist, and qdev_pass_gpios re-exports a child's lines on the SoC. One output may drive exactly one input (there is no guard), so use TYPE_SPLIT_IRQ for fan-out and TYPE_OR_IRQ for fan-in.
   detail: The max78000 one-liner (int_fl & int_en) maps directly onto SAM INTFLAG/INTENSET/INTENCLR. nrf51 treats level -1 as a floating (disconnected) input. EIC NMI can be wired to qdev_get_gpio_in_named(armv7m, "NMI", 0).
   - ev: hw/char/max78000_uart.c:29-35
   - ev: hw/char/max78000_uart.c:243
   - ev: hw/char/stm32l4x5_usart.c:172-196
   - ev: include/hw/core/irq.h:20-35
   - ev: hw/gpio/stm32l4x5_gpio.c:94-117
   - ev: hw/gpio/stm32l4x5_gpio.c:407-408
   - ev: hw/gpio/nrf51_gpio.c:245-259
   - ev: hw/gpio/nrf51_gpio.c:302-304
   - ev: include/hw/core/qdev.h:640-648
   - ev: hw/arm/stm32l4x5_soc.c:264-300
   - ev: hw/arm/b-l475e-iot01a.c:94-109
   - ev: hw/arm/mps2.c:392-395

15. (high) [LOAD-BEARING] [Clock API] A Clock is a QOM object that stores its period in units of 2^-32 ns (0 means gated). Create clocks in instance_init: qdev_init_clock_in(dev, name, callback, opaque, ClockPreUpdate|ClockUpdate) or qdev_init_clock_out. Connect them with qdev_connect_clock_in or clock_set_source; derive frequencies with clock_set_mul_div (and call clock_propagate yourself on runtime changes). A board creates root clocks with clock_new + clock_set_hz, a device enforces wiring with clock_has_source in realize, and frequency is read with clock_get_hz, clock_ticks_to_ns or clock_ns_to_ticks.
   detail: Clocks carry frequency only, not pin waveforms. stm32l4x5_rcc builds muxes and PLLs from Clock objects and exposes outputs via qdev_alias_clock. ptimer_set_period_from_clock ties a counter to a Clock. A 'qtest-clock-period' QOM property exists only under the qtest accelerator.
   - ev: docs/devel/clocks.rst:53-75
   - ev: docs/devel/clocks.rst:76-100
   - ev: docs/devel/clocks.rst:157-197
   - ev: docs/devel/clocks.rst:263-322
   - ev: docs/devel/clocks.rst:355-362
   - ev: include/hw/core/qdev-clock.h:33-94
   - ev: include/hw/core/clock.h:127-371
   - ev: hw/misc/stm32l4x5_rcc.c:47-107
   - ev: hw/misc/stm32l4x5_rcc.c:1276-1290
   - ev: hw/timer/cmsdk-apb-timer.c:207-246
   - ev: hw/arm/max78000fthr.c:25-30
   - ev: hw/arm/msf2-soc.c:114-115
   - ev: hw/core/clock.c:187-188

16. (high) [LOAD-BEARING] [Timers] Any timer that changes guest state must use QEMU_CLOCK_VIRTUAL, which stays deterministic under icount and replay. Two patterns exist. (a) A QEMUTimer (timer_new_ns + timer_mod) that schedules only the next event, with counter values computed lazily from virtual time (nrf51_timer, stm32f2xx_timer). (b) ptimer down-counters with explicit PTIMER_POLICY_* flags (never LEGACY in new code), changed inside begin/commit transactions and fed by ptimer_set_period_from_clock. ptimer itself runs on QEMU_CLOCK_VIRTUAL.
   - ev: include/qemu/timer.h:22-54
   - ev: include/qemu/timer.h:560
   - ev: include/qemu/timer.h:679
   - ev: hw/timer/stm32f2xx_timer.c:93
   - ev: hw/timer/stm32f2xx_timer.c:320
   - ev: hw/timer/nrf51_timer.c:28-83
   - ev: hw/timer/nrf51_timer.c:101-116
   - ev: hw/core/ptimer.c:465
   - ev: include/hw/core/ptimer.h:36-94
   - ev: hw/timer/cmsdk-apb-timer.c:238-246
   - ev: docs/devel/replay.rst:147-155

17. (high) [LOAD-BEARING] [UART chardev frontend] A UART model declares DEFINE_PROP_CHR("chardev") and calls qemu_chr_fe_set_handlers(&s->chr, can_receive, receive, event, NULL, s, NULL, true) in realize. can_receive returns the free RX slots, receive stores the bytes and updates flags and the IRQ, and qemu_chr_fe_accept_input is called once software drains RX. TX should be non-blocking: qemu_chr_fe_write, then retry through qemu_chr_fe_add_watch(G_IO_OUT|G_IO_HUP). max78000's qemu_chr_fe_write_all is flagged in its own source as blocking the thread.
   detail: The SoC binds the i-th '-serial' option with qdev_prop_set_chr(dev, "chardev", serial_hd(i)). Baud and framing go to the backend through CHR_IOCTL_SERIAL_SET_PARAMS, computed from the Clock input; line breaks arrive as CHR_EVENT_BREAK. None of these UARTs model byte time: TXE/TC are set immediately.
   - ev: include/chardev/char-fe.h:10-35
   - ev: include/chardev/char-fe.h:104-142
   - ev: include/chardev/char-fe.h:228-258
   - ev: hw/char/stm32l4x5_usart.c:198-284
   - ev: hw/char/stm32l4x5_usart.c:286-370
   - ev: hw/char/stm32l4x5_usart.c:439-445
   - ev: hw/char/stm32l4x5_usart.c:508-512
   - ev: hw/char/stm32l4x5_usart.c:541
   - ev: hw/char/stm32l4x5_usart.c:595-597
   - ev: hw/char/max78000_uart.c:20-52
   - ev: hw/char/max78000_uart.c:175-186
   - ev: hw/char/nrf51_uart.c:78-108
   - ev: hw/char/nrf51_uart.c:219-263
   - ev: hw/arm/stm32l4x5_soc.c:316-330

18. (high) [LOAD-BEARING] [Properties and runtime knobs] Property arrays are 'static const Property p[] = { DEFINE_PROP_...(...), };' with no terminator: DEFINE_PROP_END_OF_LIST no longer exists and device_class_set_props() rejects a terminator at compile time. Static properties cannot be set after realize. Anything a twin must change at run time through qom-set needs object_property_add(obj, name, type, getter, setter, ...), like stm32l4x5 GPIO's 'disconnected-pins' or ASPEED's per-pin bool 'gpioXN' properties.
   - ev: include/hw/core/qdev.h:905-931
   - ev: hw/core/qdev-properties.c:16-37
   - ev: hw/char/stm32l4x5_usart.c:540-542
   - ev: hw/char/stm32l4x5_usart.c:607
   - ev: hw/arm/nrf51_soc.c:211-224
   - ev: hw/gpio/stm32l4x5_gpio.c:232-247
   - ev: hw/gpio/stm32l4x5_gpio.c:412-416
   - ev: hw/gpio/aspeed_gpio.c:1483-1485

19. (high) [Reset] The preferred reset is the three-phase Resettable interface (rc->phases.enter/hold/exit). enter resets only local state and must not touch IRQs or other objects; hold and exit may. DeviceClass legacy_reset / device_class_set_legacy_reset is documented as deprecated, although older devices such as nrf51_uart still use it.
   - ev: docs/devel/reset.rst:108-149
   - ev: docs/devel/reset.rst:157-240
   - ev: include/hw/core/qdev.h:163-171
   - ev: include/hw/core/qdev.h:965-975
   - ev: hw/char/stm32l4x5_usart.c:372-388
   - ev: hw/char/stm32l4x5_usart.c:606
   - ev: hw/char/nrf51_uart.c:312
   - ev: hw/misc/stm32l4x5_rcc.c:110-151

20. (high) [VMState, tracing, logging] A VMStateDescription (dc->vmsd, with VMSTATE_CLOCK, VMSTATE_FIFO8, VMSTATE_PTIMER and post_load) is what makes savevm/loadvm and the snapshots used by replay and reverse debugging work. Trace points are declared per directory in trace-events and called as generated trace_*() functions; the default backend is 'log', with simple, ftrace, dtrace, syslog and ust also available. Guest programming errors go to qemu_log_mask(LOG_GUEST_ERROR, ...) and unmodelled features to LOG_UNIMP; enable them with -d guest_errors,unimp,invalid_mem.
   - ev: hw/char/stm32l4x5_usart.c:557-583
   - ev: hw/char/stm32l4x5_usart.c:609
   - ev: include/hw/core/clock.h:98
   - ev: include/qemu/fifo8.h:227
   - ev: include/hw/core/ptimer.h:302
   - ev: hw/char/trace-events:115-125
   - ev: hw/char/stm32l4x5_usart.c:32
   - ev: meson_options.txt:284-286
   - ev: meson.build:3585-3627
   - ev: include/qemu/log.h:26-27
   - ev: include/qemu/log.h:93
   - ev: util/log.c:513-533
   - ev: docs/system/replay.rst:194-240

21. (high) [LOAD-BEARING] [Build integration] A new board needs: (1) Kconfig: board 'bool / default y / depends on TCG && ARM / select <SOC>' (imply for optional parts); the SoC symbol selects ARM_V7M, OR_IRQ, UNIMP and each device symbol; device symbols are plain bool. (2) meson: arm_common_ss.add(when: 'CONFIG_X', if_true: files(...)) in hw/arm, and system_ss in the device directories. (3) An optional commented '# CONFIG_X=n' line in configs/devices/arm-softmmu/default.mak; the aarch64 config includes this file. (4) docs/system/arm/<board>.rst plus a toctree entry in target-arm.rst. (5) A MAINTAINERS section. (6) Tests registered in the qtest and functional meson files.
   detail: Latent gotcha: STM32L4X5_SOC and MAX78000_SOC call create_unimplemented_device but do not 'select UNIMP'; they build only because another board pulls UNIMP in. Select it explicitly. hw/arm is compiled once for both qemu-system-arm and qemu-system-aarch64 (arm_common_ss).
   - ev: hw/arm/Kconfig:280-285
   - ev: hw/arm/Kconfig:336-343
   - ev: hw/arm/Kconfig:385-400
   - ev: hw/arm/Kconfig:669-680
   - ev: hw/misc/Kconfig:186-187
   - ev: hw/char/Kconfig:51-58
   - ev: hw/arm/meson.build:1-38
   - ev: hw/arm/meson.build:95-121
   - ev: hw/char/meson.build:37
   - ev: hw/misc/meson.build:9
   - ev: hw/misc/meson.build:121-123
   - ev: configs/devices/arm-softmmu/default.mak
   - ev: configs/devices/aarch64-softmmu/default.mak:3-4
   - ev: docs/devel/kconfig.rst:206-240
   - ev: docs/system/target-arm.rst:71-102
   - ev: MAINTAINERS:834-840
   - ev: MAINTAINERS:1091-1102

22. (high) [LOAD-BEARING] [Out-of-tree devices / modules] Loadable out-of-tree peripheral modules are not a viable route; a fork or branch (or upstreaming) is required. Modules are disabled by default, cannot be built on Windows hosts or with static builds, and cover only a fixed in-tree list (hw-display qxl/virtio-gpu*, hw-usb, hw-s390x, hw-uefi, chardev baum/spice, accel-qtest). hw/arm SoC and board code is always built in, and module_load_qom resolves QOM types from a modinfo table generated at build time.
   - ev: meson_options.txt:54-57
   - ev: meson.build:259-264
   - ev: meson.build:3910-3990
   - ev: util/module.c:319-347
   - ev: hw/display/meson.build:58-148
   - ev: hw/usb/meson.build:95
   - ev: hw/s390x/meson.build:70
   - ev: hw/uefi/meson.build:20
   - ev: chardev/meson.build:55
   - ev: accel/qtest/meson.build:4
   - ev: hw/arm/meson.build:1-121

23. (high) [LOAD-BEARING] [Contribution policy and licence] Upstream QEMU currently declines contributions believed to include or derive from AI-generated content and names Claude explicitly. AGENTS.md lets agents help only with research, static analysis, debugging, local experiments not intended for upstream, and trivial changes. QEMU is GPLv2 (unlabelled files GPL-2.0-or-later), so a distributed modified binary carries GPL source obligations.
   detail: This affects the plan: any code meant for upstream must be written by humans. AI assistance is acceptable for internal prototypes and for understanding the APIs (as here).
   - ev: docs/devel/code-provenance.rst:288-349
   - ev: AGENTS.md:1-18
   - ev: LICENSE:1-20

24. (high) [LOAD-BEARING] [qtest (device-level tests)] libqtest starts QEMU with '-accel qtest', so no guest CPU executes. Tests reach MMIO through first_cpu's address space, so NVIC registers at 0xE000Exxx are visible. They intercept device GPIO lines (qtest_irq_intercept_in/out/_named + qtest_get_irq), force inputs (qtest_set_irq_in), advance virtual time (qtest_clock_step / qtest_clock_set, available only under the qtest accelerator), exchange UART bytes over sockets (qtest_init_with_serial) and use QMP (qom-get/set, qtest-clock-period). Tests are registered in tests/qtest/meson.build under qtests_arm, guarded by CONFIG_<SOC>, with optional slow_qtests timeouts.
   detail: stm32l4x5 has one test per peripheral plus a shared header (stm32l4x5.h) and checks NVIC ISPR for interrupt delivery. microbit-test uses set_irq_in with levels 0/1/-1 and clock_step for its timer test.
   - ev: tests/qtest/libqtest.c:453-480
   - ev: system/qtest.c:69-259
   - ev: system/qtest.c:523-570
   - ev: system/qtest.c:710
   - ev: system/qtest.c:747
   - ev: tests/qtest/libqtest.h:82
   - ev: tests/qtest/libqtest.h:135
   - ev: tests/qtest/libqtest.h:468-512
   - ev: tests/qtest/libqtest.h:748-770
   - ev: tests/qtest/stm32l4x5_usart-test.c:49-68
   - ev: tests/qtest/stm32l4x5_usart-test.c:200-269
   - ev: tests/qtest/stm32l4x5_gpio-test.c:124-157
   - ev: tests/qtest/stm32l4x5_gpio-test.c:270-305
   - ev: tests/qtest/stm32l4x5.h:28-41
   - ev: tests/qtest/microbit-test.c:335-436
   - ev: tests/qtest/meson.build:1-20
   - ev: tests/qtest/meson.build:245-276

25. (high) [Functional tests (firmware boot)] tests/functional/<arch>/test_*.py tests are QemuSystemTest classes. They boot real firmware fetched as Asset(url, sha256), set the machine, add '-kernel' or '-device loader', and assert console patterns with wait_for_console_pattern / exec_command_and_wait_for_pattern. MCU examples are microbit (a MicroPython .hex), max78000fthr, stellaris and emcraft_sf2, all listed as 'thorough' tests.
   detail: tests/qtest/boot-serial-test.c also embeds tiny hand-assembled vector tables (nRF51, STM32F100) and runs them under TCG, which makes a cheap smoke test for a new board.
   - ev: tests/functional/arm/test_microbit.py:13-28
   - ev: tests/functional/arm/test_max78000fthr.py:12-45
   - ev: tests/functional/arm/test_stellaris.py:11-44
   - ev: tests/functional/arm/meson.build:35-79
   - ev: docs/devel/testing/functional.rst:276-330
   - ev: tests/qtest/boot-serial-test.c:92-135
   - ev: tests/qtest/boot-serial-test.c:201-203
   - ev: tests/qtest/boot-serial-test.c:280-286

26. (high) [LOAD-BEARING] [Rust status] Rust is optional and off by default (configure sets rust=disabled; the meson 'rust' option defaults to disabled). It requires rustc >= 1.83.0, bindgen >= 0.60 and, for Rust builds, meson >= 1.12. Only two Rust devices exist, rust/hw/char/pl011 and rust/hw/timer/hpet, and they replace the C versions only when HAVE_RUST is set. The bindings support a sysbus MMIO peripheral (MemoryRegionOpsBuilder, init_mmio/init_irq), IRQs (InterruptSource), clocks (init_clock_in/out), timers (util::timer on CLOCK_VIRTUAL), chardev (CharFrontend.enable_handlers), VMState and reset phases. There is no ptimer binding, boards/SoCs cannot yet be written in Rust, and the chardev crate is missing from the docs' status table.
   detail: C SoCs that embed a Rust device's state struct need a padded C struct (pl011's static_assert / padding_for_rust). Recommendation: write the MCU twin in C.
   - ev: configure:251
   - ev: configure:743-745
   - ev: configure:1155-1161
   - ev: meson_options.txt:388-391
   - ev: meson.build:96-127
   - ev: Cargo.toml:1-14
   - ev: docs/devel/rust.rst:1-12
   - ev: docs/devel/rust.rst:74
   - ev: docs/devel/rust.rst:97-163
   - ev: rust/hw/char/pl011/src/device.rs:96-131
   - ev: rust/hw/char/pl011/src/device.rs:489-513
   - ev: rust/hw/char/pl011/src/device.rs:641-643
   - ev: rust/system/src/memory.rs:37-113
   - ev: rust/system/src/sysbus.rs:58-111
   - ev: rust/hw/core/src/qdev.rs:278-339
   - ev: rust/hw/core/src/qdev.rs:376-407
   - ev: rust/util/src/timer.rs:49-138
   - ev: rust/chardev/src/chardev.rs:40-193
   - ev: rust/hw/timer/hpet/src/device.rs:278-282
   - ev: hw/char/Kconfig:12-16
   - ev: hw/timer/Kconfig:12-17
   - ev: include/hw/char/pl011.h:60

27. (high) [LOAD-BEARING] [Time and determinism] Without icount, QEMU_CLOCK_VIRTUAL is host monotonic time while the VM runs and the CPU runs as fast as TCG allows, so the ratio of instructions to peripheral time depends on the host and is not deterministic. '-icount shift=N' defines virtual time as instructions x 2^N ns. N must be an integer from 0 to 10, so every instruction costs the same and only power-of-two ns per instruction are possible; it is explicitly not cycle-accurate. Adding sleep=off makes idle time jump to the next deadline, which is deterministic. align=on and shift=auto pace virtual time against the host instead. Record/replay (-icount ...,rr=record|replay,rrfile=...) logs non-deterministic inputs (serial chardev input, clocks, block, network) and supports gdb reverse-stepi/reverse-continue via snapshots; a diskless VM needs an orphan qcow2 to hold them.
   detail: icount is incompatible with multi-threaded TCG; that is irrelevant for a single-core M0+. The armv7m cpuclk frequency never changes the instruction rate. The only in-tree record/replay functional tests use A-profile boards (virt, cubieboard, vexpress-a9).
   - ev: include/qemu/timer.h:22-54
   - ev: system/cpu-timers.c:74-101
   - ev: docs/devel/tcg-icount.rst:10-33
   - ev: qemu-options.hx:5333-5382
   - ev: accel/tcg/icount-common.c:49
   - ev: accel/tcg/icount-common.c:420-472
   - ev: docs/system/replay.rst:13-30
   - ev: docs/system/replay.rst:69-96
   - ev: docs/system/replay.rst:140-160
   - ev: docs/system/replay.rst:186-240
   - ev: docs/devel/replay.rst:118-155
   - ev: chardev/char.c:680-691
   - ev: tests/functional/arm/test_replay.py:18-60
   - ev: hw/arm/armv7m.c:455-489

28. (medium) [LOAD-BEARING] [Implication for peripheral timing fidelity] Timing has to be modelled explicitly in each peripheral: existing UART models finish TX instantly, and timers derive counts from virtual time. For a deterministic twin whose rates are faithful, run with -icount shift=N,sleep=off and have every peripheral schedule QEMU_CLOCK_VIRTUAL events derived from its Clock input, for example byte time from SERCOM BAUD and GCLK frequency, or TC compare events from prescaler and GCLK.
   - ev: hw/char/stm32l4x5_usart.c:273-278
   - ev: hw/char/stm32l4x5_usart.c:508-512
   - ev: hw/char/max78000_uart.c:175-186
   - ev: hw/timer/nrf51_timer.c:43-83
   - ev: docs/devel/replay.rst:147-149
   - ev: qemu-options.hx:5346-5358

29. (high) [LOAD-BEARING] [Runtime control and observation already available] Existing channels are: QMP qom-get/qom-set/qom-list/qom-list-get on QOM paths such as /machine/soc/gpioa (custom setters only); HMP 'info mtree/qtree/qom-tree', 'xp' and 'trace-event'; the gdbstub (-s -S or -gdb) with the M-profile m-system register feature; chardev backends for UART bytes, whose input is recorded by replay; '-device' for off-chip I2C/SSI parts on named buses (sysbus devices are not user-creatable unless the machine allow-lists them); TCG plugins (API v7), which can detect MMIO accesses (qemu_plugin_hwaddr_is_io) and read/write memory and registers; and the qtest protocol socket. Memory and IRQ qtest commands also work while firmware runs under TCG (boot-serial-test does this), but clock_step only works under the qtest accelerator.
   detail: No GPIO/MCU device emits QMP events (qapi_event_send appears only in virtio-console under the hw directories checked), so outputs must be polled or traced. qtest IRQ interception replaces the device's handlers and is a test harness, not a stable API. Board-level wiring (b-l475e's DM163 on GPIO outputs via SPLIT_IRQ) is the in-tree way to attach 'external hardware' models.
   - ev: qom/qom-qmp-cmds.c:47
   - ev: qom/qom-qmp-cmds.c:105
   - ev: qom/qom-qmp-cmds.c:125-140
   - ev: tests/qtest/stm32l4x5_gpio-test.c:131-157
   - ev: hw/gpio/aspeed_gpio.c:1483-1485
   - ev: hmp-commands-info.hx:219
   - ev: hmp-commands-info.hx:536
   - ev: hmp-commands-info.hx:562
   - ev: hmp-commands.hx:301
   - ev: hmp-commands.hx:480
   - ev: hmp-commands.hx:1703-1717
   - ev: docs/system/gdb.rst:13-19
   - ev: target/arm/gdbstub.c:404-446
   - ev: hw/core/sysbus.c:300-311
   - ev: system/qdev-monitor.c:271-284
   - ev: tests/functional/aarch64/test_aspeed_ast2700fc.py:22
   - ev: include/plugins/qemu-plugin.h:68-91
   - ev: include/plugins/qemu-plugin.h:716-735
   - ev: include/plugins/qemu-plugin.h:1120-1293
   - ev: configure:244
   - ev: configure:1033-1054
   - ev: tests/qtest/boot-serial-test.c:280-286
   - ev: system/qtest.c:710
   - ev: system/qtest.c:747
   - ev: hw/arm/b-l475e-iot01a.c:88-110

30. (medium) [Debugger-safe peripheral reads] Memory reads from the gdbstub or monitor reach device MMIO callbacks with MemTxAttrs.debug=1; the Arm translate_for_debug path sets it. A device implementing .read_with_attrs can therefore skip side effects such as RX FIFO pops or clear-on-read flags for debugger accesses, which matters for an MPLAB-style SFR view. None of the reference UARTs do this; stm32l4x5's RDR read clears RXNE unconditionally.
   detail: cadence_uart shows the read_with_attrs signature and returns MEMTX_ERROR when the device is unclocked or in reset. The gdbstub source directory is not in the sparse clone, so the full gdb -> cpu_memory_rw_debug path was not traced end to end.
   - ev: include/exec/memattrs.h:47-48
   - ev: target/arm/ptw.c:4117
   - ev: system/physmem.c:4080-4120
   - ev: hw/core/cpu-system.c:60-80
   - ev: hw/char/cadence_uart.c:481-506
   - ev: hw/char/stm32l4x5_usart.c:439-445

31. (medium) [LOAD-BEARING] [No upstream co-simulation bridge] Upstream QEMU has no generic sysbus co-simulation bridge (no SystemC/TLM or remote-port; the grep hits are unrelated Xen code). Multi-process QEMU proxies only PCI devices and is aimed at KVM-enabled targets. An MCU twin that talks to external models must use chardev sockets, QMP, or a custom proxy device in the fork.
   - ev: docs/system/multi-process.rst:1-40
   - ev: hw/remote/ (directory listing: proxy.c, mpqemu-link.c, iohub.c)

32. (medium) [Windows-host constraints] On a Windows build host, modules are unavailable, TCG plugins need dlltool, the SocketCAN host CAN backend is Linux-only, and several qtests are skipped. All of these favour building and running the twin under Linux or WSL2 for CI.
   - ev: meson.build:259-261
   - ev: configure:1033-1036
   - ev: net/can/meson.build:3-4
   - ev: tests/qtest/meson.build:55-57
   - ev: tests/qtest/meson.build:369
   - ev: tests/qtest/libqtest.c:19-23

33. (high) [Missing Microchip-relevant models] There is no Bosch M_CAN model; the CAN controllers in the tree are sja1000, ctucan, flexcan and Xilinx CAN/CANFD. PIC32CM JH CAN0/CAN1 would need a new model or an unimplemented-device stub. No model exists for SAM-family SERCOM, PORT, EIC, GCLK, TC/TCC, EVSYS or NVMCTRL.
   - ev: hw/net/can/ (directory listing)
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:471-472
   - ev: net/can/meson.build:3-4

34. (medium) [Microchip silicon upstream and recent activity] The in-tree Microchip/Atmel silicon is AVR ATmega168/328/1280/2560 (Arduino boards), SmartFusion2 (Cortex-M3) and PolarFire SoC (RISC-V); there is no SAM, PIC32CM or PIC32CK model. On patchew, the only recent Microchip MCU series is a third-party RFC, 'hw/mips: add Microchip PIC32MK GPK/MCM board emulation' (posted around 2026-04-29), with no visible review. Searches for 'samd21' and 'pic32' returned nothing.
   - ev: hw/avr/atmega.c:461-477
   - ev: hw/avr/arduino.c:81-126
   - ev: hw/arm/msf2-soc.c:1-33
   - ev: MAINTAINERS:1240-1262
   - ev: MAINTAINERS:1315-1335
   - ev: MAINTAINERS:1795
   - ev: https://patchew.org/search?q=project%3AQEMU+microchip
   - ev: https://patchew.org/QEMU/20260429-pic32mk-board-support-v1-0-1dfac14f8e2d@voltumotor.com/
   - ev: https://patchew.org/search?q=project%3AQEMU+samd21

35. (high) [Patterns for DMA, buses, flash and IOBUS] Patterns exist for each of these. A DMA master takes a MemoryRegion link property and calls address_space_init + address_space_read/write (msf2-emac 'ahb-bus'). SPI and I2C controllers create buses with ssi_create_bus / i2c_init_bus, which can be aliased onto the SoC so off-chip devices plug in. Self-programmable flash is memory_region_init_rom_device, whose writes trap to a callback and are followed by memory_region_flush_rom_device; guest writes to a plain ROM region are silently discarded. The PIC32CM PORT IOBUS at 0x60000000 can be modelled as an alias of the PORT MMIO region, using the same memory_region_init_alias call STM32 uses for its flash alias.
   - ev: hw/net/msf2-emac.c:114-132
   - ev: hw/net/msf2-emac.c:525-529
   - ev: hw/net/msf2-emac.c:550
   - ev: hw/ssi/mss-spi.c:376-379
   - ev: hw/i2c/arm_sbcon_i2c.c:88
   - ev: hw/arm/msf2-soc.c:193-197
   - ev: hw/nvram/nrf51_nvm.c:285-306
   - ev: hw/nvram/nrf51_nvm.c:336-347
   - ev: accel/tcg/cputlb.c:1086-1087
   - ev: hw/arm/stm32l4x5_soc.c:180-185
   - ev: PIC32CM-JH01/include/pic32cm5164jh01100.h:503

36. (high) [SYSRESETREQ and NMI lines] armv7m exposes a named output 'SYSRESETREQ'. If it is left unwired, the NVIC calls qemu_system_reset_request(GUEST_RESET); wiring it to an RSTC model lets the model record RCAUSE before resetting. armv7m also exposes a named input 'NMI' that an EIC NMI line can drive, as mps2 does for its watchdog.
   - ev: include/hw/arm/armv7m.h:39-43
   - ev: hw/arm/armv7m.c:377-379
   - ev: hw/arm/mps2.c:392-395

## Table: Reference Cortex-M SoC models
| file | MCU | clock API? | unimplemented-device use | notable patterns | recommended? |
| hw/arm/stm32l4x5_soc.c + hw/arm/b-l475e-iot01a.c | ST STM32L475VG (Cortex-M4F), 2023-24; S: Maintained (MAINTAINERS:1091-1102) | Yes, full tree: an RCC device owns oscillators, PLLs and muxes and drives armv7m cpuclk ('cortex-fclk-out') and refclk ('cortex-refclk-out') plus each peripheral's 'clk' (stm32l4x5_soc.c:205-208, 229-230, 321-322) | Yes, about 50 blocks across APB/AHB (stm32l4x5_soc.c:361-435) | Abstract SoC with one subclass per flash size (449-494); careful errp handling; OR gates for shared lines (264-300); qdev_pass_gpios to board (255); serial_hd(i) per USART (320); board calls armv7m_load_kernel (b-l475e-iot01a.c:85-86) and fans GPIOs out to the DM163 via SPLIT_IRQ; 5 qtests | YES: primary template |
| hw/arm/nrf51_soc.c + hw/arm/microbit.c | Nordic nRF51822 (Cortex-M0 / Armv6-M), 2018 | Minimal: fixed 16 MHz sysclk set inside the SoC -> cpuclk; refclk intentionally unconnected (nrf51_soc.c:69-84) | Yes (174-177), plus an ad-hoc stub MemoryRegion for CLOCK (37-54, 169-172) | Only in-tree v6-M SoC: cortex-m0, num-irq 32 (186-191); SoC-owned container MR; rom_device NVM (hw/nvram/nrf51_nvm.c:336-347); qtest + MicroPython functional test + boot-serial mini-kernel; older style (type_init, legacy UART reset) | Partial: copy CPU/NVIC settings, NVM pattern and tests |
| hw/arm/max78000_soc.c + hw/arm/max78000fthr.c | ADI MAX78000 (Cortex-M4), 2025; S: Orphan (MAINTAINERS:904-910) | Board root SYSCLK via clock_new/clock_set_hz (fthr.c:25-26) -> SoC 'sysclk' -> cpuclk; no refclk; peripherals have no Clock inputs | Yes, extensive (max78000_soc.c:149-208) | Newest MCU board, but sysbus_realize() results go unchecked and one Error* is reused (108-147); UART TX blocks via qemu_chr_fe_write_all; functional test only, no qtests | No (reference only) |
| hw/arm/msf2-soc.c + hw/arm/msf2-som.c | Microchip (ex-Microsemi) SmartFusion2 M2S (Cortex-M3) | Board m3clk -> cpuclk; refclk = m3clk/32 via clock_set_mul_div + clock_set_source (msf2-soc.c:91-115); timer still uses an integer 'clock-frequency' (155-163) | Yes, 10 blocks (212-222) | Microchip silicon already upstream; eNVM ROM aliased at 0 (117-129); generic 16550 via serial_mm_init (147-153); SPI bus aliased onto the SoC (193-197); EMAC DMA through an 'ahb-bus' link; functional test | No (precedent only) |
| hw/arm/stm32f405_soc.c (+ olimex-stm32-h405.c, netduinoplus2.c) | ST STM32F405 (Cortex-M4) | sysclk from the board; refclk = sysclk/8 derived inside the SoC (101-122); peripherals take integer frequencies (timer 1 GHz, line 198) | Yes, many (257-298) | Classic layout; flash alias at 0; OR gate for ADC IRQs | No |
| hw/arm/stm32f100_soc.c + hw/arm/stm32vldiscovery.c | ST STM32F100 (Cortex-M3) | Same scheme as F405 (stm32f100_soc.c:62-97, 121-122) | Yes (152 onward) | Small and readable; boot-serial-test mini-kernel (boot-serial-test.c:110-135) | No |
| hw/arm/mps2.c | Arm MPS2 FPGA images AN385/386/500/511 (M3/M4/M7) | Machine-level SYSCLK 25 MHz + REFCLK via clock_new (152-157, 240-241); CMSDK timer/dualtimer/watchdog take Clock inputs (377, 385, 392) | Yes, large background ranges with real devices overlaid (248-266) | No separate SoC object; -cpu selectable (242); watchdog wired to NMI (395); CMSDK ptimer + Clock devices | No (source of timer patterns) |
| hw/arm/aspeed_ast10x0.c / hw/arm/aspeed_ast1040.c | ASPEED AST1030/AST1040 BMC MCUs (Cortex-M4); AST1040 is dated 2026 | Board SYSCLK -> SoC sysclk -> cpuclk (aspeed_ast1040.c:148-158) | Via the aspeed_mmio_map_unimplemented helper (271-287) | Tied to the shared ASPEED SoC framework (memmap/irqmap tables) | No |
| hw/arm/stellaris.c | TI LM3S6965/LM3S811 (Cortex-M3) | Guest-programmable SYSCLK clock output from the sysctl device (295-310, 487, 1131-1132) | Some (1391-1396) | Legacy: sets ignore_memory_transaction_failures (1422, 1442) | No |

## Table: Peripheral-model API cheat sheet
| concern | API/idiom | example file:line |
| Type declaration | OBJECT_DECLARE_SIMPLE_TYPE(State, NAME) or OBJECT_DECLARE_TYPE(State, Class, NAME) in the header; the struct starts with 'SysBusDevice parent_obj;' | include/hw/arm/max78000_soc.h:21-22,33-34; include/hw/arm/stm32l4x5_soc.h:37-41,48-72 |
| Type registration | static const TypeInfo arr[] + DEFINE_TYPES(arr) (abstract base + subclasses); or type_init + type_register_static; or OBJECT_DEFINE_SIMPLE_TYPE | hw/char/stm32l4x5_usart.c:633-657; hw/char/max78000_uart.c:278-292; docs/devel/qom.rst:347-360 |
| class_init | void x_class_init(ObjectClass *klass, const void *data): dc->realize, dc->vmsd, device_class_set_props, rc->phases.* | hw/char/stm32l4x5_usart.c:600-610 |
| instance_init (cannot fail, no external side effects) | memory_region_init_io + sysbus_init_mmio; sysbus_init_irq; qdev_init_gpio_in/out; qdev_init_clock_in/out; object_property_add; object_initialize_child | hw/char/stm32l4x5_usart.c:544-555; hw/gpio/stm32l4x5_gpio.c:398-417; include/hw/core/qdev.h:26-45 |
| realize (may fail via errp) | Validate properties; clock_has_source; qemu_chr_fe_set_handlers; create QEMUTimer/ptimer; create size-dependent regions (rom_device) | hw/char/stm32l4x5_usart.c:586-598; hw/timer/cmsdk-apb-timer.c:229-247; hw/nvram/nrf51_nvm.c:336-347 |
| MMIO callbacks | uint64_t read(void *opaque, hwaddr off, unsigned size) / void write(void *opaque, hwaddr off, uint64_t val, unsigned size), or *_with_attrs returning MemTxResult | include/system/memory.h:292-314; hw/char/stm32l4x5_usart.c:403-517 |
| MMIO constraints | endianness = DEVICE_LITTLE_ENDIAN. valid.{min,max} violation -> LOG_INVALID_MEM + MEMTX_DECODE_ERROR -> fault. Accesses below impl.min are widened at the same offset with no RMW; above impl.max they are split. valid.accepts hook available | hw/char/max78000_uart.c:204-210; system/memory.c:1342-1381,538-571; include/system/memory.h:316-351 |
| Register fields | REG32(R, off) + FIELD(R, F, shift, len) -> A_R, R_R_F_MASK; FIELD_EX32/FIELD_DP32 | hw/char/stm32l4x5_usart.c:35-56,216; include/hw/core/registerfields.h:21,42 |
| Table-driven registers (generator-friendly) | RegisterAccessInfo{name, addr, reset, ro, w1c, cor, rsvd, unimp, pre_write, post_write, post_read} + register_init_block32; matches exact register start addresses only | include/hw/core/register.h:44-58; hw/core/register.c:179-205 |
| IRQ output | sysbus_init_irq(sbd, &s->irq); qemu_set_irq(s->irq, (flags & enables) != 0) after every state change | hw/char/max78000_uart.c:29-35,243; hw/char/stm32l4x5_usart.c:172-196; include/hw/core/irq.h:20-35 |
| GPIO lines | qdev_init_gpio_in(dev, handler(opaque,line,level), n); qdev_init_gpio_out(dev, pins, n); *_named variants; qdev_pass_gpios | hw/gpio/stm32l4x5_gpio.c:94-117,407-408; hw/gpio/nrf51_gpio.c:245-259,302-304; include/hw/core/qdev.h:747-841 |
| Wire to NVIC | sysbus_connect_irq(sbd, n, qdev_get_gpio_in(armv7m, irq)); NMI: qdev_get_gpio_in_named(armv7m, "NMI", 0); named SYSRESETREQ output | hw/arm/stm32l4x5_soc.c:314; hw/arm/mps2.c:392-395; hw/arm/armv7m.c:377-379 |
| Shared or fanned lines | One output to exactly one input; fan-in with TYPE_OR_IRQ ('num-lines'), fan-out with TYPE_SPLIT_IRQ | include/hw/core/qdev.h:640-648; hw/arm/stm32l4x5_soc.c:264-300; hw/arm/b-l475e-iot01a.c:94-109 |
| Clock input | qdev_init_clock_in(dev, "clk", cb, s, ClockUpdate); clock_has_source in realize; clock_get_hz / clock_ticks_to_ns / clock_ns_to_ticks; cb(opaque, ClockEvent) | hw/timer/cmsdk-apb-timer.c:207-235; hw/char/stm32l4x5_usart.c:360,554,590; include/hw/core/clock.h:166-305 |
| Clock tree / clock controller | Root: clock_new + clock_set_hz. Derived: clock_set_source + clock_set_mul_div (+ clock_propagate). Outputs: qdev_init_clock_out + clock_update_hz. Export: qdev_alias_clock. Wire: qdev_connect_clock_in(dev, name, qdev_get_clock_out(src, name)) | hw/arm/max78000fthr.c:25-30; hw/arm/msf2-soc.c:114-115; hw/misc/stm32l4x5_rcc.c:47-107,1276-1290; hw/arm/stm32l4x5_soc.c:205-208 |
| Event timer | timer_new_ns(QEMU_CLOCK_VIRTUAL, cb, s); timer_mod(t, qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) + delta); counters computed lazily from virtual time | hw/timer/stm32f2xx_timer.c:93,320; hw/timer/nrf51_timer.c:43-83; include/qemu/timer.h:560,626,679 |
| Down-counter | ptimer_init(cb, s, PTIMER_POLICY_* flags, never LEGACY); changes inside ptimer_transaction_begin/commit; ptimer_set_period_from_clock | hw/timer/cmsdk-apb-timer.c:207-214,238-246; include/hw/core/ptimer.h:36-94,128-195 |
| UART RX | DEFINE_PROP_CHR("chardev"); qemu_chr_fe_set_handlers(&s->chr, can_receive, receive, event, NULL, s, NULL, true); qemu_chr_fe_accept_input once RX drains | hw/char/stm32l4x5_usart.c:198-236,439-445,541,595-597; include/chardev/char-fe.h:104-142 |
| UART TX | qemu_chr_fe_write + qemu_chr_fe_add_watch(G_IO_OUT|G_IO_HUP) retry; avoid qemu_chr_fe_write_all in the MMIO path | hw/char/stm32l4x5_usart.c:242-284; hw/char/nrf51_uart.c:78-108; hw/char/max78000_uart.c:175-186 |
| UART params / events / SoC binding | qemu_chr_fe_ioctl(CHR_IOCTL_SERIAL_SET_PARAMS); CHR_EVENT_BREAK; SoC: qdev_prop_set_chr(dev, "chardev", serial_hd(i)) | hw/char/stm32l4x5_usart.c:286-370; hw/char/nrf51_uart.c:246-255; hw/arm/stm32l4x5_soc.c:316-330 |
| Static properties | static const Property p[] = {DEFINE_PROP_UINT32/BOOL/LINK/CHR/STRING...} with no END_OF_LIST; device_class_set_props; immutable after realize | hw/arm/nrf51_soc.c:211-224; include/hw/core/qdev.h:905-931; hw/core/qdev-properties.c:31-37 |
| Runtime knobs (qom-set after realize) | object_property_add(obj, name, type, get, set, NULL, opaque) in instance_init | hw/gpio/stm32l4x5_gpio.c:232-247,412-416; hw/gpio/aspeed_gpio.c:1483-1485 |
| Reset | RESETTABLE_CLASS(klass)->phases.enter/hold/exit (IRQ changes only in hold/exit); device_class_set_legacy_reset is deprecated | hw/char/stm32l4x5_usart.c:372-388,606; docs/devel/reset.rst:108-133; include/hw/core/qdev.h:163-171,965-975 |
| Migration / snapshot state | VMStateDescription with VMSTATE_UINT32 / VMSTATE_CLOCK / VMSTATE_FIFO8 / VMSTATE_PTIMER; dc->vmsd; post_load | hw/char/stm32l4x5_usart.c:557-583; hw/char/max78000_uart.c:216-236; include/hw/core/clock.h:98 |
| Tracing | Declare in hw/<dir>/trace-events; #include "trace.h"; trace_<event>(); run with -trace '<pattern>' | hw/char/trace-events:115-125; hw/char/stm32l4x5_usart.c:32,466; meson_options.txt:284-286 |
| Guest-error / unimplemented logging | qemu_log_mask(LOG_GUEST_ERROR | LOG_UNIMP, ...); run with -d guest_errors,unimp,invalid_mem [-D file] | hw/char/stm32l4x5_usart.c:312-315,450-451; include/qemu/log.h:26-27,40,93; util/log.c:513-533 |
| Unmodelled blocks | create_unimplemented_device(name, base, size): priority -1000, read-as-zero/writes-ignored, logs LOG_UNIMP | include/hw/misc/unimp.h:16-52; hw/misc/unimp.c:21-49 |
| Self-programmable flash | memory_region_init_rom_device(mr, owner, &ops, s, name, size, errp); memory_region_flush_rom_device after modifying storage | hw/nvram/nrf51_nvm.c:285-306,336-347 |
| DMA master | DEFINE_PROP_LINK(..., TYPE_MEMORY_REGION, MemoryRegion *) + address_space_init + address_space_read/write | hw/net/msf2-emac.c:114-132,525-529,550 |
| Off-chip buses | ssi_create_bus(dev, "spi") / i2c_init_bus(dev, "i2c"); attach parts with -device x,bus=...,address=... | hw/ssi/mss-spi.c:376-379; hw/i2c/arm_sbcon_i2c.c:88; tests/functional/aarch64/test_aspeed_ast2700fc.py:22 |
| Debugger-safe reads | Implement .read_with_attrs and skip side effects when attrs.debug (set for gdbstub/monitor accesses) | include/exec/memattrs.h:47-48; target/arm/ptw.c:4117; hw/char/cadence_uart.c:481-506 |

## Table: Build-integration touch points for a new Microchip board
| file | what to add | in-tree example |
| hw/arm/Kconfig | Board: bool / default y / depends on TCG && ARM / select <SOC> (imply optional parts). SoC: bool / select ARM_V7M, OR_IRQ, SPLIT_IRQ (if used), UNIMP and each device symbol | hw/arm/Kconfig:385-400, 669-680, 280-285 |
| hw/{char,misc,gpio,timer,nvram}/Kconfig | config <DEVICE>: bool (selected by the SoC) | hw/char/Kconfig:51-58; hw/misc/Kconfig:186-187 |
| hw/arm/meson.build | arm_common_ss.add(when: 'CONFIG_<SOC>', if_true: files('<soc>.c')), same for the board file | hw/arm/meson.build:37-38, 95, 104 |
| hw/<cat>/meson.build | system_ss.add(when: 'CONFIG_<DEVICE>', if_true: files('<dev>.c')) | hw/char/meson.build:37; hw/misc/meson.build:121-123 |
| hw/<cat>/trace-events | Trace event declarations for each new device (these directories are already in trace_events_subdirs) | hw/char/trace-events:115-125; meson.build:3585-3627 |
| include/hw/<cat>/*.h | State structs + OBJECT_DECLARE_* for the SoC, its class (variant fields) and devices | include/hw/arm/stm32l4x5_soc.h:37-72 |
| configs/devices/arm-softmmu/default.mak | Optional commented '# CONFIG_<BOARD>=n' (boards are on by default via Kconfig); the aarch64 config includes this file | configs/devices/arm-softmmu/default.mak; configs/devices/aarch64-softmmu/default.mak:3-4 |
| docs/system/arm/<board>.rst + docs/system/target-arm.rst | Board doc (supported and missing devices, boot example) + toctree entry | docs/system/arm/b-l475e-iot01a.rst; docs/system/arm/max78000.rst; docs/system/target-arm.rst:71-102 |
| MAINTAINERS | Section listing the new hw/, include/, tests/ and docs files | MAINTAINERS:834-840, 1091-1102 |
| tests/qtest/meson.build + tests/qtest/<soc>_*-test.c | qtests_arm += [...] guarded by CONFIG_<SOC>; slow_qtests entry if needed | tests/qtest/meson.build:1-20, 245-276 |
| tests/functional/arm/meson.build + test_<board>.py | Add to tests_arm_system_thorough; firmware Asset pinned by SHA-256 | tests/functional/arm/meson.build:35-79; tests/functional/arm/test_max78000fthr.py |

## Table: PIC32CM JH01 (DFP 1.3.221) vs QEMU master Armv6-M support
| aspect | PIC32CM JH01 | QEMU master | action |
| Core | Cortex-M0+ (core_cm0plus.h; pic32cm5164jh01100.h:238-247) | Only 'cortex-m0', CPUID 0x410cc200 (target/arm/tcg/cpu-v7m.c:45-75,325-339) | Use cortex-m0; add a cortex-m0plus type only if CPUID, nPRIV or MPU matter |
| External IRQs | 28 (pic32cm1216jh01032.h:110) or 32 (pic32cm5164jh01100.h:127) | v6-M NVIC max 32 (armv7m_nvic.c:2758-2761) | num-irq per SoC subclass |
| Priority bits | 2 (pic32cm5164jh01100.h:240) | v6-M default 2 (armv7m_nvic.c:2768-2773) | Default |
| VTOR | Present (pic32cm5164jh01100.h:241) | Implemented unconditionally (armv7m_nvic.c:1109-1110,1638-1640) | None |
| MPU | Present (pic32cm5164jh01100.h:239) | cortex-m0 has 0 regions by default; armv7m mpu-ns-regions -> pmsav7-dregion (armv7m.c:349-355; target/arm/cpu.c:1667-1676) | Try mpu-ns-regions=8 and verify (untested) |
| Unprivileged (CONTROL.nPRIV) | Expected alongside the MPU on Armv6-M (Arm architecture background; not verified locally) | Not implemented on cortex-m0 (target/arm/cpu.c:1470-1471; m_helper.c:2778-2781) | target/arm patch if an RTOS uses unprivileged threads |
| Bit-banding | None on M0+ | armv7m enable-bitband defaults to false (armv7m.c:560) | Leave false |
| Flash / DATAFLASH | 512 KB at 0x0 and 8 KB at 0x00400000 on the 5164 part (pic32cm5164jh01100.h:537-575) | ROM or rom_device; writes to plain ROM are discarded (accel/tcg/cputlb.c:1086-1087) | rom_device + NVMCTRL model; no alias needed |
| SRAM | 64 KB at 0x20000000 (pic32cm5164jh01100.h:557,576) | memory_region_init_ram | sram_size as an SoC class field |
| Peripheral bridges | HPB0-3 at 0x40000000/0x41000000/0x42000000/0x43000000 (pic32cm5164jh01100.h:577-580) | create_unimplemented_device background (unimp.h:41-52) | Unimplemented device per bridge, real models overlaid |
| PORT IOBUS | 0x60000000 (pic32cm5164jh01100.h:503) | Alias pattern (stm32l4x5_soc.c:180-185) | Alias the PORT MMIO region at 0x60000000 |
| Shared vectors | IRQ0, 9, 10, 20-22 (pic32cm5164jh01100.h:84-116) | TYPE_OR_IRQ (qdev.h:640-648) | OR gates in SoC realize |
| CAN0/CAN1 (M_CAN) | 0x42001c00 / 0x42002000 (pic32cm5164jh01100.h:471-472) | No M_CAN model; SocketCAN backend is Linux-only (net/can/meson.build:3-4) | New model or unimplemented stub |

## Table: Time and determinism modes
| mode | QEMU_CLOCK_VIRTUAL meaning | deterministic? | fidelity notes | evidence |
| Default TCG (no -icount) | Host monotonic time while the VM runs | No | Instruction vs peripheral-time ratio depends on host speed and load | system/cpu-timers.c:74-101; include/qemu/timer.h:22-31 |
| -icount shift=N | Instruction count x 2^N ns (integer N from 0 to 10) | Yes when there is no external input, but sleep=on (the default) lets idle time track real time | Every instruction costs the same; power-of-two ns per instruction only; not cycle-accurate | qemu-options.hx:5340-5358; accel/tcg/icount-common.c:49,439-461; docs/devel/tcg-icount.rst:10-33 |
| -icount shift=N,sleep=off | Idle (WFI) time jumps to the next deadline | Yes | Fast-forwards idle; cannot be combined with shift=auto or align=on | qemu-options.hx:5352-5360 |
| -icount shift=auto or align=on | Kept near host time | No | Pacing against real-world I/O | qemu-options.hx:5341-5344,5362-5372 |
| -icount ...,rr=record|replay,rrfile=f[,rrsnapshot=s] | icount plus logged non-deterministic inputs | Replayable; gdb reverse-stepi/reverse-continue | Devices must use the virtual clock and replay-safe inputs; no Cortex-M replay test in tree | docs/system/replay.rst:13-30,140-160,186-240; docs/devel/replay.rst:118-155; tests/functional/arm/test_replay.py:18-60 |
| qtest accelerator (tests) | Advanced only by clock_step/clock_set | Fully controlled | No guest CPU executes | system/qtest.c:82-114,710,747; tests/qtest/libqtest.c:453-480 |

## Table: Runtime control and observation channels
| channel | control/observe | usable while firmware runs under TCG? | notes | evidence |
| QMP qom-get/qom-set/qom-list/qom-list-get | Both | Yes | Only custom (object_property_add) properties can be set after realize; no pin-change events | qom/qom-qmp-cmds.c:47,105,125,140; hw/core/qdev-properties.c:31-37 |
| HMP info mtree/qtree/qom-tree, xp, trace-event | Observe (plus some control) | Yes | Check the memory map and device tree | hmp-commands-info.hx:219,536,562; hmp-commands.hx:301,480,1703,1717 |
| chardev backends (-serial / -chardev socket, pipe, file) | Both (UART bytes) | Yes | Input is recorded by record/replay; natural transport for a twin protocol | hw/arm/stm32l4x5_soc.c:316-330; chardev/char.c:680-691 |
| gdbstub (-s -S / -gdb) | Both (CPU and memory) | Yes | M-profile XML with m-system registers; debugger MMIO reads carry attrs.debug=1 | docs/system/gdb.rst:13-19; target/arm/gdbstub.c:404-446; target/arm/ptw.c:4117 |
| qtest protocol (-qtest) | Both (memory, IRQ intercept, set_irq_in) | Memory/IRQ commands yes; clock_step only under the qtest accelerator | Test harness: intercept replaces device handlers; no API stability promise | system/qtest.c:69-259,710,747; tests/qtest/boot-serial-test.c:280-286 |
| -device on existing buses | Compose off-chip parts at startup | N/A (startup) | Sysbus devices are not user-creatable unless the machine allow-lists them | hw/core/sysbus.c:300-311; system/qdev-monitor.c:271-284; tests/functional/aarch64/test_aspeed_ast2700fc.py:22 |
| Tracing (-trace) | Observe | Yes | Backends: log (default), simple, ftrace, dtrace, syslog, ust | meson_options.txt:284-286; hw/char/trace-events:115-125 |
| TCG plugins (API v7) | Observe (plus memory/register writes) | Yes | qemu_plugin_hwaddr_is_io identifies MMIO; on by default where supported; Windows builds need dlltool | include/plugins/qemu-plugin.h:68-91,716-735,1120-1293; configure:244,1033-1054 |

## Code: SoC realize: armv7m created and clocked from the RCC clock tree (hw/arm/stm32l4x5_soc.c:199-213)
```c
    object_initialize_child(OBJECT(dev_soc), "armv7m", &s->armv7m, TYPE_ARMV7M);
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 96);
    qdev_prop_set_uint32(armv7m, "num-prio-bits", 4);
    qdev_prop_set_string(armv7m, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m4"));
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk",
        qdev_get_clock_out(DEVICE(&(s->rcc)), "cortex-fclk-out"));
    qdev_connect_clock_in(armv7m, "refclk",
        qdev_get_clock_out(DEVICE(&(s->rcc)), "cortex-refclk-out"));
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(system_memory), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }
```

## Code: Board init: root clock, SoC, firmware load (hw/arm/max78000fthr.c:20-36)
```c
static void max78000_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;

    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);

    dev = qdev_new(TYPE_MAX78000_SOC);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0x00000000, FLASH_SIZE);
}
```

## Code: Only in-tree Armv6-M SoC: cortex-m0 with 32 IRQs (hw/arm/nrf51_soc.c:186-191)
```c
    memory_region_init(&s->container, obj, "nrf51-container", UINT64_MAX);

    object_initialize_child(OBJECT(s), "armv6m", &s->armv7m, TYPE_ARMV7M);
    qdev_prop_set_string(DEVICE(&s->armv7m), "cpu-type",
                         ARM_CPU_TYPE_NAME("cortex-m0"));
    qdev_prop_set_uint32(DEVICE(&s->armv7m), "num-irq", 32);
```

## Code: NVIC limits for Armv6-M (Cortex-M0/M0+) (hw/intc/armv7m_nvic.c:2758-2773)
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

## Code: armv7m_load_kernel: ELF/raw load plus the mandatory CPU reset hook (hw/arm/armv7m.c:620-643)
```c
    if (kernel_filename) {
        image_size = load_elf_as(kernel_filename, NULL, NULL, NULL,
                                 &entry, NULL, NULL,
                                 NULL, ELFDATA2LSB, EM_ARM, 1, 0, as);
        if (image_size < 0) {
            image_size = load_image_targphys_as(kernel_filename, mem_base,
                                                mem_size, as, NULL);
        }
        if (image_size < 0) {
            error_report("Could not load kernel '%s'", kernel_filename);
            exit(1);
        }
    }

    /* CPU objects (unlike devices) are not automatically reset on system
     * reset, so we must always register a handler to do so. Unlike
     * A-profile CPUs, we don't need to do anything special in the
     * handler to arrange that it starts correctly.
     * This is arguably the wrong place to do this, but it matches the
     * way A-profile does it. Note that this means that every M profile
     * board must call this function!
     */
    qemu_register_reset(armv7m_reset, cpu);
}
```

## Code: UART chardev receive path (flags + IRQ update) (hw/char/stm32l4x5_usart.c:209-236)
```c
static void stm32l4x5_usart_base_receive(void *opaque, const uint8_t *buf,
                                         int size)
{
    Stm32l4x5UsartBaseState *s = opaque;

    if (!((s->cr1 & R_CR1_UE_MASK) && (s->cr1 & R_CR1_RE_MASK))) {
        trace_stm32l4x5_usart_receiver_not_enabled(
            FIELD_EX32(s->cr1, CR1, UE), FIELD_EX32(s->cr1, CR1, RE));
        return;
    }

    /* Check if overrun detection is enabled and if there is an overrun */
    if (!(s->cr3 & R_CR3_OVRDIS_MASK) && (s->isr & R_ISR_RXNE_MASK)) {
        /*
         * A character has been received while
         * the previous has not been read = Overrun.
         */
        s->isr |= R_ISR_ORE_MASK;
        trace_stm32l4x5_usart_overrun_detected(s->rdr, *buf);
    } else {
        /* No overrun */
        s->rdr = *buf;
        s->isr |= R_ISR_RXNE_MASK;
        trace_stm32l4x5_usart_rx(s->rdr);
    }

    stm32l4x5_update_irq(s);
}
```

## Code: Non-blocking UART transmit with chardev watch (hw/char/stm32l4x5_usart.c:252-279)
```c
    if (!(s->cr1 & R_CR1_TE_MASK) || (s->isr & R_ISR_TXE_MASK)) {
        return G_SOURCE_REMOVE;
    }

    ret = qemu_chr_fe_write(&s->chr, &ch, 1);
    if (ret <= 0) {
        s->watch_tag = qemu_chr_fe_add_watch(&s->chr, G_IO_OUT | G_IO_HUP,
                                             usart_transmit, s);
        if (!s->watch_tag) {
            /*
             * Most common reason to be here is "no chardev backend":
             * just insta-drain the buffer, so the serial output
             * goes into a void, rather than blocking the guest.
             */
            goto buffer_drained;
        }
        /* Transmit pending */
        trace_stm32l4x5_usart_tx_pending();
        return G_SOURCE_REMOVE;
    }

buffer_drained:
    /* Character successfully sent */
    trace_stm32l4x5_usart_tx(ch);
    s->isr |= R_ISR_TC_MASK | R_ISR_TXE_MASK;
    stm32l4x5_update_irq(s);
    return G_SOURCE_REMOVE;
}
```

## Code: MemoryRegionOps with guest-visible (.valid) vs implemented (.impl) sizes (hw/char/stm32l4x5_usart.c:519-538)
```c
static const MemoryRegionOps stm32l4x5_usart_base_ops = {
    .read = stm32l4x5_usart_base_read,
    .write = stm32l4x5_usart_base_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .max_access_size = 4,
        /*
         * Official STM32CubeL4 drivers use and require support for 16-bit
         * writes to UART registers for proper function despite datasheet
         * claims.
         */
        .min_access_size = 2,
        .unaligned = false
    },
    .impl = {
        .max_access_size = 4,
        .min_access_size = 4,
        .unaligned = false
    },
};
```

## Code: IRQ-update idiom (flags & enables) (hw/char/max78000_uart.c:29-35)
```c
static void max78000_update_irq(Max78000UartState *s)
{
    int interrupt_level;

    interrupt_level = s->int_fl & s->int_en;
    qemu_set_irq(s->irq, interrupt_level);
}
```

## Code: Properties (no END_OF_LIST) and instance_init (MMIO, IRQ, clock) (hw/char/stm32l4x5_usart.c:540-555)
```c
static const Property stm32l4x5_usart_base_properties[] = {
    DEFINE_PROP_CHR("chardev", Stm32l4x5UsartBaseState, chr),
};

static void stm32l4x5_usart_base_init(Object *obj)
{
    Stm32l4x5UsartBaseState *s = STM32L4X5_USART_BASE(obj);

    sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq);

    memory_region_init_io(&s->mmio, obj, &stm32l4x5_usart_base_ops, s,
                          TYPE_STM32L4X5_USART_BASE, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    s->clk = qdev_init_clock_in(DEVICE(s), "clk", NULL, s, 0);
}
```

## Code: realize (clock check + chardev handlers) and class_init (3-phase reset, props, vmsd) (hw/char/stm32l4x5_usart.c:586-610)
```c
static void stm32l4x5_usart_base_realize(DeviceState *dev, Error **errp)
{
    ERRP_GUARD();
    Stm32l4x5UsartBaseState *s = STM32L4X5_USART_BASE(dev);
    if (!clock_has_source(s->clk)) {
        error_setg(errp, "USART clock must be wired up by SoC code");
        return;
    }

    qemu_chr_fe_set_handlers(&s->chr, stm32l4x5_usart_base_can_receive,
                             stm32l4x5_usart_base_receive, NULL, NULL,
                             s, NULL, true);
}

static void stm32l4x5_usart_base_class_init(ObjectClass *klass,
                                            const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    ResettableClass *rc = RESETTABLE_CLASS(klass);

    rc->phases.hold = stm32l4x5_usart_base_reset_hold;
    device_class_set_props(dc, stm32l4x5_usart_base_properties);
    dc->realize = stm32l4x5_usart_base_realize;
    dc->vmsd = &vmstate_stm32l4x5_usart_base;
}
```

## Code: What happens on an access size outside .valid (system/memory.c:1366-1379)
```c
    if (!mr->ops->valid.max_access_size) {
        return true;
    }

    if (size > mr->ops->valid.max_access_size
        || size < mr->ops->valid.min_access_size) {
        qemu_log_mask(LOG_INVALID_MEM, "Invalid %s at addr 0x%" HWADDR_PRIX
                      ", size %u, region '%s', reason: invalid size "
                      "(min:%u max:%u)\n",
                      is_write ? "write" : "read",
                      addr, size, memory_region_name(mr),
                      mr->ops->valid.min_access_size,
                      mr->ops->valid.max_access_size);
        return false;
```

## Code: Access widening to .impl sizes (same offset, no RMW) (system/memory.c:558-571)
```c
    /* FIXME: support unaligned access? */
    access_size = MAX(MIN(size, access_size_max), access_size_min);
    access_mask = MAKE_64BIT_MASK(0, access_size * 8);
    if (devend_big_endian(mr->ops->endianness)) {
        for (i = 0; i < size; i += access_size) {
            r |= access_fn(mr, addr + i, value, access_size,
                        (size - access_size - i) * 8, access_mask, attrs);
        }
    } else {
        for (i = 0; i < size; i += access_size) {
            r |= access_fn(mr, addr + i, value, access_size, i * 8,
                        access_mask, attrs);
        }
    }
```

## Code: GPIO wiring rule: one output to one input; use SPLIT_IRQ / OR_IRQ (include/hw/core/qdev.h:640-648)
```c
 * It is not valid to try to connect one outbound GPIO to multiple
 * qemu_irqs at once, or to connect multiple outbound GPIOs to the
 * same qemu_irq. (Warning: there is no assertion or other guard to
 * catch this error: the model will just not do the right thing.)
 * Instead, for fan-out you can use the TYPE_SPLIT_IRQ device: connect
 * a device's outbound GPIO to the splitter's input, and connect each
 * of the splitter's outputs to a different device.  For fan-in you
 * can use the TYPE_OR_IRQ device, which is a model of a logical OR
 * gate with multiple inputs and one output.
```

## Code: OR gate for shared NVIC lines (hw/arm/stm32l4x5_soc.c:264-276)
```c
    /* IRQs with fan-in that require an OR gate */
    for (unsigned i = 0; i < NUM_EXTI_OR_GATES; i++) {
        if (!object_property_set_int(OBJECT(&s->exti_or_gates[i]), "num-lines",
                                     exti_or_gates_num_lines_in[i], errp)) {
            return;
        }
        if (!qdev_realize(DEVICE(&s->exti_or_gates[i]), NULL, errp)) {
            return;
        }

        qdev_connect_gpio_out(DEVICE(&s->exti_or_gates[i]), 0,
            qdev_get_gpio_in(armv7m, exti_or_gates_out[i]));
```

## Code: GPIO model: pin lines, clock and runtime QOM properties in instance_init (hw/gpio/stm32l4x5_gpio.c:407-416)
```c
    qdev_init_gpio_out(DEVICE(obj), s->pin, GPIO_NUM_PINS);
    qdev_init_gpio_in(DEVICE(obj), stm32l4x5_gpio_set, GPIO_NUM_PINS);

    s->clk = qdev_init_clock_in(DEVICE(s), "clk", NULL, s, 0);

    object_property_add(obj, "disconnected-pins", "uint16",
                        disconnected_pins_get, disconnected_pins_set,
                        NULL, &s->disconnected_pins);
    object_property_add(obj, "clock-freq-hz", "uint32",
                        clock_freq_get, NULL, NULL, NULL);
```

## Code: qtest: drive a GPIO input and qom-set a runtime property (tests/qtest/stm32l4x5_gpio-test.c:124-142)
```c
static void gpio_set_irq(unsigned int gpio, int num, int level)
{
    g_autofree char *name = g_strdup_printf("/machine/soc/gpio%c",
                                            get_gpio_id(gpio) + 'a');
    qtest_set_irq_in(global_qtest, name, NULL, num, level);
}

static void disconnect_all_pins(unsigned int gpio)
{
    g_autofree char *path = g_strdup_printf("/machine/soc/gpio%c",
                                            get_gpio_id(gpio) + 'a');
    QDict *r;

    r = qtest_qmp(global_qtest, "{ 'execute': 'qom-set', 'arguments': "
        "{ 'path': %s, 'property': 'disconnected-pins', 'value': %d } }",
        path, 0xFFFF);
    g_assert_false(qdict_haskey(r, "error"));
    qobject_unref(r);
}
```

## Code: Clock-driven ptimer: ClockEvent callback and init (hw/timer/cmsdk-apb-timer.c:207-226)
```c
static void cmsdk_apb_timer_clk_update(void *opaque, ClockEvent event)
{
    CMSDKAPBTimer *s = CMSDK_APB_TIMER(opaque);

    ptimer_transaction_begin(s->timer);
    ptimer_set_period_from_clock(s->timer, s->pclk, 1);
    ptimer_transaction_commit(s->timer);
}

static void cmsdk_apb_timer_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    CMSDKAPBTimer *s = CMSDK_APB_TIMER(obj);

    memory_region_init_io(&s->iomem, obj, &cmsdk_apb_timer_ops,
                          s, "cmsdk-apb-timer", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    sysbus_init_irq(sbd, &s->timerint);
    s->pclk = qdev_init_clock_in(DEVICE(s), "pclk",
                                 cmsdk_apb_timer_clk_update, s, ClockUpdate);
```

## Code: Self-programmable flash via ROM device (NVMCTRL pattern) (hw/nvram/nrf51_nvm.c:285-306)
```c
static void flash_write(void *opaque, hwaddr offset, uint64_t value,
        unsigned int size)
{
    NRF51NVMState *s = NRF51_NVM(opaque);

    if (s->config & NRF51_NVMC_CONFIG_WEN) {
        uint32_t oldval;

        assert(offset + size <= s->flash_size);

        /* NOR Flash only allows bits to be flipped from 1's to 0's on write */
        oldval = ldl_le_p(s->storage + offset);
        oldval &= value;
        stl_le_p(s->storage + offset, oldval);

        memory_region_flush_rom_device(&s->flash, offset, size);
    } else {
        qemu_log_mask(LOG_GUEST_ERROR,
                "%s: Flash write 0x%" HWADDR_PRIx" while flash not writable.\n",
                __func__, offset);
    }
}
```

## Code: Kconfig for board + SoC (hw/arm/Kconfig:385-400)
```kconfig
config B_L475E_IOT01A
    bool
    default y
    depends on TCG && ARM
    select STM32L4X5_SOC
    imply DM163

config STM32L4X5_SOC
    bool
    select ARM_V7M
    select OR_IRQ
    select STM32L4X5_EXTI
    select STM32L4X5_SYSCFG
    select STM32L4X5_RCC
    select STM32L4X5_GPIO
    select STM32L4X5_USART
```

## Code: Rust sysbus device init: MMIO ops builder + clock input (rust/hw/char/pl011/src/device.rs:489-513)
```rust
        static PL011_OPS: MemoryRegionOps<PL011State> = MemoryRegionOpsBuilder::<PL011State>::new()
            .read(&PL011State::read)
            .write(&PL011State::write)
            .little_endian()
            .impl_sizes(4, 4)
            .build();

        // SAFETY: this and this.iomem are guaranteed to be valid at this point
        MemoryRegion::init_io(
            &mut uninit_field_mut!(*this, iomem),
            &PL011_OPS,
            "pl011",
            0x1000,
        );

        uninit_field_mut!(*this, regs).write(Default::default());

        let clock = DeviceState::init_clock_in(
            &mut this,
            "clk",
            &Self::clock_update,
            ClockEvent::ClockUpdate,
        );
        uninit_field_mut!(*this, clock).write(clock);
    }
```

## Code: QEMU policy on AI-generated contributions (docs/devel/code-provenance.rst:293-299)
```rst
  **Current QEMU project policy is to DECLINE any contributions which are
  believed to include or derive from AI generated content. This includes
  ChatGPT, Claude, Copilot, Llama and similar tools.**

  **This policy does not apply to other uses of AI, such as researching APIs
  or algorithms, static analysis, or debugging, provided their output is not
  included in contributions.**
```

## Open questions
- Does the target firmware depend on real Cortex-M0+ details: CPUID part number 0xC60, CONTROL.nPRIV unprivileged threads, or a working 8-region MPU? QEMU only offers 'cortex-m0'. Whether armv7m 'mpu-ns-regions'=8 on cortex-m0 yields a working Armv6-M MPU was not built or run here and needs an experiment; if exact behaviour matters, plan a small target/arm 'cortex-m0plus' patch.
- Upstream or internal fork? Upstream QEMU currently declines AI-generated contributions (docs/devel/code-provenance.rst:288-349; AGENTS.md), so upstream patches must be written by humans. GPLv2 obligations apply to any distributed modified binary, for example one bundled with MPLAB tooling. Who would own long-term maintenance, given that MAX78000 is marked Orphan?
- Host platform: native Windows (MSYS2) or Linux/WSL2? Modules are unavailable on Windows, plugins need dlltool, SocketCAN is Linux-only and some qtests are skipped on Windows. Rust would also need meson >= 1.12 and bindgen.
- What timing fidelity is required? QEMU gives register-level behaviour plus icount time at a power-of-two ns per instruction; flash wait states, bus contention and cycle-exact peripheral latencies are out of scope.
- Should register maps and reset values be generated from ATDF/SVD, and how? Options are the register API (exact-address matching only) or generated decode with byte-lane merging, needed because QEMU widens sub-word accesses without read-modify-write. How will behavioural semantics (SYNCBUSY, W1C, clear-on-read, write-protect via PAC) be specified?
- Interface for external twin I/O: a chardev-socket protocol (replay-friendly), QMP qom-set plus polling, or a custom proxy device? Upstream has no QMP pin-change events and no sysbus co-simulation bridge.
- Record/replay and reverse debugging are only exercised in-tree on A-profile boards; they need validation on a Cortex-M0 board.
- Should NVM contents (flash, DATAFLASH, USER page/fuses) persist across runs, for example through a block backend? rom_device storage as used by nrf51_nvm is volatile.
- How much of PORT pin muxing (PMUX), EVSYS event routing and DMAC should be modelled? Existing STM32 models skip alternate-function routing (tests/qtest/stm32l4x5_usart-test.c:163).
- Does the PIC32CM JH SysTick have a reference clock (SYST_CALIB/NOREF)? The answer decides whether armv7m 'refclk' should be wired; the datasheet needs checking.
