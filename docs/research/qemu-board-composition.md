# qemu-board-composition

## Summary
Upstream QEMU (11.1.50, commit 602e4f2da1) builds every board in compiled C. An MCU board is a MachineClass whose init function wires a netlist by hand: SoC, parts, split-irq fan-outs and clocks (see B-L475E-IOT01A, or the roughly 250 hard-coded I2C parts across the Aspeed board files). There is no upstream declarative board format, no devicetree-driven instantiation (only Xilinx's fork has -hw-dtb), and no QMP machine construction: the 2021-22 RFC was never merged and device_add has no allow-preconfig. The only way to add parts without recompiling is -device (or JSON -device / -readconfig) at startup for parts that attach to a bus. About 40 I2C/SMBus/PMBus target types and 137 m25p80 SPI flash/EEPROM types are user-creatable with bus=/address= or bus=/cs=. This is shown on Arm boards (tpm-tis-i2c on aspeed.i2c.bus.12, mx66u51235f on ssi.0, tmp105 on imx25 i2c-bus.0 in qtests), and MPS2-TZ deliberately leaves its "shield" I2C buses open for user parts. That path has hard limits:
- bus= takes non-unique qdev bus names, and many controllers literally use "i2c".
- SPI parts only work where the controller binds chip-select itself at reset (Aspeed SMC). Otherwise CS is never toggled, which is what resets m25p80's command state.
- Nothing can be hot-added, because I2C/SSI buses have no hotplug handler.
- Most upstream Cortex-M/AVR SoC models (STM32L4/F4, SmartFusion2, MAX78000, ATmega) leave their I2C/CAN controllers as unimplemented stubs. The micro:bit TWI fakes its accelerometer probe because the firmware panics otherwise.

GPIO-level composition, which is the core of a custom board, cannot be expressed from the CLI at all: split-irq, or-irq and ssi-sd are "needs to be wired up" types, led can be created but not wired, pin-mux/alternate functions are not modeled, and qemu_irq carries only an int level with no timestamp or drive strength. Stimulus injection does exist per model via QOM properties set with qom-set (sensor temperature/voltage/ainN, GPIO-expander pinN, npcm7xx adci[N]). The bus APIs are synchronous and take zero virtual time, running inside MMIO handlers under the BQL; i2c.h says transfers must "complete immediately". So an external part model must either answer inside the MMIO access, or the MCU controller must complete transfers asynchronously, which SERCOM's INTFLAG.MB/SB design allows; CAN frames and chardev UARTs are the natural async boundaries. Nothing in-tree attaches an out-of-process part to an MCU-side I2C/SPI/GPIO controller. vhost-user-i2c/spi/gpio expose guest-visible virtio adapters, vfio-user and multiprocess are PCI-only, and vhost-user, multiprocess, SocketCAN, ivshmem-flat and loadable modules are all unavailable on Windows hosts. Between QEMU instances: UART (chardev socket/udp) and Ethernet (netdev stream/dgram) work on every host, CAN works only through Linux SocketCAN, and there is no time sync across instances. icount and record/replay are per-instance; the TCG plugin time-control API, which does work on Windows, is only a building block. The in-process alternative is one C machine holding several MCUs on a shared virtual clock (ast2700fc: a Cortex-A35 plus two Cortex-M4). In short, QEMU can supply the CPU, the device-model framework and a modest, server-skewed parts library. The layer that lets a customer build their own board would have to be built by Microchip: a declarative netlist, pad-level PORT/PMUX routing (the ATDF already has the pad/function table), GPIO and analog nets, an external-part protocol with virtual-time sync, and Windows support. A chip-level model is a prerequisite for that layer and should expose board-facing hooks from day one.

## Findings

1. (high) [LOAD-BEARING] [How MCU boards are defined] Every upstream board, MCU boards included, is a compiled-in C MachineClass whose init function wires the board netlist by hand (SoC + parts + GPIO splitters + board clocks). There is no upstream declarative board format. Adding a board or a new part type means rebuilding QEMU, and loadable modules are not available on Windows hosts.
   detail: Board code uses object_initialize_child + qdev_connect_gpio_out + i2c_slave_create_simple/ssi_create_peripheral. Most upstream board-composition work comes from BMC boards (Aspeed/Nuvoton), which is why the reusable part library leans toward PMBus/thermal/server parts.
   - ev: hw/arm/b-l475e-iot01a.c:73-110 (bl475e_init: SoC, DM163 LED driver, 13 split-irq fan-outs wired pin by pin)
   - ev: hw/arm/b-l475e-iot01a.c:113-127 (MachineClass init)
   - ev: hw/arm/stm32vldiscovery.c:38-51 (board SYSCLK frequency hard-coded and connected in C)
   - ev: hw/arm/aspeed_ast2600_rainier.c:65-108 (one i2c_slave_create_simple/at24c_eeprom_init call per part); grep counts ~250 such calls across hw/arm/aspeed*.c
   - ev: meson.build:259-261 ('Modules are not available for Windows')

2. (high) [LOAD-BEARING] [Attaching I2C parts without recompiling (-device / -readconfig)] I2C targets can be added at startup without recompiling via -device <type>,bus=<i2c bus>,address=<n> (or JSON -device or -readconfig [device] sections). All ~40 concrete I2C/SMBus/PMBus target types inherit user_creatable=true; only smbus-eeprom opts out. The mechanism is documented and tested on Arm boards, and MPS2-TZ deliberately leaves its 'shield' I2C buses open for user-created devices.
   detail: Late-added I2C targets need no help from the controller, because the core rescans the bus at every START (hw/i2c/core.c:146). Created devices land at /machine/peripheral/<id> (system/qdev-monitor.c:355), so their QOM properties can then be driven with QMP qom-set.
   - ev: hw/core/qdev.c:776 (dc->user_creatable = true by default)
   - ev: hw/i2c/core.c:21-23 ('address' property) and :404-411 (bus_type = TYPE_I2C_BUS)
   - ev: hw/i2c/smbus_eeprom.c:148-149 ('Reason: init_data'; the only I2C-family opt-out found)
   - ev: system/qdev-monitor.c:681-692 (bus= lookup plus bus-type check)
   - ev: system/vl.c:2761-2786, 2828-2836 (-device processed right after machine_run_board_init, i.e. cold-plugged)
   - ev: docs/specs/tpm.rst:386-392 (-device tpm-tis-i2c,...,bus=aspeed.i2c.bus.12,address=0x2e on ast2600-evb)
   - ev: tests/qtest/libqos/arm-imx25-pdk-machine.c:85-91 + tests/qtest/tmp105-test.c:425-430 + tests/qtest/qos-test.c:262-285 (qtest composes -device tmp105 ... bus=i2c-bus.0 ... address=0x49 on arm/imx25-pdk)
   - ev: hw/arm/mps2-tz.c:737-748, 968-975 (internal I2C buses marked full; shield 0/1 buses left open)
   - ev: qemu-options.hx:5611-5619; docs/config/mach-virt-serial.cfg ([device] sections)

3. (high) [LOAD-BEARING] [Bus naming and addressing on the CLI] bus= resolves qdev bus names, not QOM paths. Bus names need not be unique ('Yes, that's screwed up'), and path elements match a device's qdev id or type name. SoC-internal controllers have no qdev id, so a SoC with several identical controllers whose bus is literally named 'i2c' cannot cleanly target one of them. Unnamed buses get unique automatic names (i2c-bus.N, ssi.N), and Aspeed gives each I2C bus an explicit unique name.
   detail: Consequence, inferred and not executed: on MPS2-TZ all five SBCon buses are named 'i2c', and bus=i2c returns the first one that is not full (shield 0), so shield 1 is probably not reachable by name. Design requirement for a Microchip model: give each SERCOM's I2C/SSI bus a unique name (e.g. 'sercom3.i2c').
   - ev: system/qdev-monitor.c:465-466 (comment), :418-448 (qbus_find_dev matches id, then type name), :508-595 (qbus_find)
   - ev: hw/core/bus.c:111-125 (auto-naming '<id>.<n>' or '<bus type>.<global n>')
   - ev: hw/i2c/arm_sbcon_i2c.c:88 and hw/arm/stellaris.c:696 (bus named 'i2c')
   - ev: hw/i2c/aspeed_i2c.c:1408-1422, 1631 (per-bus 'bus-name' '<label>.<n>')
   - ev: hw/i2c/imx_i2c.c:296 and hw/ssi/aspeed_smc.c:1160 (NULL name -> auto i2c-bus.N / ssi.N)
   - ev: hw/i2c/i2c_mux_pca954x.c:206-210 (every mux names its child buses 'i2c.0..N')

4. (high) [LOAD-BEARING] [SPI parts via -device and chip-select] SSI peripherals can be created with bus=/cs=, but chip-select is a separate named GPIO input ('ssi-gpio-cs') that the CLI cannot wire. It only works where the SPI controller binds CS itself, by cs index, at reset (aspeed_smc). Otherwise a part such as m25p80 never sees CS deassert, which is the only thing that resets its command state machine. ssi-sd (SD card in SPI mode) cannot be created from the CLI at all.
   detail: On MCUs, CS is usually a firmware-driven PORT pin rather than controller hardware, so CS for an external SPI part is really a GPIO net. The resulting failure for unwired parts is inferred from the code, not observed.
   - ev: hw/ssi/ssi.c:30-56 (ssi_get_cs and duplicate-cs check), :97-113 ('cs' property, CS gpio created at realize)
   - ev: hw/ssi/aspeed_smc.c:628-654 (reset-time lookup of user-created flashes and CS wiring)
   - ev: docs/system/arm/aspeed.rst:141-158 (-nodefaults + -device mx66u51235f,bus=ssi.0,cs=0x0,drive=fmc0)
   - ev: hw/block/m25p80.c:1594-1605 (state reset only on CS deassert), :1909-1918 (cs_polarity = SSI_CS_LOW)
   - ev: hw/sd/ssi-sd.c:329-330 ('Reason: GPIO chip-select line should be wired up')

5. (high) [LOAD-BEARING] [Hotplug, preconfig, QMP construction, devicetree, -machine none] There is no runtime or QMP-driven board construction upstream. device_add after machine-ready needs a hotplug-capable bus, and hw/i2c and hw/ssi register none. --preconfig is experimental, and device_add has no allow-preconfig on master. The QMP machine-creation RFC (x-machine-init, query-machine-phase) is not in the tree, although the MachineInitPhase enum is. No machine is instantiated from a devicetree upstream; only Xilinx/AMD's fork does that (-hw-dtb with arm-generic-fdt). -machine none provides only a CPU and RAM.
   detail: qapi/ is not checked out locally, so the device_add schema was checked against current GitLab master.
   - ev: hw/core/qdev-hotplug.c:33-63 (errors at :40, :47, :57); grep 'hotplug' in hw/i2c, hw/ssi, hw/sensor, hw/gpio: no matches
   - ev: system/qdev-monitor.c:718-723 (hotplug check applied once PHASE_MACHINE_READY)
   - ev: docs/system/managed-startup.rst:21-35
   - ev: https://gitlab.com/qemu-project/qemu/-/raw/master/qapi/qdev.json (device_add: no 'allow-preconfig')
   - ev: include/hw/core/qdev.h:1118-1149 (MachineInitPhase); grep for x-machine-init / query-machine-phase: no matches
   - ev: https://lore.kernel.org/all/YJ6jSeu3Uz4g6cCK@redhat.com/T/ and https://mail.gnu.org/archive/html/qemu-devel/2022-05/msg04864.html (RFC series, 2021-22)
   - ev: https://docs.amd.com/r/en-US/ug1702-vitis-accelerated-reference/Versal-PS-and-PMC-Arguments-for-QEMU ('arm-generic-fdt ... parse dtb for machine generation, passes by -hw-dtb')
   - ev: https://github.com/Xilinx/qemu/tree/master/hw/core (fdt_generic_util.c exists only in the fork)
   - ev: hw/core/null-machine.c:20-43

6. (high) [Dynamic sysbus devices and the platform bus] Dynamic sysbus (MMIO) devices are a per-machine allow-list used by virt-style machines (arm/riscv/loongarch virt, pc, e500plat, pnv, spapr). The platform bus picks a free MMIO window and IRQ automatically, and the guest finds the device through a generated devicetree. That does not fit MCU firmware with a fixed memory map. Sysbus devices are non-creatable by default because device_add cannot make their MMIO and IRQ connections.
   - ev: hw/core/sysbus.c:295-311
   - ev: hw/core/machine.c:680-705; include/hw/core/boards.h:70-84
   - ev: system/qdev-monitor.c:277-284
   - ev: hw/arm/virt.c:4167-4172 (allow-list), :3915-3921 (platform_bus_link_device)
   - ev: hw/core/platform-bus.c:128-180

7. (high) [LOAD-BEARING] [GPIO wiring from the CLI or QMP] GPIO nets cannot be declared from the CLI; wiring is done with qdev_connect_gpio_out in C. The fan-out and merge primitives (split-irq, or-irq) are non-creatable ('Needs to be wired up'). led can be created but its input cannot be connected. gpio-key, gpio-pwr, stellaris-gamepad and gpio_i2c are sysbus devices and cannot be created either. The supported external hooks are qtest's set_irq_in and irq_intercept_in/out: one device at a time, and clock stepping only under the qtest accelerator, whose vCPUs are dummies. Mechanically, gpio-out pins are QOM link<irq> properties whose setter always allows the change, so QMP qom-set while paused at -S might rewire them, but that is undocumented and untested.
   detail: High confidence that the CLI cannot express GPIO wiring. Low confidence that qom-set rewiring works in practice, because nothing was built or run. Caveats: it replaces any existing connection, cannot fan out (split-irq is not creatable), and the controller may not re-send the current level until the next change.
   - ev: hw/core/gpio.c:43-66 (inputs are child<irq> 'unnamed-gpio-in[N]'), :74-95 (outputs are link<irq> with object_property_allow_set_link), :117-130
   - ev: qom/object.c:1969-1973 (allow_set_link: 'Allow the link to be set, always'), :2061-2099 (link setter resolves a path string)
   - ev: hw/core/split-irq.c:74-75; hw/core/or-irq.c:131-132
   - ev: hw/misc/led.c:100-127 (TYPE_DEVICE; gpio-in created at realize)
   - ev: hw/gpio/gpio_key.c:98-99; hw/gpio/gpio_pwr.c:59-60; hw/input/stellaris_gamepad.c:100-101; hw/i2c/bitbang_i2c.c:233-237 (all TYPE_SYS_BUS_DEVICE)
   - ev: system/qtest.c:226-260, 379-460 (:397 named in-GPIO interception unsupported, :403 'IRQ intercept already enabled'), :710, :747 (clock_step/clock_set only when qtest_enabled)
   - ev: accel/qtest/qtest.c:68 (dummy_start_vcpu_thread)

8. (high) [LOAD-BEARING] [Pin-mux and electrical pin semantics] QEMU has no generic pad or pin-mux layer: board code connects peripheral signals regardless of how the guest programs alternate functions. The Stellaris board describes the 'right way' and does not implement it; the STM32L4 GPIO model logs 'Alternate functions aren't supported'. qemu_irq is a plain int level with no drive strength, tri-state or timestamp. Pull-ups, open-drain and disconnected pins are emulated only inside individual GPIO models. Microchip's ATDF already contains the pad-to-peripheral function table needed to drive such a layer.
   detail: A customer schematic connects parts to pads (e.g. PA08), not to 'SERCOM0'. A scalable board twin therefore needs pad-level routing that follows the guest's PMUX/PINCFG writes at run time, generated from the ATDF signal tables.
   - ev: hw/arm/stellaris.c:1243-1300 (SSI/OLED/SD aux-function discussion; 'In theory the right way to model this would be...')
   - ev: hw/gpio/stm32l4x5_gpio.c:313-323 (AFRL/AFRH LOG_UNIMP), :95-160 (model-local open-drain/pull/disconnected handling)
   - ev: include/hw/core/irq.h:11-18; hw/core/irq.c:29-38
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:152-160 (CAN0 RX on PA25/PB23, TX on PA24/PB22, function G)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:900-903 (SERCOM0 PAD0 on PA04 function D, PA08 function C, ...)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5425-5427 (PORT PMUX PMUXE/PMUXO)

9. (high) [LOAD-BEARING] [Reusable board-level part library] Reusable parts in the tree: about 40 I2C-attached types and 137 m25p80 SPI NOR/EEPROM types, plus an SSD0323 SPI OLED, LED and DM163. The I2C types cover temperature/voltage/pressure/magnetometer sensors (including Microchip's EMC1413/EMC1414), PMBus regulators, RTCs, a generic 24xx EEPROM, PCA955x/PCA9554/PCF8574 GPIO expanders, PCA9546/9548 muxes, an SSD0303 OLED and an I2C TPM. The SPI types include SST25 parts and the AT25128A/AT25256A SPI EEPROMs. Missing: ADS7846 and MAX111x (no longer in the tree; the PXA boards that used them were removed in 9.2) and Microchip companion parts such as MCP23xxx, MCP9808, MCP3xxx and MCP2515 (no matches).
   detail: Statements about which parts belong to Microchip (SST25, Atmel AT25 EEPROMs, SMSC EMC141x) are medium confidence. The part mix is skewed toward BMC/server parts because Aspeed and Nuvoton boards drove most of the contributions.
   - ev: hw/sensor/*.c (tmp105.c:487-506, tmp421.c:46-50, emc141x.c:299-316, adc128d818.c:688-689, lsm303dlhc_mag.c:545-546, dps310.c:213-214, adm1266/adm1272/isl_pmbus_vr/max31785/max34451)
   - ev: hw/rtc/ds1338.c:237-238; hw/rtc/m41t80.c:109-110; hw/rtc/rs5c372.c:228-229
   - ev: hw/nvram/eeprom_at24c.c:231-234 (rom-size, address-size, writable, drive)
   - ev: hw/gpio/pca9552.c:659-680; hw/gpio/pca9554.c:342-352; hw/gpio/pcf8574.c:136-138, 155-156; hw/i2c/i2c_mux_pca954x.c:240-255
   - ev: hw/block/m25p80.c:179-198 (Atmel entries incl. at25128a/at25256a-nonjedec), :1940-1947 (one QOM type per known_devices entry; 137 INFO entries counted)
   - ev: docs/about/removed-features.rst:1144-1156 (PXA/OMAP2 boards removed in 9.2)
   - ev: https://datasheets.globalspec.com/ds/microchip-technology/emc1413/696ac8af-6223-4380-9c1f-3793801bd444 (EMC1413 is a Microchip part)

10. (high) [LOAD-BEARING] [Stimulus injection and observability] Analog and environmental stimulus exists only as per-model QOM properties set with QMP qom-set: tmp105 'temperature' in m°C, adc128d818 ainN/ext-vref-mv, PMBus vin/vout/iout, lsm303 mag-x/y/z, PCA955x pinN/ledN, npcm7xx ADC adci[N]/vref (all exercised by qtests). aspeed_adc and stm32f2xx_adc return synthetic incrementing values instead. Watching outputs is poorly supported: GPIO and LED models emit no QMP events, LED intensity is visible only in trace events, and qtest interception covers one device.
   detail: Pattern to copy for a Microchip ADC/AC/PTC model: expose per-channel input QOM properties, as npcm7xx_adc does.
   - ev: hw/sensor/tmp105.c:156-174, 409-415; tests/qtest/tmp105-test.c:39-60
   - ev: hw/sensor/adc128d818.c:640-653; hw/sensor/lsm303dlhc_mag.c:512-524; hw/sensor/adm1272.c:496-508
   - ev: hw/adc/npcm7xx_adc.c:280-289; tests/qtest/npcm7xx_adc-test.c:136-147
   - ev: hw/adc/aspeed_adc.c:63-67 (update_channels +5/+7); hw/adc/stm32f2xx_adc.c:70-73 ('Attempts to fake some ADC values'), :155-158
   - ev: hw/gpio/pca9552.c:559-580; hw/gpio/aspeed_gpio.c:1470-1494
   - ev: hw/misc/trace-events:388-389 (led_set_intensity); grep qapi_event_send in hw/gpio, hw/misc, hw/sensor, hw/i2c, hw/ssi, hw/adc: no matches
   - ev: system/qtest.c:403

11. (high) [LOAD-BEARING] [Board-facing controllers in upstream MCU SoC models] Upstream Cortex-M and AVR SoC models mostly stub out the controllers that board parts would attach to. STM32L4x5 and STM32F405 map I2C and CAN as unimplemented devices; Microchip SmartFusion2 (msf2-soc) maps I2C and CAN as unimplemented; ATmega maps TWI, SPI and ADC as unimplemented. The micro:bit TWI is a stub that fakes the accelerometer probe because the firmware 'panics if they are not found'. No SAM/PIC32CM/SERCOM model exists upstream.
   detail: The micro:bit case shows that a chip-only model breaks real application firmware that probes board parts at boot. Upstream had to add a fake board part inside the controller model.
   - ev: hw/arm/stm32l4x5_soc.c:376-380
   - ev: hw/arm/stm32f405_soc.c:267-271
   - ev: hw/arm/msf2-soc.c:213-219
   - ev: hw/avr/atmega.c:376-380
   - ev: hw/i2c/microbit_i2c.c:5-8
   - ev: grep -i 'sercom|samd2|same5|pic32cm|pic32mz|atsam' in hw, include, docs: no matches

12. (high) [LOAD-BEARING] [I2C semantics at the chip/board boundary] I2C is a synchronous, byte-level API that takes zero virtual time: start(addr,dir), send, recv, end and nack callbacks run inside the controller's MMIO handler with the BQL held. recv 'cannot fail', and a NACK can come only from start or send. The header says transfers must 'complete immediately' and that targets cannot defer their response. send_async and i2c_bus_master exist only for a target device acting as bus master (Aspeed target mode, i2c-echo). The API cannot signal arbitration loss, clock stretching or bus errors, all of which SERCOM I2CM reports.
   detail: An external I2C part must either answer inside the MMIO access (block the vCPU and drop the BQL, as multiprocess QEMU does) or the controller model must issue the request and set MB/SB later in virtual time. SERCOM's flag-driven design allows the second. A Microchip SERCOM model could keep QEMU's I2CBus for in-process parts and add an async back end for external ones.
   - ev: include/hw/i2c/i2c.h:7-10, 26-56
   - ev: hw/i2c/core.c:121-176 (start), :255-310 (send/recv), :311-327 (nack), :185-235 + :276-291 (bus_master/send_async)
   - ev: hw/i2c/imx_i2c.c:124-127, 232-242 (i2c_recv/i2c_send called inside MMIO read/write)
   - ev: docs/devel/multi-thread-tcg.rst:226-228, 240-242 (MMIO serialized by the BQL)
   - ev: hw/i2c/aspeed_i2c.c:1632; hw/misc/i2c-echo.c:47-95
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6103-6107 (I2CM INTFLAG MB/SB/ERROR), :6146-6156 (I2CM STATUS BUSERR/ARBLOST/RXNACK/LOWTOUT/CLKHOLD/...)

13. (high) [LOAD-BEARING] [SSI, GPIO, CAN and UART semantics] SSI: ssi_transfer() moves one word full-duplex and ORs the replies of all selected peripherals; CS is a separate GPIO with per-device polarity; no clock rate, CPOL/CPHA or bit order is modeled. GPIO: qemu_set_irq is a synchronous call carrying an int level plus an optional observer, with no timestamp; edge times come only from QEMU_CLOCK_VIRTUAL and are instruction-exact only under icount. CAN: frame-level (11/29-bit IDs, RTR/ERR, CAN FD via a flag and 64-byte data), delivered synchronously to every other client with can_receive back-pressure, and no bit timing or arbitration. UART: a chardev byte stream with flow control but no timing; the baud rate reaches only host serial back ends via CHR_IOCTL_SERIAL_SET_PARAMS. CAN and UART therefore make natural async boundaries for an external model; I2C, SPI and GPIO do not.
   detail: A board twin that forwards GPIO needs batching or time-stamping at quantum boundaries. Bit-banged protocols need icount, and icount is single-threaded.
   - ev: include/hw/ssi/ssi.h:1-11, 27-58; hw/ssi/ssi.c:72-95, 156-167
   - ev: include/hw/core/irq.h:11-18; hw/core/irq.c:29-38
   - ev: docs/devel/tcg-icount.rst:51-61 (MMIO handled at instruction-exact points under icount)
   - ev: include/net/can_emu.h:47-64, 92-118; net/can/can_core.c:104-127
   - ev: hw/char/serial.c:170-186 (char_transmit_time used only for the model's own timers); hw/char/ibex_uart.c:227, 249; include/chardev/char-serial.h:29-35

14. (high) [LOAD-BEARING] [Attaching external (out-of-process) parts] Nothing in-tree lets an external process back an MCU-side I2C, SPI or GPIO part. No chardev-backed device exists in hw/i2c, hw/ssi, hw/gpio, hw/sensor, hw/adc, hw/net/can or hw/i3c. vhost-user-i2c/spi/gpio expose a VirtIO adapter to the guest (which needs a virtio driver; the rust-vmm back end proxies to host /dev/i2c-X adapters), and vhost-user is not built on Windows. vfio-user and multiprocess QEMU (hw/remote) are PCI-only, and multiprocess is Linux-only. The closest templates are ipmi-bmc-extern (an async framed protocol over a chardev) and multiprocess QEMU's synchronous request/reply, which drops the BQL while it blocks.
   - ev: grep DEFINE_PROP_CHR in those dirs: only hw/misc/ivshmem-flat.c, hw/misc/ivshmem-pci.c (plus vhost-user-blk/media)
   - ev: hw/virtio/vhost-user-i2c.c:27 (VIRTIO_ID_I2C_ADAPTER); hw/virtio/vhost-user-spi.c:28; hw/virtio/vhost-user-gpio.c:27
   - ev: docs/system/devices/virtio/vhost-user.rst:20-72; https://github.com/rust-vmm/vhost-device/tree/main/vhost-device-i2c
   - ev: meson.build:223-226 ('vhost-user is not available on Windows'); meson.build:210-212 ('Multiprocess QEMU is supported only on Linux')
   - ev: docs/system/devices/vfio-user.rst:7-12
   - ev: hw/remote/mpqemu-link.c:34-80, 189; hw/remote/proxy.c:255-274
   - ev: hw/ipmi/ipmi_bmc_extern.c:109, 494, 513

15. (high) [LOAD-BEARING] [Wiring idioms a Microchip SoC model should adopt] Existing idioms that make a SoC model composable at board level: machine-level link<can-bus> properties (-machine canbus0=X); the SPI controller binding user-created parts by cs index at reset; marking internal buses full while leaving expansion buses open; creating default board parts only without -nodefaults, with machine string properties to choose part models; per-pin QOM properties for stimulus; and qemu_irq lines that carry values (npcm7xx PWM duty feeding a fan tach model).
   - ev: hw/arm/sabrelite.c:59-65, 118-133; hw/arm/fsl-imx6.c:509-511; docs/system/devices/can.rst:175-198
   - ev: hw/ssi/aspeed_smc.c:640-654
   - ev: hw/arm/mps2-tz.c:737-748
   - ev: hw/arm/aspeed.c:185-194 (defaults_enabled), :353-359 (fmc-model/spi-model)
   - ev: hw/misc/npcm7xx_pwm.c:142, 500-507; hw/arm/npcm7xx_boards.c:162, 193

16. (medium) [LOAD-BEARING] [What SERCOM means for the boundary] A SERCOM instance switches at run time between I2C master/target, SPI master/target and USART modes on the same pads. A QEMU SERCOM model would have to expose an I2C bus, an SSI bus and a chardev at once and route by CTRLA.MODE. QEMU's only analog of target mode (MCU as I2C/SPI target of another master) is Aspeed's I2C target with i2c_bus_master; SSI has no concept of a target-mode controller.
   detail: This is a design inference from the evidence. MCU-to-MCU links over I2C/SPI, which are common on customer boards, need target-mode modeling plus a way to connect two models.
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5830-5836 (SERCOM modes I2CM, I2CS, SPIS, SPIM, USART_EXT, USART_INT)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:899-1112 (SERCOM0..7 instances)
   - ev: hw/i2c/aspeed_i2c.c:1632; hw/i2c/core.c:185-235
   - ev: include/hw/ssi/ssi.h:1-11 (SSI assumes the SoC is master)

17. (high) [LOAD-BEARING] [Links between instances (multiple MCUs or ECUs)] Between QEMU processes, UART links (-chardev socket/udp) and Ethernet (-netdev stream/dgram/socket) are built on every host, Windows included. CAN reaches another process only through can-host-socketcan, which is compiled only on Linux. ivshmem-flat (MCU-friendly shared memory plus doorbell) is Linux-only, cannot be created from the CLI, and no machine instantiates it. There is no cross-process I2C or SPI link. Within one process, a C machine can host several MCUs on one virtual clock (ast2700fc: a Cortex-A35 plus two Cortex-M4 coprocessors).
   - ev: qemu-options.hx:4097-4102 (chardev socket/udp), :3075-3095 (netdev socket/stream/dgram)
   - ev: net/meson.build:1-17, 55-64; chardev/meson.build:11-13
   - ev: net/can/meson.build:1-7; docs/system/devices/can.rst:3-7, 40-46
   - ev: hw/misc/Kconfig:84-87 (depends on LINUX); hw/misc/ivshmem-flat.c:447-451 ('Must be wired up in code'); no users found in hw/arm or hw/riscv
   - ev: hw/arm/aspeed_ast27x0-fc.c:67, 141-142, 175-176, 228; hw/arm/aspeed_ast27x0-ssp.c:132, 188-190

18. (high) [LOAD-BEARING] [Time synchronization] Determinism is per instance only. -icount shift=N,sleep=off gives deterministic guest-visible time and instruction-exact MMIO timestamps, but is incompatible with MTTCG. Record/replay covers one instance's serial, network, block and input. Core QEMU has no cross-instance time barrier or quantum (grep found none). The TCG plugin API (version 7) can take control of virtual time (qemu_plugin_request_time_control / qemu_plugin_update_ns, as contrib/plugins/ips.c does) and plugins build on Windows, so a plugin is the natural place to hang a quantum sync with other instances or external models. That sync has to be written.
   detail: The plugin API has no device or bus hooks; its exported functions cover instrumentation, register/memory access and time only. Device models and bridges therefore still need C code inside QEMU.
   - ev: qemu-options.hx:5340-5372
   - ev: docs/devel/tcg-icount.rst:17-20, 51-61
   - ev: docs/system/replay.rst:20-30
   - ev: grep -i 'quantum|time.sync|timesync|lockstep|cosim' in accel/tcg, system, util, include/system: no matches
   - ev: include/plugins/qemu-plugin.h:803-827; plugins/api-system.c:105-130 (one controller; blocks migration)
   - ev: contrib/plugins/ips.c:1-14, 84-86; contrib/plugins/lockstep.c:1-20 (lockstep is a TCG debugging aid, not a sync service)
   - ev: plugins/meson.build:40, 71; plugins/win32_linker.c

19. (high) [LOAD-BEARING] [Windows-host constraints] For customers on Windows hosts (typical of MPLAB X users), several composition mechanisms are unavailable: loadable modules, vhost-user, multiprocess, TPM back ends, SocketCAN bridging and ivshmem/ivshmem-flat. Available: -device/-readconfig, chardev and netdev sockets, QMP qom-set, and TCG plugins.
   - ev: meson.build:210-212, 218-220, 223-226, 259-261
   - ev: net/can/meson.build:3-5
   - ev: hw/misc/Kconfig:84-87
   - ev: plugins/win32_linker.c

20. (medium) [Comparison outside QEMU (brief)] Renode already ships the two layers QEMU lacks: a YAML-like platform description format (REPL) with GPIO/IRQ connection syntax, and quantum-based synchronization across nodes. The separate co-simulation research run should cover it in depth.
   detail: The REPL documentation page shows IRQ/GPIO and sysbus registration syntax but no I2C/SPI example. Whether Renode's sync is deterministic was not confirmed from that page.
   - ev: https://renode.readthedocs.io/en/latest/advanced/platform_description_format.html
   - ev: https://renode.readthedocs.io/en/latest/advanced/time_framework.html ('Once a quant is granted to the sinks, the source waits for them to finish execution.')

## Table: Board-level device models in QEMU
| device | interface | file | attach properties | user-creatable via -device? | notes |
| tmp105 / tmp75 / tmp175 / lm75b | I2C temperature sensor | hw/sensor/tmp105.c | bus=<i2c bus>, address=<7-bit> | Yes (default, hw/core/qdev.c:776) | 'temperature' (m°C) settable with qom-set (:409-415); ALERT = gpio-out[0] (:406), can only be wired in C |
| tmp421 / tmp422 / tmp423 | I2C temperature sensor | hw/sensor/tmp421.c | bus=, address= | Yes | temperature0..3 class properties (:353-362) |
| emc1413 / emc1414 (Microchip) | I2C/SMBus temperature sensor | hw/sensor/emc141x.c | bus=, address= | Yes | temperature0..3 (:254-263) |
| adc128d818 | I2C ADC | hw/sensor/adc128d818.c | bus=, address=, description | Yes | ainN, temperature, ext-vref-mv via qom-set (:640-653); IRQ gpio-out (:664) |
| lsm303dlhc_mag | I2C magnetometer | hw/sensor/lsm303dlhc_mag.c | bus=, address= | Yes | mag-x/y/z, temperature (:512-524) |
| dps310 | I2C pressure/temperature | hw/sensor/dps310.c | bus=, address= | Yes | no injection properties found |
| adm1266, adm1272, isl69259/isl69260/raa228000/raa229004, max31785, max34451 | PMBus (SMBus over I2C) | hw/sensor/*.c, hw/i2c/pmbus_device.c | bus=, address= | Yes | vin/vout/iout/temperature properties (e.g. adm1272.c:496-508) |
| ds1338, m41t80, rs5c372 | I2C RTC | hw/rtc/ds1338.c, m41t80.c, rs5c372.c | bus=, address= | Yes | time follows host/QEMU RTC |
| at24c-eeprom | I2C EEPROM (24xx) | hw/nvram/eeprom_at24c.c | bus=, address=, rom-size, address-size, writable, drive | Yes | backing file must equal rom-size (:174-181) |
| smbus-eeprom | SMBus EEPROM | hw/i2c/smbus_eeprom.c | - | No ('Reason: init_data', :148-149) | board code only |
| pca9552 / pca9535 / pca9555 | I2C LED/GPIO expander | hw/gpio/pca9552.c | bus=, address=, description | Yes | per-pin led%d/pin%d properties (:559-580); gpio in/out per pin (:611-612) |
| pca9554 / pca9536 | I2C GPIO expander | hw/gpio/pca9554.c | bus=, address=, description, hw-dir | Yes | pin%d properties (:283); gpio in/out (:307-308) |
| pcf8574 | I2C GPIO expander | hw/gpio/pcf8574.c | bus=, address= | Yes | 8 gpio in/out plus named 'nINT' output (:136-138) |
| pca9546 / pca9548 | I2C mux | hw/i2c/i2c_mux_pca954x.c | bus=, address=, name | Yes | child buses named 'i2c.0..N' in every mux (:206-210) |
| ssd0303 | I2C OLED | hw/display/ssd0303.c | bus=, address= | Yes | graphic console |
| tpm-tis-i2c | I2C TPM | hw/tpm/tpm_tis_i2c.c | bus=, address=, tpmdev= | Yes | documented on ast2600-evb (docs/specs/tpm.rst:392); TPM not built on Windows (meson.build:218-220) |
| i2c-echo | I2C test target (async/bus-master) | hw/misc/i2c-echo.c | bus=, address= | Yes | example of i2c_bus_master/i2c_send_async (:47-95) |
| axp209_pmu / axp221_pmu, sii9022, i2c-ddc, smbus-ipmi | I2C PMIC / HDMI / EDID / IPMI | hw/misc/axp2xx.c, hw/display/sii9022.c, hw/display/i2c-ddc.c, hw/ipmi/smbus_ipmi.c | bus=, address= | Yes | not typical MCU board parts |
| m25p80 family: 137 part types (e.g. sst25vf016b, at25df321a, at25128a-nonjedec, w25q*, n25q*, mx25*) | SPI NOR flash / SPI EEPROM | hw/block/m25p80.c | bus=<ssi bus>, cs=, drive=, write-enable, nonvolatile-cfg | Yes | CS active-low on 'ssi-gpio-cs'; command state resets only on CS deassert (:1594-1605), so the controller must wire CS (aspeed_smc.c:640-654) |
| ssd0323 | SPI OLED | hw/display/ssd0323.c | bus=, cs= | Yes | CS active-high (:373); D/C line is a gpio-in (:363) that the CLI cannot wire |
| ssi-sd (+ sd-card-spi) | SD card in SPI mode | hw/sd/ssi-sd.c | - | No ('GPIO chip-select line should be wired up', :329-330) | board code only (e.g. stellaris.c:1301-1310) |
| led | GPIO input | hw/misc/led.c | color, description, gpio-active-high (no bus) | Yes, but its input cannot be wired from the CLI | state visible only in trace led_set_intensity (hw/misc/trace-events:388) |
| gpio-key / gpio-pwr / stellaris-gamepad | GPIO button / power / host keys | hw/gpio/gpio_key.c, hw/gpio/gpio_pwr.c, hw/input/stellaris_gamepad.c | - | No (sysbus, hw/core/sysbus.c:310) | gpio-key holds its output for 100 ms (:35, :73-75); gamepad maps host keycodes to gpio-out |
| dm163 | GPIO bit-banged LED-matrix driver | hw/display/dm163.c | none (GPIO only) | Creatable (no override) but useless without wiring in code | used by b-l475e-iot01a.c:88-110 through split-irq |
| split-irq / or-irq | GPIO fan-out / wired-OR | hw/core/split-irq.c, hw/core/or-irq.c | num-lines | No ('Needs to be wired up', split-irq.c:74-75; or-irq.c:131-132) | the only fan-out/merge primitives |
| gpio_i2c | GPIO bit-bang to I2C bridge | hw/i2c/bitbang_i2c.c | - | No (sysbus) | bit-banged SDA/SCL drives I2CBus targets (:199-224) |
| npcm7xx_adc (pattern) | ADC value injection | hw/adc/npcm7xx_adc.c | SoC-internal | n/a | adci[N]/vref via qom-set (:280-289; tests/qtest/npcm7xx_adc-test.c:136-147); aspeed/stm32 ADCs return synthetic values |
| npcm7xx_pwm -> npcm7xx_mft (pattern) | PWM duty to fan tach | hw/misc/npcm7xx_pwm.c, hw/misc/npcm7xx_mft.c | SoC-internal; board wires via split-irq | n/a | qemu_irq level carries the duty value (npcm7xx_pwm.c:142) |
| can-bus, can-host-socketcan | CAN bus objects | net/can/can_core.c, net/can/can_socketcan.c | -object can-bus,id=X; -object can-host-socketcan,if=,canbus=X | Yes (-object) | SocketCAN bridge built only on Linux (net/can/meson.build:3-5) |
| kvaser_pci, pcm3680_pci, mioe3680_pci, ctucan_pci, flexcan, xlnx-zynqmp-can, xlnx-versal-canfd | CAN / CAN FD controllers | hw/net/can/*.c | canbus=X (PCI) or machine canbus0/1 links (sabrelite.c:118-133) | PCI ones yes; SoC ones via machine link properties | no Bosch M_CAN model (PIC32CM JH01 'CAN' is U2003) |
| chardev back ends (socket, udp, pty, file, stdio, ringbuf, hub, mux) | UART byte stream | chardev/*.c | -chardev ..., attached via -serial or SoC chardev properties | Yes | no timing; baud only via CHR_IOCTL_SERIAL_SET_PARAMS (include/chardev/char-serial.h:29) |
| ads7846, max111x | SPI touchscreen / ADC | not in tree | - | - | no longer present; PXA boards that used them removed in 9.2 (docs/about/removed-features.rst:1144-1156) |

## Table: Board composition capabilities & gaps
| capability | available in QEMU? | mechanism (file:line) | gap for customer custom boards |
| Define a board | Yes, compiled C only | MachineClass + qdev wiring (hw/arm/b-l475e-iot01a.c:73-127); ~250 hard-coded I2C parts in hw/arm/aspeed*.c | Every board needs C code and a rebuild; no loadable modules on Windows (meson.build:259-261) |
| Declarative parts list at startup | Partial | -device / JSON -device / -readconfig [device] (system/qdev-monitor.c:681-692; qemu-options.hx:5611-5619; docs/config/mach-virt-serial.cfg) | Only bus-attached qdev parts on reachable, uniquely named buses; no nets, pins or analog |
| Attach I2C part to MCU controller | Partial | hw/i2c/core.c:21-23, 146, 404-411; docs/specs/tpm.rst:392; tests/qtest/libqos/arm-imx25-pdk-machine.c:87; hw/arm/mps2-tz.c:737-748 | Most upstream Cortex-M SoCs leave I2C unimplemented (stm32l4x5_soc.c:376-378, msf2-soc.c:213-216); bus names often non-unique (system/qdev-monitor.c:465-466) |
| Attach SPI part | Partial | hw/ssi/ssi.c:30-56, 112; hw/ssi/aspeed_smc.c:640-654; docs/system/arm/aspeed.rst:150-158 | Only if the controller binds CS; GPIO-driven CS (typical on MCUs) cannot be expressed; ssi-sd not creatable |
| Default parts vs custom parts | Yes, per machine | defaults_enabled()/-nodefaults (hw/arm/aspeed.c:185-194); fmc-model/spi-model properties (:353-359) | Has to be designed into each machine |
| GPIO net between MCU pin and part | No (C only) | qdev_connect_gpio_out (hw/core/gpio.c:117-130); split-irq/or-irq not creatable | Core gap. qom-set on gpio-out links (gpio.c:85-95; qom/object.c:1969-1973) might work at -S but is untested |
| Pin-mux / pad-level routing | No | hw/arm/stellaris.c:1291-1300; hw/gpio/stm32l4x5_gpio.c:313-323 | Schematics bind parts to pads; the ATDF has pad/function tables (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:152-160, 900-903, 5425-5427) |
| Electrical semantics (pulls, open-drain, contention, wired-AND) | Only inside individual models | hw/gpio/stm32l4x5_gpio.c:95-160; include/hw/core/irq.h:11-18 | No net resolution or drive-strength model |
| Analog stimulus | Per-model QOM properties | hw/adc/npcm7xx_adc.c:280-289; hw/sensor/tmp105.c:409-415; hw/sensor/adc128d818.c:640-653 | No analog nets or waveforms; each Microchip analog model must expose its own inputs |
| Observe outputs (LEDs, pins) | Partial | trace events (hw/misc/trace-events:388); qtest irq_intercept_out (system/qtest.c:379-432, one device); QOM getters (hw/gpio/aspeed_gpio.c:1470-1494) | No QMP events and no generic probe |
| Add parts at run time | No | hw/core/qdev-hotplug.c:33-63; no hotplug handlers in hw/i2c, hw/ssi | Parts are fixed at launch |
| QMP/preconfig machine construction | No | include/hw/core/qdev.h:1118-1149 (phases only); docs/system/managed-startup.rst:21-35; qapi/qdev.json on master (device_add has no allow-preconfig) | 2021-22 RFC (x-machine-init) not merged |
| Devicetree-driven machine | No (upstream) | nothing in tree; Xilinx fork has -hw-dtb arm-generic-fdt (AMD UG1702) | Would mean depending on a fork or writing new code |
| Dynamic MMIO (sysbus) devices | virt-style machines only | hw/core/machine.c:680-705; system/qdev-monitor.c:277-284; hw/arm/virt.c:4167-4172; hw/core/platform-bus.c:128-180 | Does not fit fixed MCU memory maps |
| External (out-of-process) I2C/SPI/GPIO part | No | no chardev-backed devices in hw/i2c, ssi, gpio, sensor; vhost-user-i2c/spi/gpio are guest-visible virtio (hw/virtio/vhost-user-i2c.c:27), not on Windows (meson.build:223-226) | A bridge must be built; templates: hw/ipmi/ipmi_bmc_extern.c:494, 513 (async) or hw/remote/mpqemu-link.c:34-80 (synchronous, Linux-only per meson.build:210-212) |
| UART link between instances | Yes | -chardev socket/udp (qemu-options.hx:4097-4102) | Bytes arrive in host time, not aligned to virtual time |
| Ethernet between instances | Yes | -netdev stream/dgram/socket (qemu-options.hx:3075-3095; net/meson.build:1-17) | No time sync |
| CAN between instances | Linux only | can-host-socketcan (net/can/meson.build:3-5; docs/system/devices/can.rst:6) | Nothing on Windows; no M_CAN controller model |
| Shared memory/doorbell between MCU instances | Linux only, wired in code | ivshmem-flat (hw/misc/Kconfig:84-87; hw/misc/ivshmem-flat.c:449-450) | Not creatable from the CLI; no machine instantiates it |
| MCU-to-MCU I2C/SPI (target mode) | Precedent only | Aspeed I2C target + i2c_bus_master/send_async (hw/i2c/aspeed_i2c.c:1632; hw/i2c/core.c:185-235, 276-291) | No cross-instance bus; SSI has no target-mode controller |
| Several MCUs in one process | Yes (C) | ast2700fc: Cortex-A35 + 2 Cortex-M4 (hw/arm/aspeed_ast27x0-fc.c:67, 141, 175, 228) | Shared virtual clock, but the board must be written in C |
| Deterministic time, single instance | Yes | -icount shift=N,sleep=off (qemu-options.hx:5340-5360); instruction-exact MMIO timing (docs/devel/tcg-icount.rst:51-61) | Not across instances; incompatible with MTTCG (tcg-icount.rst:17-18) |
| Time sync across instances | No (building blocks only) | plugin time control (include/plugins/qemu-plugin.h:803-827; plugins/api-system.c:105-130; contrib/plugins/ips.c); plugins work on Windows (plugins/win32_linker.c) | A quantum-barrier protocol must be built |
| Record/replay | Single instance | docs/system/replay.rst:20-30 | External models are outside replay unless they go through recorded chardev/net channels |

## Code: I2C core: transfers must complete immediately (include/hw/i2c/i2c.h:7-10)
```c
/* The QEMU I2C implementation only supports simple transfers that complete
   immediately.  It does not support slave devices that need to be able to
   defer their response (eg. CPU slave interfaces where the data is supplied
   by the device driver in response to an interrupt).  */
```

## Code: A board is a hand-written netlist in C (B-L475E-IOT01A + DM163) (hw/arm/b-l475e-iot01a.c:88-110)
```c
    if (object_class_by_name(TYPE_DM163)) {
        object_initialize_child(OBJECT(machine), "dm163",
                                &s->dm163, TYPE_DM163);
        dev = DEVICE(&s->dm163);
        qdev_realize(dev, NULL, &error_abort);

        for (unsigned i = 0; i < NUM_DM163_INPUTS; i++) {
            object_initialize_child(OBJECT(machine), "gpio-out-splitters[*]",
                                    &s->gpio_splitters[i], TYPE_SPLIT_IRQ);
            gpio_out_splitter = DEVICE(&s->gpio_splitters[i]);
            qdev_prop_set_uint32(gpio_out_splitter, "num-lines", 2);
            qdev_realize(gpio_out_splitter, NULL, &error_fatal);

            qdev_connect_gpio_out(gpio_out_splitter, 0,
                qdev_get_gpio_in(DEVICE(&s->soc), dm163_input[i]));
            qdev_connect_gpio_out(gpio_out_splitter, 1,
                qdev_get_gpio_in(dev, i));
            gpio = dm163_input[i] / GPIO_NUM_PINS;
            pin = dm163_input[i] % GPIO_NUM_PINS;
            qdev_connect_gpio_out(DEVICE(&s->soc.gpio[gpio]), pin,
                qdev_get_gpio_in(DEVICE(gpio_out_splitter), 0));
        }
    }
```

## Code: Controller binds chip-select of user-created SPI flashes at reset (makes -device work) (hw/ssi/aspeed_smc.c:640-654)
```c
    for (i = 0; i < asc->cs_num_max; i++) {
        DeviceState *dev = ssi_get_cs(s->spi, i);
        if (dev) {
            Object *o = OBJECT(dev);

            if (!object_dynamic_cast(o, TYPE_M25P80)) {
                warn_report("Aspeed SMC %s.%d : Invalid %s device type",
                            BUS(s->spi)->name, i, object_get_typename(o));
                continue;
            }

            qemu_irq cs_line = qdev_get_gpio_in_named(dev, SSI_GPIO_CS, 0);
            qdev_connect_gpio_out_named(DEVICE(s), "cs", i, cs_line);
        }
    }
```

## Code: MPS2-TZ: internal I2C buses marked full, shield buses left for -device (hw/arm/mps2-tz.c:737-748)
```c
    /*
     * If this is an internal-use-only i2c bus, mark it full
     * so that user-created i2c devices are not plugged into it.
     * If we implement models of any on-board i2c devices that
     * plug in to one of the internal-use-only buses, then we will
     * need to create and plugging those in here before we mark the
     * bus as full.
     */
    if (extradata->i2c_internal) {
        BusState *qbus = qdev_get_child_bus(DEVICE(i2c), "i2c");
        qbus_mark_full(qbus);
    }
```

## Code: GPIO outputs are settable QOM link<irq> properties (basis of the untested qom-set rewiring idea) (hw/core/gpio.c:85-95)
```c
    memset(pins, 0, sizeof(*pins) * n);
    for (i = 0; i < n; ++i) {
        gchar *propname = g_strdup_printf("%s[%u]", name,
                                          gpio_list->num_out + i);

        object_property_add_link(OBJECT(dev), propname, TYPE_IRQ,
                                 (Object **)&pins[i],
                                 object_property_allow_set_link,
                                 OBJ_PROP_LINK_STRONG);
        g_free(propname);
    }
```

## Code: Fan-out primitive cannot be created from the CLI (hw/core/split-irq.c:74-75)
```c
    /* Reason: Needs to be wired up to work */
    dc->user_creatable = false;
```

## Code: Pin-mux 'right way' left unimplemented (Stellaris board) (hw/arm/stellaris.c:1291-1300)
```c
             * In theory the right way to model this would be:
             *  - Implement aux-function support in the PL061, with an
             *    extra set of AFIN and AFOUT GPIO lines (set up so that
             *    if a GPIO line is in auxfn mode the main GPIO in and out
             *    track the AFIN and AFOUT lines)
             *  - Wire the AFOUT for D0 up to either a line from the
             *    SSI controller that's pulled low around every transmit,
             *    or at least to an always-0 line here on the board
             *  - Make the ssd0323 OLED controller chipselect active-low
             */
```

## Code: Chip-only model breaks real firmware: micro:bit TWI stub (hw/i2c/microbit_i2c.c:5-8)
```c
 * This is a microbit-specific stub for the TWI controller on the nRF51 SoC.
 * We don't emulate I2C devices but the firmware probes the
 * accelerometer/magnetometer on startup and panics if they are not found.
 * Therefore we stub out the probing.
```

## Code: Documented -device attach of an I2C part on an Arm board (docs/specs/tpm.rst:386-392)
```console
  qemu-system-arm -M ast2600-evb -nographic \
    -kernel arch/arm/boot/zImage \
    -dtb arch/arm/boot/dts/aspeed-ast2600-evb.dtb \
    -initrd rootfs.cpio \
    -chardev socket,id=chrtpm,path=/tmp/mytpm1/swtpm-sock \
    -tpmdev emulator,id=tpm0,chardev=chrtpm \
    -device tpm-tis-i2c,tpmdev=tpm0,bus=aspeed.i2c.bus.12,address=0x2e
```

## Code: CAN host bridge compiled only on Linux (net/can/meson.build:1-7)
```meson
can_ss = ss.source_set()
can_ss.add(files('can_core.c', 'can_host.c'))
if host_os == 'linux'
  can_ss.add(files('can_socketcan.c'))
endif

system_ss.add_all(when: 'CONFIG_CAN_BUS', if_true: can_ss)
```

## Code: SERCOM I2C master reports completion through flags (async completion is possible) (PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6103-6107 (PIC32CM-JH_DFP 1.3.221))
```xml
        <register modes="I2CM" name="INTFLAG" offset="0x18" rw="RW" size="1" atomic-op="clear:INTFLAG" initval="0x00" caption="I2CM Interrupt Flag Status and Clear">
          <bitfield name="MB" caption="Master On Bus Interrupt" mask="0x1"/>
          <bitfield name="SB" caption="Slave On Bus Interrupt" mask="0x2"/>
          <bitfield name="ERROR" caption="Combined Error Interrupt" mask="0x80"/>
        </register>
```

## Open questions
- Does QMP qom-set on a SoC model's gpio-out link properties (e.g. 'unnamed-gpio-out[5]' -> '/machine/peripheral/led0/unnamed-gpio-in[0]') reliably rewire GPIO nets while paused at -S? Nothing was built or run here. If it works, a first board-netlist loader could be a plain QMP client with no QEMU patches.
- On MPS2-TZ, can shield 1's I2C bus be reached from the CLI at all, given that all five SBCon buses are named 'i2c'? Inferred from qbus_find, not executed.
- Would upstream accept a generic declarative board machine for MCUs (netlist + pad layer), or would Microchip carry it downstream? The 2021-22 QMP machine-creation series stalled.
- What host OS mix do target customers use? If they must run natively on Windows, SocketCAN, vhost-user, multiprocess, ivshmem-flat and loadable modules are all out. Is WSL2 acceptable?
- GPL boundary: QEMU as a whole is GPLv2 (LICENSE:8-9), so customer-proprietary part models linked in-process would be part of a GPLv2 program. Does Microchip need an out-of-process part API so customers can keep their models private? This needs legal review.
- How much timing fidelity does customer firmware need (bit-banged protocols, pulse-width capture, PWM/ADC feedback loops)? icount gives exact MMIO timing but is single-threaded, so the performance budget per board size is unknown.
- Which wire protocol for external parts: a chardev framing like ipmi-bmc-extern, a synchronous request/reply like multiprocess QEMU, SystemC/TLM, or Renode-style? This must be reconciled with the separate co-simulation research run.
- Can MPLAB SIM's existing stimulus concepts (SCL stimulus, pin and register injection) map onto QOM-property injection plus a pad layer, so customers keep their current workflows?
