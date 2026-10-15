# Golden data from silicon

Captured from real boards, kept byte-exact (`.gitattributes`: no line-ending
conversion), and checked against the twin by `tests/golden/` without hardware.

## `pic32cm1216mc00048-a1/`

PIC32CM MC00 Curiosity Pro (EV15N46A), PIC32CM1216MC00048 rev A1, EDBG probe
ATML3396053000000017, 2026-10-13.

| file | content | how |
|---|---|---|
| `reset.json` | 811 register elements right after a reset | pyOCD halt connect, DFP reset-and-halt (SYSRESETREQ), each register read at its width; read-side-effect and write-only registers skipped |
| `hello-vcom.bin` | VCOM bytes of `firmware/hello` (board `pic32cm_mc00_cpro`) | `py -m harness.hw_run firmware/hello/build/pic32cm_mc00_cpro/hello.elf --allow-program --target pic32cm1216mc00048 --pack <MC DFP .pack> --out ...` |
| `adc-silicon.txt` | VCOM bytes of `APP=adc` (ADC0 internal inputs; 2026-10-15, the board's VCC_SEL at 5 V) | programmed with MPLAB IPE (`ipecmd -P32CM1216MC00048 -TS<serial> -F<hex> -M -OL`: pyOCD's flash algorithm faulted that day), then `hw_run --programmer none --allow-reset`. A record, not compared byte for byte (not named `-vcom.bin`): the codes follow the supply and the real ADC's small gain error; the twin (3.3 V) passes the same checks |
| `dac-silicon.txt` | VCOM bytes of `APP=dac` (DAC on PA02 read back by ADC0 AIN0; board at 5 V, 2026-10-15) | as `adc-silicon.txt`. Codes 0-1023 linear with an offset of about 12 ADC codes (15 mV at 5 V) at 0, which the twin does not model |
| `sdadc-silicon.txt` | VCOM bytes of `APP=sdadc` (SDADC AIN1 with A0/A1 left open; board at 5 V, 2026-10-15) | as `adc-silicon.txt`. Checks relative to an uncorrected reading of the open inputs: OFFSETCORR is added, then GAINCORR, then SHIFTCORR, saturating at 0x7FFFFF; REFCTRL and CTRLB ignore writes while enabled; RESULT's low byte is live (not 16 bits << 8) |

The twin reproduces `hello-vcom.bin` byte for byte when configured as this
part: `-global pic32cm1216mc00048-soc.silicon-rev=A1 -global
pic32cm1216mc00048-soc.serial-number=F79A74AF51524B5939202020FF190418`.
`tests/golden/test_mc00_silicon.py` lists the register differences that are
allowed, with the reason for each.

## `avr64ea48-b1/`

AVR64EA48 Curiosity Nano (EV66E56A), AVR64EA48 rev B1, nEDBG MCHP3428021800000314,
programmed by drag and drop (`py -m harness.nano_run <hex> --allow-program --out ...`).

| file | content | how |
|---|---|---|
| `*-vcom.bin` | VCOM bytes of `firmware/avr-hello` apps, 2026-10-14 | compared byte for byte by `tests/golden/test_avr64ea48_silicon.py` |
| `adc-silicon.txt` | VCOM bytes of `APP=adc` (VDD 3.31 V, 2026-10-15) | a record: the codes follow the supply; the twin passes the same checks (`test_adc_checks_pass`). Its SIGROW TEMPSENSE0/1 (0xFDCE, 0xEC7E) are in `profiles/avr64ea48.yaml` |
