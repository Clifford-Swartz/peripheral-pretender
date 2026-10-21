# DAC: the PIC32CM digital-to-analog converter

Model: `hw/misc/mchp_dac_u2214.c` (PIC32CM JH/MC, U2214; the SAM D5x's U2502 is in
[samd5x.md](samd5x.md)). Research: `docs/research/peripherals/dac-u2214.md`. Checked apps: `dac`
(conversions, references), `dacbuf` (DATABUF, START, EMPTY, DMA, dithering).

## How it plugs in

- VOUT is PA02, also ADC0 AIN0: the ADC reads the DAC back with no wiring (`mchp_dac_u2214_vout_mv()`),
  which is how every DAC check measures the output. Compare as ratios (DAC and ADC both against
  VDDANA): the desk board's VDDANA is 5 V, the twin's 3.3 V.
- Events by the SoC's naming rule: user START is "start-event-in", generator EMPTY "empty-event".
- DMA: `DMAC_ID_EMPTY` (39 on the MC00, 45 on the JH01) is a level trigger on INTFLAG.EMPTY; the
  model kicks the DMAC whenever EMPTY sets.

## Silicon lessons (PIC32CM MC00, DAC U2214)

- Every enable sets EMPTY, so a DMA channel on EMPTY moves its first beat into DATABUF at the
  enable.
- After a DATABUF write, SYNCBUSY.DATA and DATABUF stay set until a START event moves it. A wait on
  SYNCBUSY after a DATABUF write never ends with STARTEI = 0, and writes meanwhile are dropped.
  Wait on SYNCBUSY with a bound in test firmware.
- EVCTRL is enable-protected (the register description says otherwise): configure events before
  the enable.
- DITHER: EMPTY every 16th START from the enable, UNDERRUN only at a group's first START, but a
  full DATABUF moves at any START.

## Not modelled

The dithered output's sub-conversion steps (the model outputs the average), the conversion time,
VPD, standby, the error of a write dropped during synchronization, silicon's ~15 mV offset at
code 0.
