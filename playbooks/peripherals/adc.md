# ADC: the PIC32CM analog-to-digital converters

Models: `hw/adc/mchp_adc_u2247.c` (PIC32CM JH/MC, U2247), `hw/adc/mchp_sdadc_u2260.c` (MC00
SDADC). Research: `docs/research/peripherals/adc-u2247.md`.

## Silicon lessons (PIC32CM MC00, ADC U2247)

- Conversions take time: firmware that reads RESULT right after SWTRIG.START reads the old value.
  Wait for INTFLAG.RESRDY. No conversion starts within 10 us of the enable.
- Time conversions in CPU cycles with GCLK_ADC = CPU clock and compare differences between
  settings: the start's fixed cycles and the polling loop cancel out.
- A free-running ADC with SEQCTRL starts its sequence at the enable; SEQSTATUS shows SEQBUSY and
  the last input.
- ADC1 as a client still needs its own GCLK for its SWRST: without it, CTRLA stays at SWRST.
- The DMA trigger for ADC0 RESRDY is 36 (0x24), not dmac.h's 0x2A. Read the DMAC note's trigger
  table before probing a DMA path.
- Leave no result unread before enabling a DMA channel on RESRDY: silicon moves it at once.
- The desk board's VDDANA (5 V) differs from the twin's default (3.3 V): compare values as
  orders or ratios in checked apps.

## Not modelled

The analog clock limit (silicon reads 0 with CLK_ADC above 16 MHz), double buffering for a
conversion already sampling, sleep (RUNSTDBY, ONDEMAND).
