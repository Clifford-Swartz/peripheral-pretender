# cosim-options

## Summary
I checked these findings against upstream QEMU master (11.1.50, 602e4f2da1). The only first-class way to put a peripheral model on a Cortex-M machine is a native QOM sysbus device, written in C or in Rust (still disabled by default): MemoryRegionOps plus qemu_set_irq, wired by the SoC code to the armv7m NVIC. Comparable in-tree MCU peripheral models are about 300-700 LOC each, plus a few hundred LOC of qtest. Upstream has no reusable external-model bridge for such a machine: multi-process QEMU (x-pci-proxy-dev) needs PCI+PCIe+KVM, and the vfio-user client is vfio-user-pci (Linux+PCI, UNIX sockets only). vfio-platform was removed in 10.2, ivshmem-flat only offers shared RAM plus doorbells on Linux hosts, and the tree has no remote-port or mmio-sockdev. A bridge therefore has to be a custom sysbus device; the upstream PCI proxy (hw/remote/proxy.c + mpqemu-link.c + message.c, about 885 LOC) is a working template for blocking per-access RPC that drops the BQL while it waits. Blocking costs one IPC round trip per access: about 7.7 µs over a Unix socket, about 14 µs over TCP and about 0.2 µs over spinning shared memory, from a Linux ping-pong benchmark. It is deterministic only if every message carries QEMU virtual time, the external model reports its next event deadline (with -icount sleep=off, QEMU otherwise skips or stalls on events it cannot see), and IRQ changes are applied on virtual-time timers rather than when the socket becomes readable. For SystemC/RTL, the mature paths are AMD/Xilinx remote-port + libsystemctlm-soc (MIT library and protocol header; the QEMU side is GPL and exists only in Xilinx's fork, now at 10.2.4) and Qualcomm QBox (libqemu, not upstream). Reusing remote-port's timestamped read/write/interrupt/sync packet format for a Microchip bridge would keep the SystemC path open. Verilated Microchip RTL is a validation tier, not the everyday twin. Published figures put full-SoC Verilator at about 0.1 MIPS against 170-1000 MIPS for functional simulators, and SimBricks' synchronized QEMU-icount+Verilator run took about 490x longer than its unsynchronized KVM+Verilator run of the same workload. TCG plugins (API v7; the header is now include/plugins/qemu-plugin.h and the minimum accepted version is also 7) cannot implement or intercept guest MMIO, because memory callbacks fire after the access. Since v5/v6 they can issue bus reads and writes through address_space_rw, write CPU registers, set the PC and drive time. That makes them suited to MMIO tracing, register coverage, IRQ logging and lockstep diffing, not to modeling; the user docs still describe them as passive. qtest (-accel qtest: the vCPUs never run and the client owns QEMU_CLOCK_VIRTUAL) is the deterministic register-level unit-test and fuzzing harness. For runtime stimulus and observation, upstream uses QMP qom-set/qom-get on device properties (ADC adci[N], tmp105 temperature, per-pin GPIO bools); there is no generic pin or GPIO channel to the outside. Windows hosts keep TCG, plugins, AF_UNIX/TCP sockets, named pipes and COM ports. They lose SocketCAN (the only CAN host backend), pty, vfio-user/multi-process and ivshmem, and no upstream CAN controller model matches the PIC32CM JH CAN module. Everything linked into QEMU (devices, plugins, Rust) is GPL; the FSF FAQ treats socket-connected programs as separate unless they exchange intimate internal data structures. Microchip legal and process review is needed on that boundary and on QEMU's policy of declining AI-generated contributions before anything goes upstream. Recommended layering: L0 is native models for boot-critical and common peripherals (register skeletons generated from ATDF, behavior written by people). L1 is QEMU-native board I/O plus QMP. L2 is a timestamped, remote-port-compatible bridge for plant and confidential models. L3 is optional internal Verilator co-simulation used to validate L0. Renode (MIT; C#/Python peripherals; Verilator and SystemC co-simulation; an existing Microchip partnership) is the main alternative, and Unicorn only fits unit-test or fuzzing harnesses.

## Findings

1. (high) [LOAD-BEARING] [native-qom-mechanism] Native QOM sysbus devices are QEMU's standard mechanism for on-chip peripherals. A device registers a MemoryRegion with MemoryRegionOps (memory_region_init_io + sysbus_init_mmio) and IRQ outputs (sysbus_init_irq), and takes backends such as chardev and clock as properties. The SoC code maps it with sysbus_mmio_map and wires its IRQs to the armv7m NVIC via qdev_get_gpio_in(armv7m, n). Guest MMIO from TCG reaches the model through the softmmu slow path (io_prepare then memory_region_dispatch_read), an in-process call. Sysbus devices are not user-creatable by default, so the machine or SoC code must instantiate and wire every peripheral, a bridge device included.
   detail: Interrupt delivery path: device qemu_set_irq() -> wired qdev GPIO -> armv7m NVIC input -> CPU takes the interrupt at the next TB boundary. Inter-peripheral signals such as EVSYS channels and DMAC triggers map naturally onto named qdev GPIO lines. Per-access cost is a function call; I found no measured nanosecond figure.
   - ev: hw/char/stm32l4x5_usart.c:519-555
   - ev: hw/arm/stm32l4x5_soc.c:313-314
   - ev: hw/arm/stm32l4x5_soc.c:320-328
   - ev: accel/tcg/cputlb.c:1273-1288
   - ev: accel/tcg/cputlb.c:1949
   - ev: hw/core/sysbus.c:300-310

2. (high) [LOAD-BEARING] [native-effort] In-tree MCU peripheral models give a grounded effort reference. Typical peripherals are about 300-700 LOC: STM32L4x5 USART 657, GPIO 478, EXTI 293, SYSCFG 282, MAX78000 UART 292, AVR USART 320. A clock/reset controller is about 1.5k LOC (STM32L4x5 RCC 1477) and CAN controllers 684-1461 LOC. SoC glue is 232-494 LOC and per-peripheral qtests 395-580 LOC. Reusable helpers include the register API (RegisterAccessInfo with ro/w1c/reset/cor/rsvd/unimp masks and pre/post read/write hooks), the Clock API, ptimer, and unimplemented-device, which logs via LOG_UNIMP and reads as 0, for regions not yet modeled.
   detail: The LOC counts are measured. Turning them into person-weeks is judgment and depends heavily on how much hidden behavior the peripheral has (SYNCBUSY, clock gating, DMA triggers, EVSYS).
   - ev: wc -l: hw/char/stm32l4x5_usart.c=657, hw/gpio/stm32l4x5_gpio.c=478, hw/misc/stm32l4x5_exti.c=293, hw/misc/stm32l4x5_rcc.c=1477, hw/arm/stm32l4x5_soc.c=494, hw/char/max78000_uart.c=292, hw/arm/max78000_soc.c=232, hw/net/can/flexcan.c=1461, hw/net/can/ctucan_core.c=684, tests/qtest/stm32l4x5_usart-test.c=395, tests/qtest/stm32l4x5_gpio-test.c=580
   - ev: include/hw/core/register.h:23-60
   - ev: docs/devel/clocks.rst:1-20
   - ev: hw/arm/Kconfig:689-695 (MSF2 selects PTIMER, UNIMP)
   - ev: include/hw/misc/unimp.h:29-52
   - ev: hw/misc/unimp.c:25-36

3. (medium) [native-atdf-skeletons] The PIC32CM JH ATDF holds the per-register metadata needed to generate RegisterAccessInfo skeletons: initval maps to reset, rw to ro masks, atomic-op='clear:INTFLAG' to w1c, access='WSYNC' plus the SYNCBUSY register to write-synchronization semantics, and register 'modes' (SERCOM USART_INT/SPI/I2C aliasing) to mode-dependent layouts. The SVD for the same part has no modifiedWriteValues entries, so W1C semantics are lost in SVD and ATDF is the better generator input. QEMU accepts generator-produced boilerplate only when humans finish it under clearly defined copyright and licensing, and it does not accept AI-generated content.
   detail: The data facts are high confidence. Whether ATDF captures every behavior a model needs (for example cross-register side effects) is not established. AMD/Xilinx uses register-database generators (Xregdb, memmap-gen) for model skeletons, which is a precedent; see the fork-precedents finding.
   - ev: DFP 1.3.221 PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5836 (mode USART_INT)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5904 (CTRLA access=WSYNC)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6137 (INTFLAG atomic-op=clear:INTFLAG)
   - ev: PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:6216 (SYNCBUSY)
   - ev: grep -c modifiedWriteValues PIC32CM-JH01/svd/PIC32CM5164JH01100.svd -> 0
   - ev: include/hw/core/register.h:45-60
   - ev: docs/devel/code-provenance.rst:280-286

4. (high) [native-rust] Rust device support targets SysBusDevice in safe Rust. hwcore::sysbus, hwcore::qdev, system::memory, util::timer, migration::vmstate and qom are marked 'stable', and pl011 and hpet are sample devices. Rust is disabled by default (meson option 'rust' = disabled) and needs rustc 1.83 or newer. Rust code is still GPL-2.0-or-later and linked into QEMU. The Rust pl011 is 1181 LOC against 750 LOC for the C version.
   detail: Rust is viable for new models but adds toolchain and build complexity. Rust-on-Windows build support was not verified.
   - ev: docs/devel/rust.rst:1-12
   - ev: docs/devel/rust.rst:74
   - ev: docs/devel/rust.rst:112-119
   - ev: docs/devel/rust.rst:142-162
   - ev: meson_options.txt:388-389
   - ev: Cargo.toml:12
   - ev: wc -l rust/hw/char/pl011/src/*.rs=1181 vs hw/char/pl011.c=750

5. (high) [LOAD-BEARING] [determinism-native] With -icount, QEMU_CLOCK_VIRTUAL is derived from instruction count. MMIO-performing instructions are re-translated as single-instruction blocks so the count is exact at each I/O access. With sleep=off, virtual time jumps to the next timer deadline when the CPU sleeps, which makes guest timing deterministic. icount is incompatible with MTTCG (irrelevant for a single-core M0+) and is the basis of record/replay, which replays serial and network input. QEMU states that icount is deterministic but not cycle-accurate.
   - ev: docs/devel/tcg-icount.rst:10-22
   - ev: docs/devel/tcg-icount.rst:51-63
   - ev: accel/tcg/cputlb.c:1283-1284 (cpu_io_recompile)
   - ev: qemu-options.hx:5340-5360
   - ev: docs/system/replay.rst:14-31
   - ev: docs/system/replay.rst:86-93

6. (medium) [LOAD-BEARING] [sync-external-deadline] Any model outside QEMU has to feed its next event time into QEMU's timer system. Under -icount sleep=off, virtual time 'will not advance if no timer is enabled' and otherwise jumps straight to the next QEMU timer deadline. An external timer, ADC conversion or UART that QEMU cannot see would therefore be skipped, or would leave firmware sleeping in WFI indefinitely. A bridge protocol needs per-message virtual timestamps and a next-deadline field, and remote-port's busaccess, interrupt and sync payloads all carry a uint64 timestamp.
   detail: This is a design inference from documented icount semantics, not a measured failure. Applying IRQ edges from the external model through QEMU_CLOCK_VIRTUAL timers at the model-supplied timestamp, or piggy-backing them on MMIO replies on the vCPU thread, avoids nondeterminism from socket-readiness timing.
   - ev: qemu-options.hx:5352-5357
   - ev: util/qemu-timer.c:701-720
   - ev: https://github.com/Xilinx/libsystemctlm-soc/blob/master/libremote-port/remote-port-proto.h (enum rp_cmd: hello, cfg, read, write, interrupt, sync, ats; timestamp fields)

7. (high) [LOAD-BEARING] [bridge-upstream-status] Upstream has no generic sysbus MMIO-forwarding device usable on a Cortex-M machine. The tree contains no remote-port (grep hits are Xen remote_port fields only) and no mmio-sockdev. Multi-process QEMU depends on PCI && PCI_EXPRESS && KVM, uses x-pci-proxy-dev (parent TYPE_PCI_DEVICE) with an x-remote machine, and its maintainer status is 'Odd Fixes'. The vfio-user client is vfio-user-pci (parent TYPE_VFIO_PCI_DEVICE): VFIO_PCI depends on LINUX && PCI, only UNIX sockets are accepted, the docs describe it as emulating (PCI) devices, and the vfio-user server is Linux-only. The non-PCI vfio-platform was removed in 10.2 with 'PCIe passthrough shall be the mainline solution'. QEMU Cortex-M machines have no PCI bus and Windows has no VFIO, so none of these paths works for a PIC32CM JH twin.
   - ev: grep -rIl 'remote-port|remote_port' -> only hw/xen, hw/display/xenfb.c, hw/usb/xen-usb.c, include/hw/xen/*
   - ev: hw/remote/Kconfig:1-8
   - ev: hw/remote/proxy.c:228-236
   - ev: docs/system/multi-process.rst:12
   - ev: docs/system/multi-process.rst:59-66
   - ev: MAINTAINERS:4561-4564
   - ev: hw/vfio-user/Kconfig:3-6
   - ev: hw/vfio/Kconfig:7-12
   - ev: hw/vfio-user/pci.c:19
   - ev: hw/vfio-user/pci.c:452-458
   - ev: hw/vfio-user/pci.c:479-480
   - ev: docs/system/devices/vfio-user.rst:7-12
   - ev: meson.build:214-215
   - ev: docs/about/removed-features.rst:1493-1500
   - ev: MAINTAINERS:4590-4594

8. (high) [bridge-ivshmem-flat] ivshmem-flat is the only upstream non-PCI device designed so Cortex-M machines can talk to the outside world. It provides shared RAM plus doorbell interrupts through an ivshmem server. It builds only on Linux hosts, is not user-creatable ('Must be wired up in code'), and is not a register bridge: there is no per-access forwarding and no read/write side effects.
   - ev: docs/system/devices/ivshmem-flat.rst:4-29
   - ev: hw/misc/Kconfig:84-87
   - ev: hw/misc/ivshmem-flat.c:435-450

9. (medium) [LOAD-BEARING] [bridge-pattern-and-cost] The upstream PCI proxy is the reference for a blocking-per-access bridge. MemoryRegionOps read/write build a message, send it, and wait for the reply on the vCPU thread, dropping the BQL around socket I/O so QMP, chardevs and timers keep running. The device also registers a migration blocker. Its size (proxy.c 396 + mpqemu-link.c 263 + message.c 226 = 885 LOC) suggests a sysbus equivalent on the QEMU side would be about 0.5-1k LOC. Each access costs one IPC round trip. In goldsborough/ipc-bench ping-pong on Linux (i5-4590S): Unix domain sockets 130,372 msg/s (~7.7 µs), TCP 70,221 msg/s (~14 µs), shared memory 4,702,557 msg/s (~0.21 µs). A blocking socket bridge therefore caps register polling at roughly 10^5 accesses per second. QEMU's own design doc proposes serving side-effect-free loads from a shadow image.
   detail: The code facts are high confidence. The IPC figures come from an old third-party benchmark on Linux; per-µs values are my reciprocals of its msg/s rates. Windows latencies were not measured. Mitigations: posted (non-blocking) writes, shadow registers for reads without side effects, and time-quantum synchronization.
   - ev: hw/remote/proxy.c:246-294
   - ev: hw/remote/proxy.c:118-121
   - ev: hw/remote/mpqemu-link.c:36
   - ev: hw/remote/mpqemu-link.c:58-78
   - ev: hw/remote/mpqemu-link.c:189-215
   - ev: docs/devel/multi-process.rst:678-681
   - ev: https://github.com/goldsborough/ipc-bench

10. (medium) [bridge-irq-and-prior-art] The IRQ back-path for a bridge is: external message, then a QEMU-side handler, then qemu_set_irq on the bridge device's sysbus IRQ, then the NVIC. An out-of-tree 'mmio-sockdev' (QEMU 8.1.0 fork, dual Cortex-M4 SoC) shows the approach works: per-access binary framing over TCP, a separate TCP IRQ channel, and -icount shift=5 so device timers follow virtual time. Its asynchronous IRQ channel, however, is a nondeterminism risk. SimBricks (SIGCOMM'22) reports that QEMU's device interface 'expects accesses to return immediately'. To model MMIO latency they aborted and retried the instruction, and they found QEMU 5.1.92 'no longer fully deterministic even with instruction counting'.
   - ev: https://github.com/Kexu1984/qemu_device
   - ev: https://arxiv.org/pdf/2012.14219 (section 6.2, QEMU)
   - ev: hw/core/irq.c:29-38

11. (medium) [LOAD-BEARING] [bridge-windows] On a Windows host a bridge can use AF_UNIX (via afunix.h, with fallback definitions), TCP sockets, Windows named pipes (char-pipe) or COM ports. The fd-passing fast paths that vfio-user and vhost rely on (sharing memory and eventfds over AF_UNIX) are not available, so a shared-memory transport would have to be implemented inside the bridge device itself.
   detail: The lack of fd passing on Windows is inferred from the protocol rationale and from the Linux-only gating of the code that uses it; I did not test it.
   - ev: include/system/os-win32.h:33-47
   - ev: meson.build:2616-2618
   - ev: chardev/char-pipe.c:32-64
   - ev: chardev/char-serial.c:32-47
   - ev: docs/interop/vfio-user.rst:28-35
   - ev: docs/about/build-platforms.rst:148-166

12. (high) [LOAD-BEARING] [systemc-remote-port] AMD/Xilinx remote-port + libsystemctlm-soc is the most mature open QEMU-SystemC/TLM-RTL path, but its QEMU side exists only in Xilinx's QEMU fork, whose master VERSION is 10.2.4 against upstream 11.1.50. remote-port.h is GPL ('This code is licensed under the GNU GPL'). The fork adds -machine-path DIR (machine nodes and shared memory) and -sync-quantum ('Max time between synchroniation, nanoseconds') and is used with -icount. libsystemctlm-soc is MIT-licensed, including the remote-port protocol header. It includes tlm2apb, tlm2axi, tlm2axilite, tlm2wishbone and other bridges (no tlm2ahb) and uses Verilator to bring RTL into SystemC. The cosim demo connects over unix:/tmp/qemu/qemu-rport-... sockets.
   detail: The fork's machines target Zynq, ZynqMP, Versal and MicroBlaze. Using it for a PIC32CM JH means porting the remote-port devices into a tree that has the Microchip machine; I did not verify the effort or any existing Cortex-M support. AMD's 2022 slides note that BootROM and PUF models are internal-only and that 'Successfully identified bug in IP RTL during integration'.
   - ev: local grep: no remote-port in upstream tree
   - ev: https://github.com/Xilinx/qemu/blob/master/include/hw/remote-port.h
   - ev: https://raw.githubusercontent.com/Xilinx/qemu/master/qemu-options.hx (machine-path, sync-quantum entries)
   - ev: https://raw.githubusercontent.com/Xilinx/qemu/master/VERSION
   - ev: https://github.com/Xilinx/libsystemctlm-soc
   - ev: https://github.com/Xilinx/libsystemctlm-soc/blob/master/LICENSE
   - ev: https://github.com/Xilinx/libsystemctlm-soc/tree/master/tlm-bridges
   - ev: https://github.com/Xilinx/libsystemctlm-soc/blob/master/libremote-port/remote-port-proto.h
   - ev: https://github.com/Xilinx/systemctlm-cosim-demo

13. (medium) [systemc-qbox] Qualcomm QBox embeds QEMU in SystemC as a TLM-2.0 model through libqemu/libqemu-cxx. The wrapper is BSD-3-Clause, the CPU list includes Cortex-M7 and M55, and documented hosts are Ubuntu 20.04-24.04 and macOS. Time sync uses either the quantum keeper (default) or MCIPS, a TCG-plugin-based instruction-counting sync that works with TCG only; quantum_ns = 100000 is the recommended start. MINRES (Feb 2026) reports that because libqemu is not part of official QEMU, 'Each QEMU update requires significant effort to realign'.
   - ev: https://github.com/quic/qbox
   - ev: https://github.com/quic/qbox/blob/main/LICENSE
   - ev: https://www.minres.com/?p=1430
   - ev: upstream grep: no libqemu/SystemC in meson.build or docs

14. (medium) [LOAD-BEARING] [rtl-cosim-cost] Verilator co-simulation of Microchip-owned peripheral RTL is the top-fidelity tier, but it is costly. Verilator claims single-threaded models run 'over 10x faster than standalone SystemC' and 'about 100 times faster than interpreted Verilog simulators' (LGPL-3.0 or Artistic-2.0). The GVSoC paper's comparison table lists Verilator, for full-platform cycle-accurate simulation, at 0.1 MIPS against 170 MIPS for Spike and 1000 MIPS for OVPSim. In SimBricks, the same 20 s netperf workload took 31 s with unsynchronized QEMU-KVM plus Verilated Corundum (giving unrealistic 81 Mbps and 3.4 ms results) and 4 h 13 m with synchronized, deterministic QEMU-icount plus Verilated Corundum (500 ns sync interval), about 490x. Verilator could not simulate the vendor IP Corundum uses, which SimBricks reimplemented in C++.
   detail: The SimBricks ratio also includes the switch from KVM to TCG-icount, and both designs are much larger than a single MCU peripheral. The speed of one Verilated SERCOM/TC/CAN block must be benchmarked. Analog parts (ADC front end, oscillators) are not in RTL and need behavioral models. Third-party or encrypted IP inside Microchip RTL may not be Verilator-compatible or licensable for distribution.
   - ev: https://www.veripool.org/verilator/
   - ev: https://www.verilator.org/guide/latest/overview.html
   - ev: https://arxiv.org/pdf/2201.08166 (Table II)
   - ev: https://arxiv.org/pdf/2012.14219 (Table 1; section 6.3; section 7.1 setup: 500 ns sync, 250 MHz RTL clock)

15. (high) [LOAD-BEARING] [plugins-api] In this tree the plugin header lives at include/plugins/qemu-plugin.h. QEMU_PLUGIN_VERSION is 7 and QEMU_PLUGIN_MIN_VERSION is also 7, so out-of-tree plugins must be rebuilt as the API moves. Memory callbacks are registered per translated instruction and 'called after a successful load or store'; faults are invisible. A plugin therefore cannot supply or override the value of a guest MMIO load, intercept an access, or implement a device. It can identify MMIO accesses (hwaddr_is_io, phys_addr, device_name, which returns the MemoryRegion name) and read the accessed value. Since v5/v6 it can also write CPU registers, set the PC, and issue its own physical-address reads and writes through address_space_rw on the vCPU's address space; these do execute device MMIO handlers and their side effects. The user-facing docs still say plugins 'are unable to change the system state'.
   - ev: include/plugins/qemu-plugin.h:56-91
   - ev: plugins/plugin.h:19
   - ev: plugins/loader.c:214-217
   - ev: include/plugins/qemu-plugin.h:764-781
   - ev: docs/devel/tcg-plugins.rst:117-123
   - ev: include/plugins/qemu-plugin.h:690
   - ev: include/plugins/qemu-plugin.h:706-742
   - ev: plugins/api-system.c:70-97
   - ev: include/plugins/qemu-plugin.h:1120-1157
   - ev: include/plugins/qemu-plugin.h:1245
   - ev: include/plugins/qemu-plugin.h:1279
   - ev: plugins/api.c:552-627
   - ev: docs/about/emulation.rst:184-191

16. (high) [LOAD-BEARING] [plugins-good-for] Plugins fit observability and validation. contrib/plugins/hwprofile.c profiles MMIO per device, by register offset ('pattern') or by guest PC ('source'). Combining memory callbacks with unimplemented-device regions gives coverage of unimplemented registers. Discontinuity callbacks report interrupt and exception entry with from_pc/to_pc, which supports IRQ latency logs. contrib/plugins/lockstep.c finds where two QEMU instances diverge, which could compare a native model against a bridged or RTL-backed one. ips.c and QBox's MCIPS throttle or synchronize time. Plugins build and load on Windows hosts through a delay-load import library. Memory callbacks only see CPU-originated accesses, not DMA performed by device models.
   detail: That lockstep is applicable to native-vs-RTL differential runs is my inference; the plugin was written to debug TCG divergences and notes mixed results once asynchronous events are involved.
   - ev: docs/about/emulation.rst:613-648
   - ev: contrib/plugins/hwprofile.c:197-207
   - ev: contrib/plugins/hwprofile.c:258
   - ev: docs/about/emulation.rst:580-600
   - ev: docs/about/emulation.rst:805-829
   - ev: include/plugins/qemu-plugin.h:158-199
   - ev: include/plugins/qemu-plugin.h:302
   - ev: plugins/meson.build:28-75
   - ev: plugins/win32_linker.c:1-30
   - ev: include/plugins/qemu-plugin.h:764-766
   - ev: https://github.com/quic/qbox

17. (medium) [plugins-time-control] qemu_plugin_request_time_control and qemu_plugin_update_ns let one plugin advance QEMU_CLOCK_VIRTUAL through qemu_clock_advance_virtual_time; requesting control also adds a migration blocker. In this tree only the qtest accelerator implements set_virtual_clock, and TCG with icount only overrides get_virtual_clock. Under TCG the call can only run timers up to the target time, which is how ips.c throttles. Whether a plugin could act as a deterministic external time master for co-simulation needs an experiment.
   - ev: include/plugins/qemu-plugin.h:802-827
   - ev: plugins/api-system.c:102-131
   - ev: util/qemu-timer.c:701-720
   - ev: system/cpus.c:207-237
   - ev: accel/qtest/qtest.c:64-71
   - ev: accel/tcg/tcg-accel-ops.c:205-218
   - ev: contrib/plugins/ips.c:1-31

18. (high) [LOAD-BEARING] [qtest] qtest is an internal ('HXCOMM Internal use') line-based protocol over a chardev. libqtest starts QEMU with -accel qtest, which uses dummy vCPU threads, so no firmware executes, and the client is 'completely in charge of the QEMU_CLOCK_VIRTUAL' through clock_step and clock_set. Available commands: read/write/b64/memset on the first CPU's address space, set_irq_in to drive any qdev gpio-in, irq_intercept_in (now a non-destructive observer; one device at a time; named in-GPIOs unsupported) and irq_intercept_out (replaces the wiring). Combined with QMP qom-get and qom-set, this is how upstream tests MCU peripherals deterministically. The generic-fuzz target can fuzz MMIO spaces. Clock commands require the qtest accelerator, so qtest is a register-level test harness and not a co-simulation interface.
   detail: A -qtest chardev can be attached when running under TCG (qtest_server_init runs whenever -qtest is given: system/vl.c:2103-2104), and the memory and IRQ commands do not check qtest_enabled(). Clock commands would then be unavailable. I did not test this; treat it as unsupported.
   - ev: qemu-options.hx:5655-5657
   - ev: tests/qtest/libqtest.c:453-470
   - ev: accel/qtest/qtest.c:30-72
   - ev: system/qtest.c:69-261
   - ev: system/qtest.c:347-354
   - ev: system/qtest.c:379-466
   - ev: system/qtest.c:700-760
   - ev: hw/core/irq.c:29-38
   - ev: hw/core/irq.c:127-133
   - ev: tests/qtest/stm32l4x5_gpio-test.c:124-158
   - ev: tests/qtest/npcm7xx_adc-test.c:118-140
   - ev: docs/devel/testing/fuzzing.rst:135-162
   - ev: docs/devel/testing/qtest.rst:11-14
   - ev: docs/devel/testing/qtest.rst:25-27

19. (medium) [LOAD-BEARING] [qmp-qom-control] Upstream runtime stimulus and observation for plant values uses QMP qom-set and qom-get on device QOM properties. Examples: ADC analog inputs as adci[N] and vref properties, an I2C temperature sensor's 'temperature' (millidegrees C), and per-pin GPIO bool properties (Aspeed 'gpio<group><n>'). There is no generic external pin or GPIO channel: no hw/gpio device takes a chardev, and the LED model's state is visible only through trace events. A twin must define explicit QOM properties per peripheral, or a custom pin-bridge device, and observers must poll.
   detail: The facts are high confidence. Two points are not verified: that push-style QMP events would need QAPI schema changes (qapi/ is not checked out), and that QMP writes land at points not deterministic relative to guest execution unless the VM is paused (this is reasoning from main-loop and vCPU concurrency).
   - ev: hw/adc/npcm7xx_adc.c:280-289
   - ev: hw/sensor/tmp105.c:409-414
   - ev: tests/qtest/tmp105-test.c:39-57
   - ev: hw/gpio/aspeed_gpio.c:1476-1494
   - ev: tests/qtest/aspeed_gpio-test.c:42-82
   - ev: grep DEFINE_PROP_CHR in hw/gpio hw/misc hw/adc hw/sensor -> only ivshmem-flat.c, ivshmem-pci.c
   - ev: hw/misc/led.c:38-58

20. (high) [chardev-uart] UART backends built on Windows are socket (TCP and AF_UNIX), Windows named pipes, COM-port serial, file, stdio/console, null, ringbuf, udp and mux. pty, fd and parallel are POSIX-only.
   - ev: chardev/meson.build:1-28
   - ev: chardev/char-pipe.c:32-64
   - ev: chardev/char-serial.c:32-47

21. (high) [LOAD-BEARING] [can] CAN support consists of a generic can-bus object plus an abstract can-host backend class with connect and disconnect methods. The only host backend, SocketCAN, is built only on Linux; the docs say 'at this time only Linux SocketCAN is supported'. In-tree controller models are SJA1000 (PCI cards), CTU CAN FD, FlexCAN, Xilinx ZynqMP CAN and Versal CANFD. None of them is the PIC32CM JH CAN module (ATDF module CAN, id U2003), so that controller needs a new model. Windows users would also need a new CanHostClass backend, for example to a vendor CAN API or a TCP bridge.
   - ev: net/can/meson.build:1-7
   - ev: docs/system/devices/can.rst:3-7
   - ev: docs/system/devices/can.rst:40-45
   - ev: include/net/can_host.h:44-49
   - ev: net/can/can_host.c:90-93
   - ev: ls hw/net/can: can_sja1000.c ctucan_core.c flexcan.c xlnx-zynqmp-can.c xlnx-versal-canfd.c ...
   - ev: DFP PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:1983

22. (high) [i2c-ssi-board] Board-level slave devices attach through QEMU's I2C bus (I2CSlaveClass send/recv/event) and SSI bus (SSIPeripheralClass transfer/set_cs). A SERCOM model in I2C or SPI master mode should create an I2CBus or SSIBus so existing sensor and flash models (tmp105, m25p80) and new plant models can plug in, with values set via QOM.
   - ev: include/hw/i2c/i2c.h:26-46
   - ev: include/hw/ssi/ssi.h:34-48
   - ev: docs/devel/ssi.rst:1-10
   - ev: hw/sensor/tmp105.c:409-414

23. (medium) [snapshot-migration] External models break QEMU snapshot and migration unless the bridge also saves and restores the external state. The upstream proxy registers a migration blocker, and plugin time control adds one as well. Native devices with VMState support savevm/loadvm checkpoints, which a twin can use for what-if branches.
   - ev: hw/remote/proxy.c:118-121
   - ev: plugins/api-system.c:105-115
   - ev: hw/adc/npcm7xx_adc.c:254-255

24. (medium) [LOAD-BEARING] [licensing] QEMU as a whole is GPLv2. In-tree C and Rust models are GPL (the Rust workspace license is GPL-2.0-or-later). The plugin header is GPL-2.0-or-later, and plugins are dynamically loaded into the QEMU process. The FSF GPL FAQ says pipes, sockets and command-line arguments are 'normally used between two separate programs' unless the semantics are 'intimate enough, exchanging complex internal data structures'. It says dynamically linked plug-ins that call each other and share data structures 'form a single combined program', and that shared memory with complex data structures is 'pretty much equivalent to dynamic linking'. A narrow register-level socket protocol is therefore the conventional way to keep external models separate, but this needs Microchip legal review. Component licenses: libsystemctlm-soc and the remote-port protocol header MIT (QEMU-side remote-port GPL), QBox wrapper BSD-3 (links libqemu), Renode MIT, Unicorn GPLv2, Verilator LGPL-3.0 or Artistic-2.0.
   detail: This is not legal advice. Search results also point to a 2019 qemu-devel thread titled 'TCG plugins and the GPL'; I could not open it, so I do not characterize its conclusions.
   - ev: LICENSE:1-15
   - ev: Cargo.toml:12
   - ev: include/plugins/qemu-plugin.h:5
   - ev: https://www.gnu.org/licenses/gpl-faq.en.html (MereAggregation, GPLPlugins)
   - ev: https://github.com/Xilinx/libsystemctlm-soc/blob/master/LICENSE
   - ev: https://github.com/quic/qbox/blob/main/LICENSE
   - ev: https://github.com/renode/renode
   - ev: https://github.com/unicorn-engine/unicorn
   - ev: https://www.veripool.org/verilator/

25. (high) [LOAD-BEARING] [upstreaming-policy] QEMU's policy is to 'DECLINE any contributions which are believed to include or derive from AI generated content'. The policy exempts AI used for research, static analysis or debugging, as long as its output is not contributed, and the repository's AGENTS.md repeats it. Generated files are usually inappropriate unless the output is boilerplate subsequently human-authored, with clear licensing. New hardware is expected to come with qtests, and boards need active maintainers; the recent Cortex-M MAX78000FTHR board is already 'Orphan'. Upstream already carries Microchip-family silicon: PolarFire SoC Icicle Kit (Odd Fixes), SmartFusion2 / Emcraft SF2 (Maintained), and AVR ATmega Arduino boards.
   - ev: docs/devel/code-provenance.rst:232-299
   - ev: AGENTS.md:1-18
   - ev: docs/devel/testing/qtest.rst:25-27
   - ev: MAINTAINERS:904-910
   - ev: MAINTAINERS:1240-1263
   - ev: MAINTAINERS:1315-1334
   - ev: MAINTAINERS:1795-1810
   - ev: hw/riscv/Kconfig:12-24
   - ev: hw/avr/Kconfig

26. (medium) [fork-precedents] Silicon-vendor precedents point both ways. Espressif maintains a QEMU fork per chip that emulates 'the CPU, memory, and several peripherals', ships prebuilt binaries including x86_64 Windows, and integrates it into its tools as 'idf.py qemu monitor/gdb', a template for MPLAB X integration. AMD/Xilinx runs a fork, upstreams frameworks (Register, Clock and Reset APIs), keeps some models internal-only, and generates model skeletons from register databases. xPack/GNU MCU Eclipse QEMU, with SVD-derived STM32 peripherals, forked because upstream and it 'could not agree on a common strategy'; its qemu-system-gnuarmeclipse binary has since reached end-of-life.
   detail: The end-of-life status of the gnuarmeclipse binary comes from search-result summaries of the xPack npm page and a 2023 qemu-arm post (lists.nongnu.org/archive/html/qemu-arm/2023-02/msg01555.html), neither of which I opened directly.
   - ev: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/tools/qemu.html
   - ev: Xilinx 'QEMU Overview' slides, E. Iglesias, 2022-04-19 (Linaro wiki attachment QEMU_overview_2022_04_19_v2_public.pdf), pages 3, 6, 8, 18-19
   - ev: https://lists.endsoftwarepatents.org/archive/html/qemu-arm/2020-02/msg00255.html
   - ev: https://lists.endsoftwarepatents.org/archive/html/qemu-arm/2020-02/msg00260.html

27. (medium) [alt-renode] Renode is MIT-licensed and ships packages for Windows, Linux and macOS. Peripherals are written in C# (there is a modeling guide); Python peripherals use IronPython with Python-2 syntax, with request.IsRead, IsWrite, Value, Offset and Length. HDL co-simulation works with Verilator either as an in-process library (APB3, AXI4, AXI4-Lite, Wishbone, CFU) or over sockets, and DPI over TCP covers Verilator, Questa and VCS (AHB, APB3, AXI4, AXI4-Lite). SystemC TLM models connect over TCP sockets (since July 2024). Virtual time is synchronized in quanta. Microchip already works with Antmicro: PolarFire SoC and Mi-V, SoftConsole integration, and Cortex-M MCUs that 'can also be simulated along with their peripheral interfaces'. sam_e70 is a supported board.
   detail: I did not verify whether Renode's CPU translation library (tlib) is derived from QEMU, or whether Renode supports SAM D21 or PIC32CM.
   - ev: https://github.com/renode/renode
   - ev: https://renode.readthedocs.io/en/latest/
   - ev: https://renode.readthedocs.io/en/latest/basic/using-python.html
   - ev: https://renode.readthedocs.io/en/latest/advanced/co-simulating-with-an-hdl-simulator.html
   - ev: https://renode.readthedocs.io/en/latest/tutorials/co-simulating-custom-hdl.html
   - ev: https://antmicro.com/blog/2024/07/systemc-co-simulation-in-renode/
   - ev: https://renode.readthedocs.io/en/latest/advanced/time_framework.html
   - ev: https://antmicro.com/blog/2022/12/microchip-riscv-support-in-renode
   - ev: https://antmicro.com/blog/2021/06/advanced-co-simulation-with-renode-and-verilator
   - ev: https://renode.readthedocs.io/en/latest/introduction/supported-boards.html

28. (medium) [alt-unicorn] Unicorn 2 is a 'CPU emulator framework' whose ChangeLog says it is 'Based on Qemu 5.0.1'; the latest release is 2.1.4 (Sep 2025). It is GPLv2, runs natively on Windows, and has Python and many other bindings. MMIO is handled by uc_mmio_map read/write callbacks on 4 KB-aligned regions whose size is a multiple of 4 KB. No SoC, device or interrupt-controller infrastructure is documented. It fits library-embedded unit tests of drivers with mocked registers, and fuzzing (Fuzzware serves MMIO reads from fuzzer input), not a full peripheral twin.
   - ev: https://github.com/unicorn-engine/unicorn
   - ev: https://raw.githubusercontent.com/unicorn-engine/unicorn/master/include/unicorn/unicorn.h
   - ev: https://raw.githubusercontent.com/unicorn-engine/unicorn/master/ChangeLog
   - ev: https://github.com/fuzzware-fuzzer/fuzzware-emulator

29. (high) [cpu-model-caveat] Upstream has a cortex-m0 CPU model but no cortex-m0plus model, which affects any QEMU-based PIC32CM JH (Cortex-M0+) twin regardless of how peripherals are integrated.
   detail: This belongs to the CPU research area. M0+ specifics such as VTOR, the optional MPU and the single-cycle I/O port were not assessed here.
   - ev: target/arm/tcg/cpu-v7m.c:45
   - ev: target/arm/tcg/cpu-v7m.c:326
   - ev: hw/arm/nrf51_soc.c:190

## Table: Integration options
| option | mechanism | perf | determinism | effort | upstreamable | fidelity ceiling | best for |
| 1. Native C QOM sysbus device | MemoryRegionOps + qemu_set_irq; SoC code maps the region and wires IRQs to armv7m NVIC GPIO inputs; chardev, can-bus, I2C/SSI and QOM properties for I/O | In-process call through the softmmu slow path (cputlb.c:1949); no IPC; fastest option (no measured ns figure) | Deterministic with -icount shift=N,sleep=off; record/replay available | About 300-700 LOC per typical peripheral, about 1.5k for clock/RCC-class blocks, plus 400-600 LOC of qtest (measured in-tree) | Yes, if human-written (no AI-generated code), qtested and with a committed maintainer; GPL | Register- and function-accurate; timing approximate, not cycle-accurate | Boot-critical and common peripherals; the customer-facing twin |
| 2. Native Rust QOM sysbus device | Same as C, through safe-Rust bindings (hwcore::sysbus and system::memory are stable) | Same as C | Same as C | Similar or more LOC (pl011: 1181 Rust vs 750 C); rustc 1.83+; needs --enable-rust | Possible in principle; Rust is off by default | Same as C | New models where memory safety matters, given a team with Rust skills |
| 3. Custom MMIO bridge, blocking per access | Custom sysbus device forwards each read/write over socket, pipe or shared memory and drops the BQL while waiting; IRQ messages come back to qemu_set_irq | One IPC round trip per access: about 7.7 µs over a Unix socket, about 14 µs over TCP, about 0.2 µs with spinning shared memory (Linux benchmark); caps polling near 10^5 accesses/s | Deterministic only with per-message virtual timestamps, a next-deadline from the model, and IRQs applied on virtual-time timers | Medium: about 0.5-1k LOC on the QEMU side (cf. upstream proxy at 885 LOC) plus protocol and external SDK | Unlikely as a generic upstream device (no precedent; vfio-platform removed); keep in a fork or out of tree | Whatever the external model provides, up to RTL | Plant and board models; Python, MATLAB or C++ models; confidential models; the hook for RTL |
| 4. Bridge with time-quantum sync (remote-port style) | Option 3 plus periodic sync packets every Q ns, posted writes and shadow reads | Amortized cost; throughput set by the quantum; reads with side effects stay synchronous | Deterministic if quantum boundaries are based on virtual time (icount) and the peer is deterministic | Medium-high | Not upstream; remote-port exists only in the Xilinx fork | Up to RTL, with timing error bounded by the quantum | SystemC/TLM and multi-simulator setups |
| 5. Upstream multi-process (x-pci-proxy-dev) or vfio-user-pci | PCI proxy or the vfio-user protocol over a UNIX socket | n/a for an MCU | n/a | n/a | Upstream, but gated on PCI/KVM/Linux | n/a | Not applicable to Cortex-M or Windows; useful only as reference code |
| 6. ivshmem-flat | Shared RAM plus doorbell IRQs via ivshmem-server; wired in machine code | Memory-speed data | Not deterministic (asynchronous peers) | Low | Upstream, Linux hosts only | No register semantics | Mailbox or shared-buffer links to external peers on Linux |
| 7. SystemC/TLM via remote-port + libsystemctlm-soc | Xilinx QEMU fork with remote-port devices, -machine-path, -sync-quantum and -icount; SystemC models; tlm2apb and other bridges | IPC plus SystemC kernel overhead; quantum-bound | Timestamped packets plus sync quanta plus icount | High: port remote-port to a Cortex-M machine and maintain a fork | No (fork only) | Up to RTL through Verilator | IP teams with existing SystemC/TLM models |
| 8. QBox / libqemu (QEMU inside SystemC) | QEMU as a TLM-2.0 initiator inside the SystemC process | In-process TLM calls; quantum keeper or MCIPS plugin sync | Quantum keeper; MCIPS works with TCG only | High; libqemu must be realigned on every QEMU update | No (libqemu is not upstream) | Up to RTL | SystemC-centric virtual platforms on Linux or macOS |
| 9. Verilator RTL co-sim of Microchip IP (behind 3, 4, 7 or 8, or in Renode) | Verilate peripheral RTL and wrap it with an APB/TLM bridge; analog parts modeled behaviorally | Full-SoC Verilator about 0.1 MIPS; a synchronized QEMU + RTL run took about 490x the unsynchronized time in SimBricks | Deterministic if lock-step synchronized | High: RTL cleanup, vendor or encrypted IP gaps, analog modeling | No, and RTL confidentiality applies | Cycle-accurate peripheral; the CPU is still approximate | Validating functional models and IP regressions; internal use |
| 10. TCG plugins | Shared library loaded into QEMU; translation, exec, memory and discontinuity callbacks plus bus read/write helpers | Per-access callback cost (not measured) | Observe-only for guest MMIO; cannot intercept accesses | Low | Yes for generic tools (contrib/plugins); API churn (minimum version 7) | n/a (observer) | MMIO tracing, register coverage, IRQ logs, lockstep diffs, throttling |
| 11. qtest | Line protocol over a chardev with -accel qtest; vCPUs never run | Per-command IPC; no CPU | Fully deterministic; the client owns the virtual clock | Low | Yes; tests are expected with new devices | Register level only | Model unit tests, conformance against IP-verification register sequences, fuzzing |
| 12. QMP/QOM plus chardev, can-bus, I2C/SSI, netdev | qom-set/qom-get on device properties; UART chardev; can-bus object; I2C/SSI slave devices | Out-of-band control path (not measured) | QMP writes land at points not tied to guest execution unless paused (reasoning) | Low per property | Yes | Board-level functional | Test-bench stimulus and observation; attaching UART, CAN and network |
| 13. Renode (alternative) | C# and Python peripherals; Verilator library or DPI; SystemC over TCP | Not measured here | Virtual-time framework with quanta | Medium: new platform .repl and C# models | n/a (MIT, Antmicro-led) | Up to RTL via co-sim | Multi-node systems, permissive license, Robot Framework CI; existing Microchip relationship |
| 14. Unicorn (alternative) | CPU library derived from QEMU 5.0.1; uc_mmio_map callbacks | In-process callbacks | Up to the test harness | Low per test | n/a (GPLv2) | CPU plus mocked registers; no SoC | Driver unit tests and fuzzing harnesses |

## Table: QEMU plugin API capabilities
| capability | API | can it change guest-visible behavior? | file:line |
| API version and compatibility | QEMU_PLUGIN_VERSION 7; QEMU_PLUGIN_MIN_VERSION 7 | n/a (older plugins are refused) | include/plugins/qemu-plugin.h:91; plugins/plugin.h:19; plugins/loader.c:214-217 |
| Instrument translated blocks and instructions | qemu_plugin_register_vcpu_tb_trans_cb, _insn_exec_cb, _insn_exec_cond_cb, _insn_exec_inline_per_vcpu | No (observe) | include/plugins/qemu-plugin.h:438, :520, :543, :562 |
| Observe every load/store issued by an instruction | qemu_plugin_register_vcpu_mem_cb (called after a successful access; faults invisible) | No; cannot alter loaded values or intercept MMIO | include/plugins/qemu-plugin.h:764-781; docs/devel/tcg-plugins.rst:117-123 |
| Value of the access | qemu_plugin_mem_get_value, _mem_size_shift, _mem_is_store | No | include/plugins/qemu-plugin.h:690, :657, :681 |
| Identify MMIO and the target device or register | qemu_plugin_get_hwaddr, _hwaddr_is_io, _hwaddr_phys_addr, _hwaddr_device_name (MemoryRegion name) | No | include/plugins/qemu-plugin.h:706, :723, :735, :742; plugins/api-system.c:70-97 |
| Low-overhead counters | qemu_plugin_register_vcpu_mem_inline_per_vcpu, qemu_plugin_scoreboard_new, qemu_plugin_u64_add | No | include/plugins/qemu-plugin.h:795, :1304, :1338 |
| Observe interrupt and exception entry | qemu_plugin_register_vcpu_discon_cb (INTERRUPT, EXCEPTION, HOSTCALL; from_pc and to_pc) | No | include/plugins/qemu-plugin.h:172-199, :302 |
| Observe WFI idle and resume | qemu_plugin_register_vcpu_idle_cb, _resume_cb | No | include/plugins/qemu-plugin.h:272, :285 |
| Read memory or MMIO by physical address | qemu_plugin_read_memory_hwaddr (address_space_rw read) | Yes, indirectly: the device read handler runs, so clear-on-read registers change | include/plugins/qemu-plugin.h:1245; plugins/api.c:552-590 |
| Write memory or MMIO by physical address | qemu_plugin_write_memory_hwaddr (address_space_rw write) | Yes: the device write handler runs, so it can poke peripheral registers | include/plugins/qemu-plugin.h:1279; plugins/api.c:592-627 |
| Read or write memory by virtual address | qemu_plugin_read_memory_vaddr, _write_memory_vaddr (cpu_memory_rw_debug) | Yes (write) | include/plugins/qemu-plugin.h:1178, :1199; plugins/api.c:514-550 |
| Read or write CPU registers | qemu_plugin_get_registers, _read_register, _write_register (requires an RW_REGS callback context) | Yes (write) | include/plugins/qemu-plugin.h:1100, :1120, :1144, :325-339 |
| Redirect execution | qemu_plugin_set_pc (noreturn; RW_REGS_PC context) | Yes | include/plugins/qemu-plugin.h:1157; plugins/api.c:505-512 |
| Control virtual time | qemu_plugin_request_time_control, qemu_plugin_update_ns (one plugin only; adds a migration blocker) | Yes, affects timer progression; under TCG it can only run timers up to the target (semantics need testing) | include/plugins/qemu-plugin.h:812, :827; plugins/api-system.c:102-131 |
| Implement a device, supply MMIO read data, or raise an IRQ line | none | Not possible through the API; an indirect write to interrupt-controller registers via write_memory_hwaddr is conceivable but untested | n/a |
| Disassembly and symbols for logs | qemu_plugin_insn_disas, qemu_plugin_insn_symbol | No | include/plugins/qemu-plugin.h:947, :957 |

## Table: Host-platform constraints (Windows vs Linux)
| feature | Windows host | Linux host | evidence |
| Build and run QEMU | Supported (MinGW/MSYS2) | Supported | docs/about/build-platforms.rst:148-166 |
| TCG plugins | Supported (delay-load import library) | Supported | plugins/meson.build:28-75; plugins/win32_linker.c |
| AF_UNIX and TCP sockets for a bridge | Yes (afunix.h or fallback definitions) | Yes | include/system/os-win32.h:33-47; meson.build:2616-2618 |
| fd passing for shared memory and eventfd fast paths | No (inferred) | Yes | docs/interop/vfio-user.rst:28-35 |
| vfio-user client and server, multi-process | No | Client needs Linux + PCI; server Linux-only; multi-process needs PCI + KVM | hw/vfio/Kconfig:7-12; meson.build:214-215; hw/remote/Kconfig:1-4 |
| ivshmem-flat | No | Yes | hw/misc/Kconfig:84-87 |
| CAN host connection | None; needs a new can-host backend | SocketCAN | net/can/meson.build:1-7; docs/system/devices/can.rst:6-7 |
| chardev pty | No | Yes | chardev/meson.build:16-28 |
| chardev named pipe and serial port | Windows named pipes; COM ports | FIFO; tty | chardev/char-pipe.c:32-64; chardev/char-serial.c:32-47 |
| Rust devices | Unverified | Yes with --enable-rust | meson_options.txt:388-389 |
| QBox (SystemC) | Not listed | Ubuntu 20.04-24.04 (macOS also) | https://github.com/quic/qbox |
| Renode | Installer and portable package | Packages | https://github.com/renode/renode |
| Unicorn | Native | Native | https://github.com/unicorn-engine/unicorn |

## Table: Recommended layered architecture for a silicon-vendor peripheral twin
| layer | contents | QEMU mechanism | why / evidence |
| L0: SoC core (native, upstream candidate) | Cortex-M0+ CPU (model gap), NVIC/SysTick, memory map, NVMCTRL, PM/RSTC/SUPC, OSCCTRL/GCLK/MCLK, PORT, EIC, EVSYS, DMAC, SERCOM (USART/SPI/I2C), TC/TCC, RTC, WDT, ADC digital part with QOM analog inputs, CAN controller; an unimplemented-device catch-all for every ATDF instance not yet modeled | C (or Rust) QOM sysbus devices; RegisterAccessInfo skeletons generated from ATDF with behavior written by people; Clock API, ptimer, named qdev GPIOs for EVSYS and DMA triggers; VMState | Fastest and deterministic under icount; the only first-class path; upstream precedent from Microchip-family machines (hw/char/stm32l4x5_usart.c:519-555; include/hw/core/register.h:45-60; MAINTAINERS:1240-1263,1795-1810) |
| L1: Board and plant I/O (QEMU-native backends) | UART to socket, named pipe or COM; CAN to can-bus (plus a Windows can-host backend); I2C/SPI slave models; analog and pin values as QOM properties; a Python QMP harness | chardev, can-bus, I2CSlave/SSIPeripheral, qom-set/qom-get | No IPC per register access; reuses existing infrastructure (net/can/meson.build:1-7; hw/adc/npcm7xx_adc.c:280-289; hw/gpio/aspeed_gpio.c:1476-1494) |
| L2: External model bridge (fork or out of tree) | One generic sysbus bridge, selectable per peripheral instance or as a board-level plant port, for confidential, plant or Simulink-style models | Blocking RPC with BQL dropped, or quantum sync; timestamped packets with next-deadline; IRQs on virtual-time timers; remote-port-compatible framing; migration blocker or external save/restore | No upstream equivalent; keeps the GPL boundary at a narrow socket protocol (hw/remote/proxy.c:246-294; qemu-options.hx:5352-5357; FSF GPL FAQ; remote-port-proto.h) |
| L3: RTL co-sim (internal validation) | Verilated Microchip IP (for example SERCOM or CAN) behind the L2 protocol or SystemC tlm2apb | libsystemctlm-soc bridges or a thin C++ shim; differential runs against L0 using the lockstep plugin and shared qtest sequences | Highest fidelity but hours per scenario when synchronized (SimBricks Table 1; GVSoC Table II); RTL stays internal |
| Cross-cutting: observability and test | Register-coverage and MMIO-profile plugins, IRQ-latency logs, trace events, per-model qtest conformance suites in CI, functional tests with Harmony/MCC firmware, record/replay for bug reproduction | TCG plugins, qtest/libqtest, functional tests, -icount record/replay | docs/about/emulation.rst:613-648; system/qtest.c:82-114; docs/system/replay.rst:86-93 |

## Table: Licensing posture by option (needs Microchip legal review; not legal advice)
| option | where the code runs | licenses involved | note |
| Native C or Rust models | Linked into the QEMU binary | GPLv2 (QEMU as a whole); Rust workspace GPL-2.0-or-later | Any distributed QEMU containing them is GPL (LICENSE:1-15; Cargo.toml:12) |
| TCG plugins | Dynamically loaded into the QEMU process | Plugin header GPL-2.0-or-later | FSF FAQ: dynamically linked plug-ins that share data structures form a single combined program |
| Socket bridge plus external model | Separate process | QEMU-side bridge GPL; external model under any license | FSF FAQ: sockets are normally between separate programs unless the semantics are intimate; keep the protocol narrow (register R/W, IRQ, time) |
| Shared-memory bridge | Separate process with shared memory | Same as above | FSF FAQ: shared memory with complex data structures is pretty much equivalent to dynamic linking; a higher-risk design |
| remote-port + libsystemctlm-soc | Xilinx QEMU fork plus SystemC process | QEMU side GPL; library and protocol header MIT | Protocol reuse is permissive; the QEMU device code is GPL |
| QBox | QEMU linked inside the SystemC process | Wrapper BSD-3; libqemu GPL | The combined binary inherits GPL questions |
| Verilated RTL | Inside the SystemC or bridge process | Verilator LGPL-3.0/Artistic-2.0; Microchip RTL proprietary | Distributing a Verilated model may expose RTL and third-party IP; needs IP review |
| Renode | Separate tool | MIT | Permissive |
| Unicorn | Linked into the test harness | GPLv2 | A harness that embeds Unicorn takes on GPL considerations |

## Code: Multi-process QEMU is gated on PCI + PCIe + KVM (hw/remote/Kconfig:1-8)
```kconfig
config MULTIPROCESS
    bool
    depends on PCI && PCI_EXPRESS && KVM
    select REMOTE_PCIHOST

config VFIO_USER_SERVER
    bool
    depends on MULTIPROCESS
```

## Code: The vfio-user client builds on VFIO_PCI (Linux + PCI only) (hw/vfio/Kconfig:7-12)
```kconfig
config VFIO_PCI
    bool
    default y
    select VFIO
    select EDID
    depends on LINUX && PCI
```

## Code: vfio-user-pci accepts only UNIX sockets (hw/vfio-user/pci.c:452-458)
```c
    if (udev->socket->type != SOCKET_ADDRESS_TYPE_UNIX) {
        error_setg(errp, "Unsupported socket type %s",
                   SocketAddressType_str(udev->socket->type));
        qapi_free_SocketAddress(udev->socket);
        udev->socket = NULL;
        return;
    }
```

## Code: Non-PCI vfio-platform removed in 10.2 (docs/about/removed-features.rst:1493-1500)
```rst
``-device vfio-platform`` (removed in 10.2)
'''''''''''''''''''''''''''''''''''''''''''
The vfio-platform device allows to assign a host platform device
to a guest in a generic manner. Integrating a new device into
the vfio-platform infrastructure requires some adaptation at
both kernel and qemu level. No such attempt has been done for years
and the conclusion is that vfio-platform has not got any traction.
PCIe passthrough shall be the mainline solution.
```

## Code: Reference pattern: synchronous MMIO forwarding over a socket (upstream PCI proxy) (hw/remote/proxy.c:246-274)
```c
static void send_bar_access_msg(PCIProxyDev *pdev, MemoryRegion *mr,
                                bool write, hwaddr addr, uint64_t *val,
                                unsigned size, bool memory)
{
    MPQemuMsg msg = { 0 };
    long ret = -EINVAL;
    Error *local_err = NULL;

    msg.size = sizeof(BarAccessMsg);
    msg.data.bar_access.addr = mr->addr + addr;
    msg.data.bar_access.size = size;
    msg.data.bar_access.memory = memory;

    if (write) {
        msg.cmd = MPQEMU_CMD_BAR_WRITE;
        msg.data.bar_access.val = *val;
    } else {
        msg.cmd = MPQEMU_CMD_BAR_READ;
    }

    ret = mpqemu_msg_send_and_await_reply(&msg, pdev, &local_err);
    if (local_err) {
        error_report_err(local_err);
    }

    if (!write) {
        *val = ret;
    }
}
```

## Code: Plugin API v5-v7 added calls that change system state (include/plugins/qemu-plugin.h:70-91)
```c
 * version 5:
 * - added qemu_plugin_write_memory_vaddr
 * - added qemu_plugin_read_memory_hwaddr
 * - added qemu_plugin_write_memory_hwaddr
 * - added qemu_plugin_write_register
 * - added qemu_plugin_translate_vaddr
 *
 * version 6:
 * - changed return value of qemu_plugin_{read,write}_register from int to bool
 * - added qemu_plugin_set_pc
 * - added disconinuity callback API (for interrupts, exceptions, host calls)
 * - added syscall filter callback API, which allows skipping syscalls and
 *   setting custom syscall return values
 *
 * version 7:
 * - add userdata to all plugin callbacks, allowing maintenance of state
 *   externally, and easing interfacing with other languages.
 */

extern QEMU_PLUGIN_EXPORT int qemu_plugin_version;

#define QEMU_PLUGIN_VERSION 7
```

## Code: Memory callbacks fire after the access, so a plugin cannot supply MMIO read data (docs/devel/tcg-plugins.rst:117-123)
```rst
Memory Accesses
+++++++++++++++

Memory callbacks are called after a successful load or store.
Unsuccessful operations (i.e. faults) will not be visible to memory
instrumentation although the execution side effects can be observed
(e.g. entering a exception handler).
```

## Code: User docs still call plugins passive (out of date relative to API v5+) (docs/about/emulation.rst:184-191)
```rst
QEMU TCG plugins provide a way for users to run experiments taking
advantage of the total system control emulation can have over a guest.
It provides a mechanism for plugins to subscribe to events during
translation and execution and optionally callback into the plugin
during these events. TCG plugins are unable to change the system state
only monitor it passively. However they can do this down to an
individual instruction granularity including potentially subscribing
to all load and store operations.
```

## Code: Plugin physical-address writes go through address_space_rw and reach device MMIO handlers (plugins/api.c:592-611)
```c
enum qemu_plugin_hwaddr_operation_result
qemu_plugin_write_memory_hwaddr(hwaddr addr, GByteArray *data)
{
#ifdef CONFIG_SOFTMMU
    if (data->len == 0) {
        return QEMU_PLUGIN_HWADDR_OPERATION_ERROR;
    }

    g_assert(current_cpu);

    int as_idx = cpu_asidx_from_attrs(current_cpu, MEMTXATTRS_UNSPECIFIED);
    AddressSpace *as = cpu_get_address_space(current_cpu, as_idx);

    if (as == NULL) {
        return QEMU_PLUGIN_HWADDR_OPERATION_INVALID_ADDRESS_SPACE;
    }

    MemTxResult res = address_space_rw(as, addr,
                                       MEMTXATTRS_UNSPECIFIED, data->data,
                                       data->len, true);
```

## Code: MMIO tracing in a plugin (hwprofile) (contrib/plugins/hwprofile.c:197-207)
```c
static void vcpu_haddr(unsigned int cpu_index, qemu_plugin_meminfo_t meminfo,
                       uint64_t vaddr, void *udata)
{
    struct qemu_plugin_hwaddr *hwaddr = qemu_plugin_get_hwaddr(meminfo, vaddr);

    if (!hwaddr || !qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    } else {
        const char *name = qemu_plugin_hwaddr_device_name(hwaddr);
        uint64_t off = qemu_plugin_hwaddr_phys_addr(hwaddr);
        bool is_write = qemu_plugin_mem_is_store(meminfo);
```

## Code: Native sysbus peripheral skeleton (chardev, IRQ, MMIO region, clock) (hw/char/stm32l4x5_usart.c:540-555)
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

## Code: Declarative register metadata API (a target for ATDF-generated skeletons) (include/hw/core/register.h:45-60)
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

## Code: ATDF register metadata (mode aliasing, WSYNC, W1C, SYNCBUSY) (DFP 1.3.221 PIC32CM-JH01/atdf/PIC32CM5164JH01100.atdf:5836,5904,6137,6216 (non-contiguous lines))
```xml
        <mode name="USART_INT" qualifier="SERCOM.USART_CTRLA.MODE" mask="6" value="1" caption="USART INTERNAL CLOCK Mode"/>
        <register modes="USART_INT" name="CTRLA" offset="0x0" rw="RW" access="WSYNC" size="4" initval="0x00000000" caption="USART_INT Control A">
        <register modes="USART_INT" name="INTFLAG" offset="0x18" rw="RW" size="1" atomic-op="clear:INTFLAG" initval="0x00" caption="USART_INT Interrupt Flag Status and Clear">
        <register modes="USART_INT" name="SYNCBUSY" offset="0x1C" rw="R" size="4" initval="0x00000000" caption="USART_INT Synchronization Busy">
```

## Code: Analog inputs exposed as QOM properties (settable with QMP qom-set) (hw/adc/npcm7xx_adc.c:280-289)
```c
    for (i = 0; i < NPCM7XX_ADC_NUM_INPUTS; ++i) {
        g_autofree char *adciprop = g_strdup_printf("adci[%u]", i);

        object_class_property_add_uint32_ptr(klass, adciprop,
                offsetof(NPCM7xxADCState, adci[i]),
                OBJ_PROP_FLAG_READWRITE);
    }
    object_class_property_add_uint32_ptr(klass, "vref",
                offsetof(NPCM7xxADCState, vref),
                OBJ_PROP_FLAG_WRITE);
```

## Code: The qtest client owns the virtual clock (system/qtest.c:82-107)
```c
 * Clock management:
 * """""""""""""""""
 *
 * The qtest client is completely in charge of the QEMU_CLOCK_VIRTUAL.  qtest commands
 * let you adjust the value of the clock (monotonically).  All the commands
 * return the current value of the clock in nanoseconds.
 *
 * If the commands FAIL then time wasn't advanced which is likely
 * because the machine was in a paused state or no timer events exist
 * in the future. This will cause qtest to abort and the test will
 * need to check its assumptions.
 *
 * .. code-block:: none
 *
 *  > clock_step
 *  < OK VALUE
 *
 * Advance the clock to the next deadline.  Useful when waiting for
 * asynchronous events.
 *
 * .. code-block:: none
 *
 *  > clock_step NS
 *  < OK VALUE
 *
 * Advance the clock by NS nanoseconds.
```

## Code: icount sleep=off: virtual time only advances to known QEMU timer deadlines (qemu-options.hx:5352-5360)
```rst
    When the virtual cpu is sleeping, the virtual time will advance at
    default speed unless ``sleep=off`` is specified. With
    ``sleep=off``, the virtual time will jump to the next timer
    deadline instantly whenever the virtual cpu goes to sleep mode and
    will not advance if no timer is enabled. This behavior gives
    deterministic execution times from the guest point of view.
    The default if icount is enabled is ``sleep=on``.
    ``sleep=off`` cannot be used together with either ``shift=auto``
    or ``align=on``.
```

## Code: How icount handles MMIO (an exact instruction count at every I/O access) (docs/devel/tcg-icount.rst:51-63)
```rst
Dealing with MMIO
-----------------

While we can adjust the instruction budget for known events like timer
expiry we cannot do the same for MMIO. Every load/store we execute
might potentially trigger an I/O event, at which point we will need an
up to date and accurate reading of the icount number.

To deal with this case, when an I/O access is made we:

  - restore un-executed instructions to the icount budget
  - re-compile a single [1]_ instruction block for the current PC
  - exit the cpu loop and execute the re-compiled block
```

## Code: The SocketCAN host backend is Linux-only (net/can/meson.build:1-7)
```meson
can_ss = ss.source_set()
can_ss.add(files('can_core.c', 'can_host.c'))
if host_os == 'linux'
  can_ss.add(files('can_socketcan.c'))
endif

system_ss.add_all(when: 'CONFIG_CAN_BUS', if_true: can_ss)
```

## Code: QEMU policy on AI-generated contributions (affects any upstream plan) (docs/devel/code-provenance.rst:291-299)
```rst
TL;DR:

  **Current QEMU project policy is to DECLINE any contributions which are
  believed to include or derive from AI generated content. This includes
  ChatGPT, Claude, Copilot, Llama and similar tools.**

  **This policy does not apply to other uses of AI, such as researching APIs
  or algorithms, static analysis, or debugging, provided their output is not
  included in contributions.**
```

## Code: IRQ observer: qtest irq_intercept_in observes without breaking delivery (hw/core/irq.c:29-38)
```c
void qemu_set_irq(qemu_irq irq, int level)
{
    if (!irq)
        return;

    irq->handler(irq->opaque, irq->n, level);
    if (unlikely(irq->observer)) {
        irq->observer(irq->opaque, irq->n, level);
    }
}
```

## Open questions
- Would upstream QEMU accept a generic non-PCI (sysbus) MMIO co-simulation bridge? There is no precedent: vfio-platform was removed in favor of PCIe, and multi-process is 'Odd Fixes'. This needs a qemu-devel RFC discussion before any investment that depends on it.
- What are the real per-access latencies of AF_UNIX, TCP and named-pipe round trips, and of a spinning shared-memory ring, on typical Windows 11 developer machines? The only figures found are an old Linux ping-pong benchmark.
- Can qemu_plugin_update_ns act as an external co-simulation time master under TCG+icount? In this tree only the qtest accelerator implements set_virtual_clock, so the semantics need an experiment.
- Upstream lacks a cortex-m0plus CPU model (only cortex-m0). How much M0+ behavior (VTOR, MPU, single-cycle I/O port used by PORT) must be added before peripheral work pays off? This belongs to the CPU research area.
- Can the PIC32CM JH peripheral RTL (SERCOM, TC/TCC, CAN, ADC digital) be Verilated without encrypted or third-party IP blockers, how fast does a single Verilated peripheral run, and may Verilated models leave Microchip?
- Legal: where is the GPL boundary for TCG plugins and for the chosen bridge protocol; what are the obligations when shipping QEMU binaries with MPLAB X; and how will Microchip comply with QEMU's no-AI-generated-content policy for anything intended for upstream?
- Is AMD/Xilinx remote-port portable to a Cortex-M machine in an upstream-based tree, and would AMD collaborate on upstreaming a remote-port-style bridge? The fork currently tracks QEMU 10.2.4.
- Does a current QEMU (11.x) with icount plus a timestamped bridge actually reproduce runs bit-exactly? SimBricks found QEMU 5.1.92 not fully deterministic even with icount.
- Strategic choice: QEMU versus Renode, given Microchip's existing Antmicro/Renode relationship (PolarFire SoC, SoftConsole) and Renode's MIT license and built-in SystemC and Verilator co-simulation.
- How do MPLAB SIM stimulus concepts (register injection, SCL, pin stimulus) map onto QMP qom-set, qtest set_irq_in, or a custom pin-bridge device? There is no generic external GPIO channel in QEMU.
