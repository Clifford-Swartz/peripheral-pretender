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
| WDT (watchdog) | U2251 | [wdt.md](wdt.md) |
| NVMCTRL (flash programming) | 03929 | [nvmctrl.md](nvmctrl.md) |
| ADC (conversions, sequences, events, DMA) | U2247 | [adc.md](adc.md) |
| DAC (DATABUF, START, EMPTY, DMA, dithering) | U2214 | [dac.md](dac.md) |
| SAM D5x: DMAC, USB (with a host), QSPI, DAC, EVSYS, clocks, ADC | U2503, U2222, U2008, U2502, U2504, ... | [samd5x.md](samd5x.md) |

The procedure for a new model, or a gap in one, is [../model-peripheral.md](../model-peripheral.md):
research, probe the silicon, model, checked app and golden, qtest, hand-back. `py -m tools.periph`
does the mechanical steps.
