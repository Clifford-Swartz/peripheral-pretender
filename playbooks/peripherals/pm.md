# PM: sleep modes and standby

Model: PM.SLEEPCFG in `hw/misc/mchp_sysctrl.c` (stores only); standby itself in
`hw/arm/pic32cm_soc.c` (`pic32cm_sleep()`), `hw/misc/mchp_clock.c` (GCLK channel gating, MCLK CPU
clock), the CPU's sleep hook in `target/arm` and SysTick in `hw/timer/armv7m_systick.c`. Research:
`docs/research/peripherals/pm-u2240.md`. Checked app: `sleep`; qtest `pic32cm-pm-test`.

## How it works

- WFI with SLEEPCFG = STANDBY: each GCLK peripheral channel runs only if one of its peripherals
  is enabled with RUNSTDBY (located per instance by the generator: `MchpInstanceInfo.standby_*`
  from the ATDF's CTRLA/CTRL RUNSTDBY and ENABLE). Channels with no such instance keep running.
  The CPU clock stops, and SysTick with it. Waking up (any interrupt that still fires) restores
  everything.
- A model takes standby through its GCLK clock: at 0 Hz it must stop, keeping its state, and
  resume when the clock returns. TC and TCC do (`gclk_update` folds the ticks since the last event
  in on ClockPreUpdate). A new model with timing must do the same; one that ignores its clock keeps
  running in standby.
- Set the SoC's "standby" QOM property to test standby without firmware.

## Silicon lessons (PIC32CM MC00)

- Standby gates channels, not peripherals: a TC without RUNSTDBY counts if another peripheral on
  its channel asks for it (TC0/TC1 share a channel, so do TC2/TC3).
- Clock sources and generators run on demand in standby whatever their RUNSTDBY (seen with the
  debugger attached).
- Flush the console and arm the wake-up timer right before the WFI. The first standby after a
  debugger reset may run SysTick for a while: put a warm-up sleep first in a check.
- An RTC COUNT read (COUNTSYNC) after waking up takes several milliseconds on silicon.

## Not modelled

SLEEPONEXIT, STDBYCFG (regulator, SRAM back-biasing), conversions (TSENS, ADC) paused by standby,
SERCOM in standby, the DMAC and EVSYS standby bits, the slow RTC read after a wake-up.
