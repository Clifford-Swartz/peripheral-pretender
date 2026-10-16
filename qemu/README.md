# The pp-qemu fork

The twin's QEMU side lives in a separate checkout, `pp-qemu`, next to this repo: upstream QEMU
**v11.1.2** plus the commits in `patches/`, on branch `pp/main`. This repo carries the fork as
patches rather than a full QEMU history. These patches are AI-assisted and must never be submitted
upstream, because QEMU declines AI-generated contributions (`docs/devel/code-provenance.rst`).

| patch | content |
|---|---|
| 0001 | `scripts/symlink-install-tree.py`: skip symlink failures on Windows (no Developer Mode/admin needed) |
| 0002 | generated per-IP register tables (`include/hw/mchp/`) |
| 0003 | generated device description and NVM-rows image (`include/hw/arm/pic32cm5164jh01100_gen.h`, `hw/arm/pic32cm5164jh01100_nvmrows.h`) |
| 0004 | part A: register-file helper, generic regblock, clock and system models, SoC, `pic32cm-jh01-cpro` board, qtest, board docs |
| 0005 | part B: SERCOM USART and PORT models, LED0 and SW0 on the board, SERCOM and PORT qtests |
| 0006 | review fixes: clock migration, SERCOM loadvm, ICSR.RETTOBASE on ARMv6-M, unimplemented-write log cap, WDT CONFIG/EWCTRL protection |
| 0007 | generic `pic32cm-soc` built from generated device descriptors; PIC32CM1216MC00048 and the `pic32cm-mc00-cpro` board; tables regenerated for both devices (shared constants, per-device tables); MC00 qtest and board docs |
| 0008 | reset values checked against PIC32CM MC00 silicon (golden reset dump): DSU ROM table, MTB IDs, OSC48MRDY flag, PA30 SWCLK, EVSYS, MC00 calibration and bus-matrix values |
| 0009 | EIC model (edges, levels, NMI, clock-dependent synchronization as on silicon); PORT routes pads muxed to function A to the EIC; EIC qtest |
| 0010 | TC model (COUNT8/16/32, commands, one-shot, buffering, pairing), behaviour checked on silicon; regblock gclk_update hook; TC qtest |
| 0011 | RTC model for the MC00 (three modes, PER flags, calendar, alarm); OSC32KCTRL CLK_RTC_OSC output; power-on-only reset for RTC and OSC32KCTRL (as on silicon); RTC qtest |
| 0012 | Per-instance counter width for TCC registers (TCC0/1 24-bit, TCC2 16-bit; ATDF SIZE), matching silicon reset values |
| 0013 | EVSYS model (channels, users, software events, CHSTATUS/OVR/EVD as on silicon); generated event table (MchpDeviceInfo.events) wired by the SoC; event outputs of EIC (levels for level senses), TC and RTC; event inputs of TC (COUNT, RETRIGGER, START) and PORT (OUT/SET/CLR/TGL); EVSYS qtest |
| 0014 | `cortex-m0plus` CPU type (M0+ r0p1 CPUID, 8-region ARMv6-M MPU in the PMSAv7 format, CONTROL.nPRIV); ARMv6-M MPU_RASR/RBAR masks as on silicon; fix for disabled MPU subregions caching a whole TLB page; PIC32CM SoC uses it; board docs brought up to date |
| 0015 | TCC model (single/dual-slope waveform modes, flags, STATUS.CMPx, one-shot, commands, buffers with silicon SYNCBUSY behaviour, CNTSEL, event inputs/outputs), checked against MC00 silicon; TCC qtest; board docs |
| 0016 | Peripheral outputs on pads: PORT `pmux-in`/`pmux-active`/`pad` lines and per-pad duty accounting (`pX-stats`); TCC waveform outputs WO[x] (pin level checked on silicon); SoC wiring from the signal table; LED pull-up on the boards; TC/TCC keep scheduling after a catch-up jump |
| 0017 | SERCOM SPI host and I2C host (on the QEMU SSI and I2C buses), checked against MC00 silicon; EDBG data gateway I2C target (0x28) on the MC00 board; qtests |
| 0018 | Click boards on the MC00 board (`ext1=`/`ext2=` machine options); the Thermo 8 Click (`mchp-mcp9808` I2C temperature sensor with a live `temperature` property and an open-drain ALERT on INT); PORT `pX-levels`; qtests |
| 0019 | Thermo 8 Click: its 4.7 kOhm pull-up (R3) on INT, so an inactive ALERT reads high without the pad's pull-up |
| 0020 | Click table per mikroBUS signal (RST/PWM/INT/CS, I2C or SPI, the Clicks' pull resistors); Button G (`click-button`), 7seg (`74hc595` chain on SPI) and Relay Clicks; qtests |
| 0021 | `target/avr`: `avrxt` CPU type for the AVRxt core (AVR Dx/Ex, tinyAVR 0/1/2, megaAVR 0): I/O from data address 0 with no mapped register file, CPUINT interrupt levels (SREG.I kept on entry, RETI ends a level); loader accepts avr-gcc avrxmega ELFs |
| 0022 | generated AVR64EA48 tables (`include/hw/mchp/gen/avr64ea48/`, `include/hw/avr/avr64ea48_gen.h`, `hw/avr/avr64ea48_device.h`) and the AVR types in `mchp_device.h` |
| 0023 | generic `avr8x-soc` (flash, FLMAP window, SRAM, EEPROM, CCP, regblocks wired by vector); PORT/VPORT, USART, CLKCTRL, CPUINT, NVMCTRL and NVM-byte models; the `avr64ea48-cnano` board (LED0, SW0, VCOM on USART1); qtest |
| 0024 | regenerated AVR64EA48 tables: SPI and TCB overlays, CLKCTRL.MCLKSTATUS read-only by field |
| 0025 | AVR8X SPI host and TCB (periodic interrupt); MAX7219 (8x8 G Click); shared mikroBUS Click catalog `hw/misc/mikrobus.c` (the MC00 board uses it too); `mb1=`..`mb3=` Click slots of a Curiosity Nano Base on `avr64ea48-cnano`; qtests, board docs |
| 0026 | Buzz Click (`click-buzzer`: tone from the PWM pin's edges); each SPI Click gets its socket's CS index (two SPI Clicks on one board failed before); qtests |
| 0027 | SSD1351 OLED driver and the OLED C Click (`oledc`); `pp_panel`, the panel picture (`frame`, `frame-gen`) that headless front ends poll, shared by future display controllers; qtests |
| 0028 | MCP3204 SPI ADC (`mchp-mcp3204`, bit by bit) and the Thumbstick Click (`click-thumbstick`: joystick on CH0/CH1, switch on INT); qtests |
| 0029 | AVR8X TWI host (`avr8x-twi`; regenerated AVR64EA48 TWI tables), I2C on the Nano Base's mikroBUS sockets; ADXL345 and the Accel Click (`accel`); qtests |
| 0030 | MCP79411 RTCC with SRAM/EEPROM (`mchp-mcp79411`, two I2C addresses) and the RTC 6 Click (`rtc6`); qtests |
| 0031 | 8x8 R/Y/B and Button R/Y Clicks (colour variants of 8x8 G and Button G); qtests |
| 0032 | ATSAME51J20A (SAM E51, Cortex-M4F) through the generic SoC: instances with several NVIC lines, SAM D5x OSCCTRL/OSC32KCTRL/MCLK/NVMCTRL/TRNG, QSPI (serial memory mode, memory window), USB device controller with a built-in host (CDC ACM to a chardev), GCLK source numbering per family; SST26VF quad SPI flash; the `curiosity-circuitpython` board (PyKit Explorer); regenerated tables; qtest |
| 0033 | SAM D5x DMAC (descriptors, peripheral-paced triggers, write-back; transfers start at the next status read); ST7789V TFT driver on `pp_panel`; WS2812 chain decoder; the PyKit Ruler on `curiosity-circuitpython` (display, NeoPixels, I2C pull-ups); DMAC triggers in `MchpDeviceInfo`; qtests |
| 0034 | ICM-20948 (with AK09916) and BNO085 (SH-2/SHTP) I2C sensors; RNBD451 BLE module chardev (replies as from the kit); SAM D5x ADC (pins, supplies, temperature sensor); SERCOM I2C host keeps the next byte's SB when firmware clears SB right after a smart-mode read; machine option `bno085=on` |
| 0035 | SAM D5x DAC (START events, the `audio` property: the output as PCM for front ends) and EVSYS U2504 models; DMAC pulsed triggers (TC overflows; served without touching channels firmware has just enabled) and block events; TC serves every overflow while a DMAC channel waits for it; `mchp_event_time_ns()`, the time of a late event being served; the speaker on `curiosity-circuitpython`; regenerated DAC/EVSYS tables; qtest; board doc `docs/system/arm/curiosity-circuitpython.rst` |
| 0036 | PORT bench drive layer for the web view's parts bench: `pX-bench` (`"<mask> <level>"`, over the `port-in` drivers so Clicks and board buttons keep their pads) and `pX-pull` (`"<mask> <up>"`, the pads with a pull resistor on), in both the SAM/PIC32CM (U2210) and AVR8X PORT models |
| 0037 | EDBG data gateway (DGI) on `pic32cm-mc00-cpro`: the DGI host protocol on a chardev (`-M ...,dgi=<chardev>`, the bytes of a debugger's USB DGI endpoints; command layer checked against an nEDBG), I2C target 0x28, SPI target (SERCOM1, SS PA17), GPIO0-3 (PA10/PA11/PA20/PA21) with 2 MHz timestamps; `mchp_port_watch()` pad observers; SERCOM SPI ignores an idle DGI target when reading an open MISO |
| 0038 | PIC32CM JH/MC ADC (U2247): conversions on START and free-running, AIN0-11 from the board (`ain`), INTREF/VDDCORE/VDDANA inputs, the data sheet's references (INTVCC0 = VDDANA/1.48 pending silicon), 16-bit averaging, differential, correction, window monitor; `adc_u2247` overlay and regenerated tables |
| 0039 | U2247 ADC checked on MC00 silicon (`APP=adc`): INTREF is SUPC VREF (1.024 V at reset) and converts only with VREF.VREFOE (else START stays set), INTVCC0 = VDDANA/1.6 (DFP right, data sheet table wrong), REFCTRL enable-protected |
| 0040 | PIC32CM JH/MC DAC (U2214): 10-bit, INTREF/VDDANA/VREFA, VOUT on PA02 (`vout` property for front ends), read back by ADC0 AIN0 and used as the ADC's DAC reference; checked on MC00 silicon (`APP=dac`); `dac_u2214` overlay |
| 0041 | AVR8X 12-bit differential ADC (AVR64EA48): single/8-bit/series/burst with accumulation and scaling, differential, window, three vectors; AIN0-31 (`ain`), VDD/10, temperature sensor via SIGROW calibration; all references; overlay |
| 0042 | AVR8X ADC checked on AVR64EA48 silicon (`APP=adc`): an internal reference above VDD acts as VDD; the desk kit's SIGROW TEMPSENSE0/1 in the device descriptor |
| 0043 | PIC32CM MC00 sigma-delta ADC (SDADC U2260): three differential pairs (`ain`), INTREF/AREFB/DAC/INTVCC, 24-bit result with offset (added), gain and shift correction and saturation, window, free-running; checked on MC00 silicon (`APP=sdadc`); `sdadc_u2260` overlay |
| 0044 | mikroBUS AN (a Click's analog output in mV onto the ADC channels behind the pad; MC00 EXT1/EXT2) and UART (a UART Click is a chardev on the socket's UART, linked as `/machine/<socket>-click`; MC00 SERCOM3, Nano Base sockets 1/3 USART2); POT Click (`click-pot`, MCP1501 2.048 V, buffered onto AN); qtest |
| 0045 | GNSS Click: Quectel L86 (`chardev-quectel-l86`) on the socket's UART, 9600 baud paced; boot lines, NMEA 4.10 sentences in the module's order and talkers with and without a fix, PMTK220/251/314/161/101-104 with ACKs, RESET on RST; position, altitude, satellites as QOM properties |
| 0046 | Rotary G/R/B/Y Click (`click-rotary`: two 74HC595 for the 16-LED ring plus an EC12D encoder, quadrature on PWM and AN, A = B at detents, A leading clockwise, push switch on INT); mikroBUS AN as a digital pin (`an_pin`); four outputs per Click; qtest |
| 0047 | RN4871 Click (`chardev-rn4871`: the RNBD451 model with the RN4871's identity); the shared model follows DS50002466C closer: "$$$" only for close '$' (MikroElektronika's "$", pause, "$$$"), leading '$' dropped, SF/C/K,1/I/B/&/GN, %STREAM_OPEN% only with Transparent UART; MIKROBUS selects RNBD451 |
| 0048 | EEPROM 3 Click: AT24CM02 (`mchp-at24cm02`) at I2C 0x50-0x53 (A17:A16 in the address), 256-byte page writes wrapping in the page, write cycle at STOP without ACK, address counter kept between transfers; `view-at`/`view` for front ends |
| 0049 | OLED W Click: SSD1306 (`ssd1306`) on SPI with D/C# and RES#, three addressing modes, re-map, MUX, COM direction/offset/layout, inverse, charge pump; panel geometry as properties (MI9639BO-W 96x39 defaults); PpPanel `frame` |
| 0050 | OLED B Click: `ssd1306-i2c` (0x3C/0x3D by SA0 on D/C at each START, control bytes, an `ssd1306` on a private SSI bus inside); catalog `props` for device properties (the blue panel) |
| 0051 | Color 2 Click: ISL29125 (`isl29125`) at I2C 0x44, G/R/B conversions in mode order (101 ms at 16 bits, 1/16 at 12), 375/10000 lux range, threshold (with persistence) and conversion-done interrupts on the open-drain INT, cleared by reading STATUS, the 0x46 reset; the light as `red`/`green`/`blue`/`lux`; qtest |
| 0052 | Weather Click: BME280 (`bme280`, I2C) at 0x76, sleep/forced/normal with typical measurement and standby times, oversampling resolution, IIR filter, skipped readings, results held during a burst read, 0xB6 reset; typical calibration, raw readings by bisection over the data sheet's compensation so `temperature`/`pressure`/`humidity` read back exactly; qtest |
| 0053 | Fan Click: EMC2301 (`emc2301`) at 0x2F with a fan on it: Direct Setting and the RPM-based algorithm, spin-up, stall/spin/drive-fail status, power-up watchdog, software lock, TACH Reading by equation 4-2 from the fan's speed (`max-rpm`, `min-duty`, `tau-ms`, `poles`, `blocked`; `rpm`, `duty`); ALERT# on INT; qtest |
| 0054 | LR 2 Click: RN2903 (`chardev-rn2903`) on the UART at 57600: sys/mac/radio commands with their documented errors, OTAA/ABP joins after the join window, uplinks after RX1, radio tx after the LoRa time on air, radio rx with the radio watchdog; the network and the other radio as properties (`join-accept`, `downlink`, `peer-send`, `air`); CTS on INT pulled low |

## Setting it up

The QEMU path must not contain spaces (QEMU's configure rejects them). Clone this repo somewhere
like `~/src/peripheral-pretender`, so the fork lands at `~/src/pp-qemu`.

```
qemu/setup.sh            # clone v11.1.2 to ../pp-qemu, apply patches, configure, build
qemu/setup.sh /path/qemu # or choose the location
```

Build dependencies:

- **Ubuntu/Debian:** `sudo apt install git build-essential ninja-build python3 python3-venv
  pkg-config libglib2.0-dev libpixman-1-dev libfdt-dev flex bison gcc-arm-none-eabi
  libnewlib-arm-none-eabi gdb-multiarch`
- **Windows (MSYS2, UCRT64 shell):** `pacman -S --needed base-devel git mingw-w64-ucrt-x86_64-toolchain
  mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pixman
  mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-dtc mingw-w64-ucrt-x86_64-arm-none-eabi-toolchain`.
  MSYS2 deprecated the MINGW64 environment in March 2026; QEMU's docs still mention it, but use UCRT64.
- **macOS:** `brew install ninja pkg-config glib pixman dtc` plus an Arm GNU toolchain.

Configure options used everywhere: `--target-list=arm-softmmu --without-default-features
--enable-fdt=system --disable-docs --disable-werror` in `build/`, and for the 8-bit AVR boards
`--target-list=avr-softmmu --without-default-features --disable-docs --disable-werror` in
`build-avr/` (one QEMU binary per CPU family; `tools.pp` picks it from the ELF). `-Werror` is off because GCC 16 is newer than
anything QEMU 11.1 was tested with.

## Working on the fork

Edit and commit in `pp-qemu` on `pp/main`, then refresh the patches here:

```
rm qemu/patches/*.patch
git -C ../pp-qemu format-patch v11.1.2..pp/main -o "$PWD/qemu/patches"
```

Once the fork grows, it may be easier to keep it as its own private GitHub repo (a fork of
`qemu/qemu` with a `pp/main` branch) and drop the patch files.

## Running

```
../pp-qemu/build/qemu-system-arm -M pic32cm-jh01-cpro -kernel firmware/hello/build/pic32cm_jh01_cpro/hello.elf \
    -display none -serial stdio -d unimp,guest_errors
python -m tools.pp run firmware/hello/build/pic32cm_jh01_cpro/hello.elf     # runner: UART transcript + exit code
```

For a debugger, use `-gdb tcp:127.0.0.1:1234 -S`, not `-s`. On Windows, `-s` listens on every
interface and triggers a Windows Firewall prompt (an admin dialog) for each QEMU binary location.
