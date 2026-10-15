# Peripheral playbooks

One file per peripheral model. Each lists what the model covers, how it plugs into the SoC, and
the behaviour that silicon showed and the datasheet did not make obvious. Read the file before
changing or reusing a model. When you learn something new against silicon, add it here.

| Model | IP versions | Playbook |
|---|---|---|
| PORT (pads, pin mux, pulls) | U2210 | [port.md](port.md) |
| TC / TCC (timers, PWM) | U2249 / U2213 | [timers.md](timers.md) |
| SERCOM (USART, SPI host, I2C host) | U2201 | [sercom.md](sercom.md) |
| EVSYS (event system) | U2256 | [evsys.md](evsys.md) |
| Cortex-M0+ core and MPU | | [core.md](core.md) |
| SAM D5x: DMAC, USB (with a host), QSPI, DAC, EVSYS, clocks, ADC | U2503, U2222, U2008, U2502, U2504, ... | [samd5x.md](samd5x.md) |

General method for a new model, in this order:

1. Read the datasheet chapter (Microchip docs MCP) and the existing overlay for the IP.
2. Write a probe (`firmware/hello/tests/<ip>probe.c`) for every behaviour you are unsure of and
   run it on silicon: `py -m harness.hw_run <elf> --allow-program`.
3. Write the model in pp-qemu (`hw/<class>/mchp_<ip>.c`), event-driven on
   `QEMU_CLOCK_VIRTUAL`, with no per-tick polling.
4. Write a qtest, and a checked firmware test whose silicon transcript becomes a golden.
