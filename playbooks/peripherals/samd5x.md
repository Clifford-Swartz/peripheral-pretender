# SAM D5x/E5x models: DMAC, USB, QSPI, DAC, EVSYS U2504

Learned on the ATSAME51J20A (Curiosity PyKit Explorer, 2026-10-14) with stock CircuitPython as
the firmware under test. No silicon probes: the kit on the desk runs someone's project and is
read-only (REPL only), so these lessons come from CircuitPython's source
(`ports/atmel-samd`, TinyUSB `dcd_samd`) and the datasheet DS60001507.

| Model (pp-qemu) | IP | Notes |
|---|---|---|
| `hw/dma/mchp_dmac.c` | DMAC U2503 | descriptors in SRAM, write-back, triggers |
| `hw/misc/mchp_usb.c` | USB U2222 | device controller plus a built-in host (CDC ACM to a chardev) |
| `hw/ssi/mchp_qspi.c`, `hw/block/sst26vf.c` | QSPI U2008 | serial memory mode, AHB window; SST26 flash |
| `hw/misc/mchp_dac.c` | DAC U2502 | START events; `audio` property |
| `hw/misc/mchp_evsys5x.c` | EVSYS U2504 | a different register layout from U2256 (`evsys.md`) |
| `hw/misc/mchp_same5x.c` | OSCCTRL U2401, OSC32KCTRL U2400, MCLK U2408, NVMCTRL U2409, TRNG U2242 | enough for clock init and flash READY |
| `hw/adc/mchp_adc.c` | ADC U2500 | instant conversions, temperature sensor |

## DMAC

- **Never run a transfer during the write that enables its channel.** Firmware (CircuitPython,
  Harmony) sets CHCTRLA.ENABLE and then clears CHINTFLAG: a transfer that completed in between
  loses TCMPL and the firmware waits forever. The model runs started transfers at the next read
  of a DMAC status register, or 20 us later.
- **The same race comes back through any asynchronous caller.** A pulsed trigger (a TC overflow)
  arrives from a timer callback, between two guest instructions. Serving only the channels that
  took the pulse keeps it out of a channel that was just enabled. Before that fix, CircuitPython
  hung after a few minutes of audio while displayio waited for an SPI channel's TCMPL.
- CircuitPython's `shared_dma_transfer_start` re-enables every enabled channel if ACTIVE.ABUSY
  stays 0 while a channel is busy ("silicon bug?" comment). The model's ACTIVE reads 0, so a
  transfer must not linger busy after its trigger.
- Triggers: a SERCOM's are flags (DRE/RXC at INTFLAG 0x18, polled); a TC's overflow is a pulse
  (`trigger`[id] input), and the TC only runs to every overflow while a channel waits for it
  (`trigger-wanted`[id]). One pending pulse per channel, as CHSTATUS.PEND.
- Block events (CHEVCTRL.EVOE, BTCTRL.EVOSEL BLOCK) pulse after the next descriptor is in the
  write-back memory: CircuitPython's double-buffered audio refills the buffer the write-back's
  DESCADDR names.

## USB with a built-in host

- CircuitPython has no console but USB. The model enumerates the device (descriptors,
  SET_ADDRESS, SET_CONFIGURATION), then opens the first CDC ACM interface (line coding, DTR) and
  bridges its bulk endpoints to a chardev, one transfer per 125 us poll.
- SYNCBUSY.SWRST must read 1 once after a reset (TinyUSB waits for it to go high, then low).
- Output before DTR is dropped by CircuitPython, as with a real terminal that opens the port
  late; the first `code.py` lines can be missing.

## DAC and audio

- CircuitPython's `audioio`: a TC in MFRQ mode overflows at the sample rate; EVSYS routes the
  overflow (asynchronous path) to DAC START0, and the same overflow triggers a DMAC burst into
  DATABUF0. `synthio` and WAV playback double-buffer and refill on DMAC block events through a
  synchronous EVSYS channel's EVD interrupt.
- DATABUF is write-only in the register table: keep the buffered value in the model.
- Timers fire late on Windows (the host timer runs at a coarse resolution), so a TC serves
  hundreds of overflows in one callback. Each output change is stamped with the time of the
  overflow being served (`mchp_event_time_ns()`), not the callback's, or the pitch drops. A TC
  that drops overflows when far behind (the default) loses samples; with a DMAC channel waiting
  it serves every one.
- The `audio` property is read destructively by one poller (the web page's session).

## QSPI and the CIRCUITPY drive

- CircuitPython reads and writes the flash through the AHB window with memcpy and serial memory
  mode commands; it does not use DMA for it. The SST26's block protection (BPR) is set at power-on
  until firmware sends ULBPR.
- The drive image is a raw 4 MiB file; `tools/pp/fatfs.py` formats it as CircuitPython does
  (FAT12, 8184 sectors of 512 bytes, 2 KiB clusters, label CIRCUITPY), so `os.statvfs` matches
  the kit.
