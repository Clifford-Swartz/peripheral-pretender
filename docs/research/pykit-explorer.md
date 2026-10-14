# PyKit Explorer: research notes (Phase 0 inputs)

Gathered 2026-10-14, before onboarding. Sources:

- Microchip blog "Meet the PyKit Explorer" (Ross Satchell, 2026-03-17).
- github.com/MicrochipTech/PyKit-Explorer-Code-Modules (cloned at C:/pp/pykit): API modules,
  libraries, assets and the starter `code.py`.
- github.com/MicrochipTech/pykit-provisioning-tool: writes the content to the `CIRCUITPY` drive
  from a private Microchip Bitbucket repository.
- CircuitPython's board definition `ports/atmel-samd/boards/microchip_curiosity_circuitpython`
  (adafruit/circuitpython).

## What it is

The Microchip **Curiosity PyKit Explorer** is a CircuitPython kit from the Academic Program, shaped
like a 12-inch ruler. It is two boards:

- **Curiosity CircuitPython** board: SAME51J20A (Cortex-M4F, 120 MHz, 1 MB flash, 256 KB SRAM,
  CAN FD). It has QSPI flash (SST26VF016B/032B/064B) holding the CIRCUITPY filesystem, a 32.768 kHz
  crystal, a status LED and one NeoPixel. USB VID:PID 04D8:E52B.
- **PyKit Ruler** baseboard, on a friction-fit header. It carries a 1.14" 240x135 ST7789 TFT, a
  9-axis IMU, a class-D amplifier and 8-ohm speaker, five NeoPixels, a microSD slot, an RNBD451
  BLE module, CAN, capacitive touch and a STEMMA/QWIIC I2C connector.
- Plug-in sensors in the kit: a BME680 (temperature, humidity, pressure, gas) and an APDS-9999
  (proximity, light, colour). The blog says APDS-9960; the code and README say APDS-9999.

Software: CircuitPython. The user edits `code.py` on the `CIRCUITPY` USB drive, and the REPL and
`print` output go to USB CDC. Workshops progress from GPIO to IMU and display, sensor dashboards,
audio and gesture, then full games (Donkey Kong, Snake, Super Mario Bros) controlled by tilting.

## Pins (CircuitPython board definition)

| Function | Pins |
|---|---|
| Status LED (board.LED, D13) | PB23 |
| NeoPixel on the Curiosity board | PB22 |
| I2C (STEMMA, sensors, IMU) | SCL PB30, SDA PB31 |
| board.SPI | SCK PB03, MOSI PB02, MISO PB00 (also IMU_INT); CS PB01 (also IMU_ADDR) |
| LCD (ST7789) on LCD_SPI | SCK PA05, MOSI PA04, CS PA07, backlight PA06 |
| microSD on SD_SPI | SCK PA17, MOSI PA16, MISO PA18, CS PA19 |
| BLE (RNBD451) UART | TX PA12, RX PA13, BLE_CLR (reset) PA14 |
| Debug UART (board.UART) | TX PA22, RX PA23 |
| CAN | TX PB12, RX PB13, standby PB17 |
| DAC (audio) | PA02 |
| Analog A0..A5 | PB04..PB09 (A5 = PB09 = CAP1, touch) |
| Digital D0..D6 | PA15, PA20, PA21, PA27, PB14, PB15, PB16 |
| VREF | PA03 |

The API modules also use `board.NEOPIX`, `board.BOOST_ENABLE` and `board.LCD_*`. These names are
not in the upstream definition read here, so the kit runs a newer or Microchip-specific
CircuitPython build. Read the real board's `boot_out.txt` for its version.

## Open questions

1. **Which IMU?** `imu_sensor.py` says BNO085 (I2C 0x4A/0x4B, adafruit_bno08x). `reg_peek_poke.py`
   and the bundled `adafruit_icm20x` library say ICM-20948 (0x69). Ruler revisions may differ:
   check with an I2C scan on the real kit.
2. **Design files** (schematic, Gerbers) for the Curiosity CircuitPython board and the Ruler. The
   blog (March 2026) said "not yet available"; check the product pages.
3. **The kit's CircuitPython version and build**, and the NeoPixel pin (PB22 vs `NEOPIX`).

## What a twin needs, hardest first

- **USB device.** CircuitPython's console (USB CDC) and the `CIRCUITPY` drive (USB mass storage)
  go over the SAME51's USB controller. The twin needs a USB device model bridged to the page:
  console in the page, files from a folder. Alternatively, start with a prebuilt QSPI flash image
  (FAT with code.py and lib/) and the debug UART, keeping the stock firmware unchanged.
- **DMA.** CircuitPython uses the DMAC for displayio SPI, audio and more: a DMAC model is required.
- **QSPI** with an SST26 flash model (the filesystem).
- **Chip:** SAME51 through the generator (SAMD5x/E5x DFP). SERCOM U2201 and PORT U2210 are
  probably shared with the PIC32CM models; check the versions. New: Cortex-M4F (QEMU has
  cortex-m4 with FPU), clocks (OSCCTRL/GCLK/MCLK versions), TC/TCC versions, ADC, DAC, MCAN, USB.
- **The fun parts**, each a visible feature in the web view:
  - ST7789 framebuffer, shown live on the page;
  - IMU you tilt with the mouse (tilt games);
  - DAC audio played in the browser;
  - five NeoPixels (WS2812 timing decoded from the pad);
  - sliders for the BME680 and APDS-9999;
  - SD card image (QEMU's ssi-sd);
  - RNBD451 as a "phone" chat on the page.
- **Checking:** the real kit (if on the desk) via its debug UART and CIRCUITPY drive. CircuitPython
  examples are .py, so acceptance is "copy code.py, observe"; pp_accept needs a CircuitPython mode.
