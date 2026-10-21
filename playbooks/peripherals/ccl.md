# CCL: configurable custom logic

Model: `hw/misc/mchp_ccl.c` (U2225: PIC32CM JH/MC, SAM E51 register layout). Research:
`docs/research/peripherals/ccl-u2225.md` (sections 8 and 9: silicon answers, the twin). Checked
app: `ccl`; qtest `pic32cm-ccl-test`.

## How it plugs in

- IN pads come through the generic pad input path: PORT "func-in"[pin * 16 + f] to the model's
  "pad-in-in"[x] (`pic32cm_connect_pad_inputs()`; any model can take a pad signal this way by
  declaring "pad-<group>-in"). OUT pads through `pic32cm_connect_pads()` ("pad-out").
- TC WO[0], TCC WO[0..2] and AC CMPn reach the CCL without pads: `pic32cm_connect_pads()` adds the
  CCL as an extra sink of those signals ("tap-tc", "tap-tcc", "tap-ac") and ORs the CCL's
  "tap-*-active" into the source's "pad-wo-active", so a TC or TCC stops coasting over output
  changes while a LUT reads it.
- The per-device INSEL wiring comes from the SoC: "tcc-count" and "ac-count" (LUTn reads
  TCC(n % count), AC(n % count)).

## Silicon lessons (PIC32CM MC00, CCL U2225)

- Configure a LUT in the write that enables it, with the CCL off: once a LUT is enabled its other
  bits are locked until it is disabled, even with CTRL.ENABLE = 0, and a disabling write changes
  only ENABLE. With the CCL on, nothing but ENABLE takes.
- A disabled LUT or CCL drives its pad to 0.
- Read an output without a pad through its LUTOUT event: asynchronous EVSYS channel into PORT
  event OUT on a free pin, read through PORT.IN with INEN (PORT.OUT does not show it).
- Measure clocked behaviour with a slow GCLK_CCL (OSCULP32K / 32 = 1024 Hz) and SysTick polling;
  count fast outputs through an event channel into a TC.

## Not modelled

The SERCOM TX bit stream (the input reads 1), JH01 ALT2TC and ASYNCEVENT, standby, FILTER's exact
glitch rejection, the EVENT strobe's sub-cycle width (zero here).
