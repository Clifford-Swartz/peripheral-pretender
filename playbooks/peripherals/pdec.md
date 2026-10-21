# PDEC: position decoder

Model: `hw/timer/mchp_pdec.c` (U2263: PIC32CM MC00, JH01, SAM E51). Research:
`docs/research/peripherals/pdec-u2263.md` (sections 8 and 9: silicon answers, the twin). Checked
app: `pdec`; qtest `pic32cm-pdec-test`.

## How it plugs in

- The PDEC[n] pads come through the generic pad input path ("pad-qdi-in", see
  [port.md](port.md)); events EVU0-2 through EVSYS ("evu-event-in"). Outputs are the usual event
  lines (OVF, ERR, VLC, MC0/MC1 pulses; DIR as a level).
- Input changes that arrive in the same instant are decoded together: one EVSYS event fanned out
  to two users is a simultaneous change. Two separate qtest commands never are.

## Silicon lessons (PIC32CM MC00)

- Drive phases without wiring by pulling pads muxed to the PDEC (PA08/PA09 on function J); avoid
  PA18 (the DGI level converter pulls it up). For a simultaneous change, route one EIC event to
  two EVU users.
- Write CC0/CC1 after SWRST: a probe that set them first counted with TOP = 0.
- QDEC and HALL need START after every enable; COUNTER does not.
- MC0/MC1 flag a count arriving at CCx, not one staying there; RETRIGGER loads 0 in both
  directions.

## Not modelled

The index and the X4S/X2S index checks (silicon's behaviour is not understood yet), AUTOC, the
HALL window and delay counters, the input filter, COUNTER event actions, standby.
