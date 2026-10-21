# PORT: pads, pin mux, pulls

Model: `hw/gpio/mchp_port.c` (pp-qemu). Header comment and `include/hw/gpio/mchp_port.h` list
the GPIO lines and QOM properties.

## How a pad's level is decided

In order: the PORT's own drive (DIR/OUT), then a peripheral that drives the pin through the
mux, then an external driver ("port-in" from the board), then the pull (PINCFG.PULLEN, towards
OUT), else 0 (floating reads 0).

## Lines for boards and peripherals

- `port-in[n]` (board → pad): 0, 1, or -1 for "released" (open drain letting go).
- `pmux-in[pin*16+func]` (peripheral → pad): 0/1, -1 undriven, -2 = Hi-Z with the pull off
  (`MCHP_PORT_PAD_HIZ`).
- Outputs `pad[n]` (any driver's level or -1) and `pmux-active[...]` (which function owns a pin).
- `func-in[pin*16+func]` (pad → peripheral): the pad's level while the pin is muxed to that
  function, else 0. The SoC wires it to every model input named `pad-<group>-in`
  (`pic32cm_connect_pad_inputs()`, an OR gate when several pads carry a signal): the CCL's IN
  pads use it, and so can any model that reads a pin. (`func-a` is the older EIC-only form.)
- QOM `pX-stats`: driven-high/low nanoseconds per pad, for duty cycle. `pX-levels`: every pad's
  level including external drivers and pulls (for showing open-drain signals).

## Silicon lessons

- A TCC output whose TCC is disabled leaves the pin **floating with the pull off**, not driven
  low. Hence the Hi-Z state.
- Pulls apply under PMUXEN, except where a peripheral drives the pin or holds it Hi-Z.
- Record the mux selection **before** raising `pmux-active`. The peripheral may drive the pad
  inside that callback, and the old order recursed until the stack overflowed (0xC00000FD).
- Board pull-ups are part of the board model. An LED to VCC pulls its pad high (the boards
  drive `port-in` = 1 on the LED pin). A Click's pull-up on an open-drain INT (Thermo 8 R3) is
  modelled in the board's Click table (`int_pullup`). Leaving it out made ALERT read active
  whenever firmware did not enable the pad's own pull.

## Outside drivers: who wins

A pad's level, highest first: the PORT's own output, a peripheral it is muxed to, the web
view's parts bench (`pX-bench`), the `port-in` drivers (Clicks, board buttons, `pX-in`),
then the pull resistor. The bench reads which pulls are on through `pX-pull` (`"<mask> <up>"`),
so its solver includes them; keep both in step when the pad logic changes (both PORT models).
