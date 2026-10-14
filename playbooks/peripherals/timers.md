# TC and TCC: timers and PWM

Models: `hw/timer/mchp_tc.c`, `hw/timer/mchp_tcc.c` (pp-qemu). Closed-form counters: the count
is computed from virtual time, and a timer is armed only for the next event (overflow, compare
match), never per tick.

## Silicon lessons (PIC32CM MC00, TCC U2213)

- PWM duty on a waveform output is **CC / (PER + 1)**.
- A buffered write (CCBUF/PERBUF) keeps SYNCBUSY set until it is copied at the period
  boundary. A probe that waits on SYNCBUSY therefore blocks to the end of the period. This was
  first misread as "a buffer write restarts the counter". Use bounded waits in probes.
- STOP clears COUNT; after STOP the WO pin shows the start-of-cycle level.
- Dual-slope modes: the direction flag and the MCx/CMPx timing follow the slope. Several
  datasheet readings (CMP values at BOTTOM, NPWM counting down with CC > TOP never matching) were
  wrong until checked on silicon. Trust the golden transcripts in `golden/`.
- COUNTEV/INC events replace the clock as the count source; CNTSEL rules apply.
- Counter width comes from the ATDF (`.counter_bits`, TCC `SIZE`), channel count from `CC_NUM`
  (`.channels`). Both are generated into the instance info.
- **Catch-up bug to avoid:** when the host falls behind, do not delete the timer after a
  maximum number of steps. Reset the step count and continue, or PWM stops.

## Not modelled

TCC faults, dead-time insertion, pattern generation, dithering, capture, circular buffers,
the DSCRITICAL waveform; TC capture.
