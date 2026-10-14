# Oracles: what to believe

A model needs a reference to be checked against. Different sources are good at different
things, and they disagree more often than you would expect.

| Source | Good for | Known problems |
|---|---|---|
| **Silicon** (the kit on the desk) | Final answer on behaviour, timing order, flag semantics, reset values | Needs the kit; some things hard to observe (internal clocks) |
| **Datasheet** (Microchip docs MCP, `search_microchip_product_documents`) | Intended behaviour, register meaning, timing | Ambiguities and errors; errata documents correct some |
| **ATDF / DFP** (`~/.mchp_packs`) | Register layout, addresses, IRQs, signals, events, pin mux | Known errors, listed in `docs/research/microchip-data.md` |
| **MPLAB simulator** (MDB, `Hwtool SIM`) | Mature 8-bit PIC and AVR families | For PIC32CM (JH01, MC00) it is **beta** and stops in clock init (see below) |
| **Harmony / MCC examples** | Real code paths a customer runs | They test the model only as far as the example exercises it |

Precedence: **silicon > datasheet > ATDF**. The MPLAB simulator ranks below the datasheet: use it
where the kit is not available, and only for families where it is mature.

## Rules

1. When a source and the model disagree, write a probe that shows the behaviour, run it on
   silicon, and record the result in the model's source comment and the peripheral playbook.
   `firmware/hello/tests/*probe.c` are examples: they print observations and check nothing.
2. Then write a checked test (`firmware/hello/tests/<ip>.c`) that asserts the silicon behaviour.
   Run it on silicon, save the transcript as a golden
   (`golden/<device>-<rev>/<test>-vcom.bin`), and add it to `tests/golden/`.
3. Never "fix" a test expectation to match the model without silicon or a datasheet quote.
4. When the datasheet and silicon differ, say so in the model's comment ("silicon: ...;
   DS 30.6.2 says ..."), so the next reader does not "correct" it back.

## MPLAB simulator on PIC32CM (checked 2026-10-14, MPLAB X v6.20)

- Device Support lists Sim = Y for PIC32CM1216MC00048 and PIC32CM5164JH01100, but MDB reports
  PORT, EIC and TC as "partial support ... beta mode", "Peripheral Missing: ADCController" and
  "We don't simulate the clock selection".
- It does not implement the calibration area at 0x806020 (`W0030-CORE ... unimplemented memory
  access`).
- Microchip's own `port_led_on_off_polling` example (Harmony, XC32) never leaves
  `OSCCTRL_Initialize` because the oscillator ready flag never sets. Our `hello` stops in the same
  place.
- It loads Intel hex but rejects GCC-built ELFs with a bare "Program failed". Convert with
  `arm-none-eabi-objcopy -O ihex`.

So for PIC32CM, silicon is the only behavioural reference. The simulator becomes useful for
8-bit PIC and AVR boards, where it is not beta.
