# EVSYS: event system

Model: `hw/misc/mchp_evsys.c` (pp-qemu). GPIO inputs `generator[128]` and `user-busy[64]`,
outputs `user[64]`; per-channel clocks `channel-clock[n]`. Generators and users are wired by the
SoC from the device's event table (generated from the ATDF: `MchpDeviceInfo.events`).

## Silicon lessons (PIC32CM MC00, U2256)

- Synchronous and resynchronized paths need the channel's GCLK running. Without it, events do
  not pass. The asynchronous path does not need it.
- CHSTATUS (CHBUSY, USRRDY) and the OVR/EVD flags follow silicon. A user that is still busy when
  the next event arrives gives OVR.

## Not modelled

The FALLING edge setting together with pulse-generating sources.
