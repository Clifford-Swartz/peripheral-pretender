# TC and TCC: timers and PWM

Models: `hw/timer/mchp_tc.c`, `hw/timer/mchp_tcc.c` (pp-qemu). Closed-form counters: the count
is computed from virtual time, and a timer is armed only for the next event (overflow, compare
match), never per tick.

- **Keep tick instants exact.** The base time a counter counts from must be the exact instant of
  the last tick (here t_base plus a 2^-32 ns fraction); round only the timer deadline up. Storing
  the deadline (instant + 1 ns) as the new base lost a tick every ~250 events at 4 MHz and every
  ~20 at 48 MHz. A long qtest at DIV1 with outputs on pads (many events) catches it; short tests
  at DIV1024 never do.

## Silicon lessons (PIC32CM MC00, TCC U2213)

- PWM duty on a waveform output is **CC / (PER + 1)**.
- A buffered write (CCBUF/PERBUF) keeps SYNCBUSY set until it is copied at the period
  boundary. A probe that waits on SYNCBUSY therefore blocks to the end of the period. This was
  first misread as "a buffer write restarts the counter". Use bounded waits in probes.
- STOP clears COUNT. The outputs go to their non-recoverable state (DRVCTRL.NREx/NRVx): NRVx
  where NREx is set, else the pad is released (pulled down if PULLEN, whatever PORT.OUT says)
  a few GCLK cycles after the command. STATUS.CMPx keeps its state, but reads 0 a few cycles
  after a STOP with DIV1.
- Dual-slope modes: the direction flag and the MCx/CMPx timing follow the slope. Several
  datasheet readings (CMP values at BOTTOM, NPWM counting down with CC > TOP never matching) were
  wrong until checked on silicon. Trust the golden transcripts in `golden/`.
- COUNTEV/INC events replace the clock as the count source; CNTSEL rules apply.
- Counter width comes from the ATDF (`.counter_bits`, TCC `SIZE`), channel count from `CC_NUM`
  (`.channels`). Both are generated into the instance info.
- **Catch-up bug to avoid:** when the host falls behind, do not delete the timer after a
  maximum number of steps. Reset the step count and continue, or PWM stops.

## TC outputs and capture (tcprobe2..4, APP=tcio)

- Watch outputs without wiring: `scope.h` records PORT.IN edges of pins muxed to the TC (INEN on).
  Stimulate captures with a pull resistor on an EIC pin (HIGH sense, ASYNCH) through an
  asynchronous EVSYS channel. A pin muxed to the TC itself loses its pull, so capture on pin
  (COPEN) needs a wire.
- Silicon starts the outputs about a tick after the enable; the twin at once. Check periods and
  widths from the first falling edge, not absolute times.
- A start with COUNT = CCx sets MCx (CC = 0: at once).
- STAMP captures MAX at every overflow: in a slow test (or across a print, which takes
  milliseconds on silicon and nothing on the twin) the FIFO fills and ERR sets.
- PW waits for the first active edge after the enable.

## TCC waveform extension (tccprobe2..5, APP=tccwave)

- Undriven pins keep their charge, so a probe that reads a released pin sees its last level for
  hundreds of microseconds: "the outputs are kept" and "the outputs are released" look the same.
  Choose levels opposite to what the pins float to, or drive a known state (NREx/NRVx) and watch
  it change. On the EV15N46A, PA18/PA19 (DGI SPI behind the level converter) rise when released.
- WAVE and PATT synchronize for ~4 GCLK cycles and silicon drops a second write meanwhile: wait
  for SYNCBUSY between writes, or the second write (SWAP, POL) silently does nothing.
- Measure from an edge of the same run, and take a "first fall" after a rise: a pin left high by
  the previous case falls at the start too.
- The units an instance has (OTMX, DTI, SWAP, PG, outputs) come from the DFP's TCC parameters
  (`MchpInstanceInfo.features`, `.outputs`); write all ones and read back to check (B0).
- 16 frames of dithering on eight outputs overflow a 128-edge recording: record the output under
  test alone.

## Not modelled

TCC faults, capture, circular buffers, RAMP2CS, the outputs' start latency; TC capture on pins
(COPEN), CAPTMODE (JH01).
