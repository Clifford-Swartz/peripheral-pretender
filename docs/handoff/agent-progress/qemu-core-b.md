# qemu-core-b progress log

## Status
- [x] read contract + research (boot-path finding 18, busy-wait table, overlay), part A code
- [ ] include/hw/char/mchp_sercom.h + hw/char/mchp_sercom.c
- [ ] include/hw/gpio/mchp_port.h + hw/gpio/mchp_port.c
- [ ] Kconfig/meson/trace-events (hw/char, hw/gpio); PIC32CM_JH_SOC selects MCHP_SERCOM/MCHP_PORT; board selects LED
- [ ] SoC model table (2 lines) + board LED0 (PC05 -> port-out[69]) / sw0-pressed (PB19 port-in[51])
- [ ] qtests pic32cm-jh-sercom-test.c + pic32cm-jh-port-test.c (+ tests/qtest/meson.build)
- [ ] docs/system/arm/pic32cm-jh01-cpro.rst update
- [ ] e2e: py -m tools.pp run firmware/hello/build/hello.elf -> 0; -trace led_set_intensity; MCP twin_run
- [ ] regression qtests (pic32cm-jh-test, microbit, stm32l4x5 x5, device-introspect, qom-test, test-hmp)

## Design decisions
- Both types subclass mchp-regblock (part A), state private in .c, type names in headers.
- SERCOM: TX = DATA buffer + shift register; instant by default (chardev write + G_IO_OUT watch on EAGAIN);
  "pace-tx" property (default off) holds the shifter one frame (BAUD + gclk) via QEMU_CLOCK_VIRTUAL timer.
  DRE = USART && ENABLE && TXEN && !buf_full (status bit maintained in storage); TXC after frame.
  RX: 2-char buffer; can_receive = 0 while receiver off (input waits, accept_input on enable / DATA pop).
  RX off (RXEN/ENABLE falling) flushes buffer, clears STATUS PERR/FERR/BUFOVF.
  SPI/I2C modes: store/read-back + LOG_UNIMP; buses "<inst>.i2c" (I2C) / "<inst>.spi" (SSI).
- PORT: tri-state pads (nrf51 convention: gpio level -1 = not driven). port-out[n] = OUT if DIR && !PMUXEN else -1.
  IN = INEN & pad; pad = own output > external driver > pull (OUT) > 0.
  QOM per group: pX-in (rw; write drives all 32 pads), pX-in-mask (rw), pX-out, pX-out-mask (ro).
- Board: LED0 = led_create_simple(active low, yellow, "LED0") on port-out[69]; sw0-pressed -> port-in[51] 0 / -1.

## Commands that work
- build: $env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/.../scratchpad/qemu-core-b/build.sh
