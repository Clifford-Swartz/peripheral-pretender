# SERCOM: USART, SPI host, I2C host

Model: `hw/char/mchp_sercom.c` (pp-qemu). Each SERCOM has a QEMU SSI bus
(`/machine/soc/sercomN/sercomN.spi`) and I2C bus (`sercomN.i2c`) for attaching parts.

## Silicon lessons (PIC32CM MC00, U2201)

I2C host:
- STATUS.BUSSTATE is UNKNOWN after enable. Firmware forces IDLE by writing 1. During a transfer
  it is OWNER, and it stays OWNER for about 10 µs after a stop before going IDLE (modelled with
  a stop timer).
- An address with no device answering sets **MB** (also for reads, not SB), with RXNACK and
  CLKHOLD.
- Smart mode (CTRLB.SMEN): reading DATA acknowledges and fetches the next byte; with
  CTRLB.ACKACT set, reading DATA NACKs instead. **ACKACT stays set** after a stop command. Clear
  it before the next read, or the first byte is NACKed. This caught our own qtest.
- The EDBG data gateway on Curiosity Pro boards answers at 0x28: it ACKs, reads return 0, and it
  ignores the general call. QEMU's I2C core ACKs a broadcast from any device, so parts that should
  not answer it override `match_and_add`.

SPI host:
- With MISO on the MOSI pad the data loops back. An unconnected MISO reads 0xFF.
- A lost character sets STATUS.BUFOVF/INTFLAG.ERROR and leaves a **0 placeholder** in the
  receive buffer.

## Not modelled

SPI/I2C client modes, bus timing, arbitration, timeouts, 10-bit and high-speed I2C, the USART
external clock, synchronous mode, LIN, IrDA, 9-bit characters, receive errors.
