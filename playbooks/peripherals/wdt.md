# WDT (U2251)

The watchdog of the PIC32CM JH/MC and SAM E5x parts, in `hw/misc/mchp_sysctrl.c` (`mchp-wdt`).
Research and silicon answers: [docs/research/peripherals/wdt-u2251.md](../../docs/research/peripherals/wdt-u2251.md).
Checked on the MC00 by `firmware/hello` APP=wdt (golden `wdt-silicon.txt`) and the qtest
`pic32cm-wdt-test`.

## What the model covers

- Normal and window mode, early warning, time-out, on CLK_WDT_OSC (1.024 kHz) with edges at
  whole multiples of 1/1024 s of virtual time.
- Synchronization delays: CTRLA changes 3–4 clocks, CLEAR 2–3, by the write's phase; SYNCBUSY
  meanwhile. Writes that change nothing do not synchronize.
- The count starts one clock before an enable completes and restarts when a CLEAR completes.
- Wrong key: reset when its CLEAR completes, one clock later if the WDT is disabled. A clear in
  the closed window resets the same way.
- Reserved PER/WINDOW/EWOFFSET values count as 16384 clocks.
- ALWAYSON survives system resets (the data sheet; never set on silicon here).

## SoC wiring

The named GPIO `system-reset` goes to RSTC `reset-cause` line 5 (RCAUSE.WDT), which requests a
system reset. Reset values of CTRLA, CONFIG, EWCTRL come from the user row (`reset_from` in the
overlay).

## Lessons

- **A wrong CLEAR key resets even when the WDT is disabled.** The data sheet only says "any other
  value resets"; firmware that writes CLEAR blindly at start-up resets on silicon.
- **Disabling with WEN = 0 leaves WEN set**: enable-protected bits cannot change in the write that
  clears ENABLE. The next write while disabled can clear it.
- **Every sync is about 3 ms.** Firmware that waits on SYNCBUSY after each CLEAR spends 3 ms
  there. A CLEAR written while one is busy does not extend it (the model drops it).
- **The desk part's OSCULP32K runs about 2 % slow** (T ≈ 997 µs). Check times within ±5 %.
- **Probing resets** needs state that survives them: `keep_area()` in `check.h`, the top 256 bytes
  of RAM.
