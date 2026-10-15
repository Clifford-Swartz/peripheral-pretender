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

## Open questions (answered 2026-10-14)

1. **Which IMU?** Both. The Ruler carries an ICM-20948 at 0x69 (schematic EV82H04A rev 3, U3). The
   kit on the desk also answers at 0x4A: an **Adafruit BNO085 breakout** (STEMMA QT), an add-on
   plugged into the QWIIC connector, not part of the PyKit (confirmed by the owner 2026-10-15). The
   kit's own `code.py` uses it.
2. **Design files.** Schematics exist (Curiosity CircuitPython 02-01318 rev 3, 2025-09-05; PyKit
   Ruler 03-02-01326 rev 3, 2025-09-10; under `C:/pp/pykit/`). No Gerbers or pick-and-place: the
   product pages for EV82H04A and EV78P89A redirect to the development-tool listing. The board art
   is composed from the renders in the workshop slides (`tools/pp_web/boards/pykit/make_art.py`).
3. **The kit's CircuitPython:** see "The kit on the desk" below. The NeoPixel pin is PB22
   (`board.NEOPIXEL`), one chain of five: D3 on the Curiosity board, then D1-D4 on the Ruler.

## The kit on the desk (read-only REPL probes, 2026-10-14)

Probed with `py -m harness.cp_repl` (Ctrl-C, REPL lines, then always Ctrl-D so its `code.py` runs
again; nothing written). Transcripts: `C:/pp/trial-pykit/work/kit_probe1.txt`, `kit_probe2.txt`.

| What | Kit |
|---|---|
| `boot_out.txt` / banner | Adafruit CircuitPython 10.1.0-alpha.0-2-g706c4b2abf on 2025-10-27; Microchip Curiosity CircuitPython with same51j20 |
| `sys.implementation` | version (10, 1, 0), `_mpy` 774, `_build` microchip_curiosity_circuitpython |
| `os.uname()` | sysname/nodename `same51j20`, release 10.1.0 |
| USB | composite 04D8:E52B: CDC (REPL), mass storage (CIRCUITPY), HID, audio, MIDI |
| `cpu.frequency` | 120000000 |
| `cpu.uid` | f3 e5 04 00 54 32 50 53 20 20 20 41 21 36 10 ff |
| `cpu.temperature`, `cpu.voltage` | 36.331 C, 3.30354 V |
| `cpu.reset_reason` | UNKNOWN |
| I2C scan (`board.I2C()`) | 0x4A (BNO085, QWIIC), 0x69 (ICM-20948) |
| `os.statvfs('/')` | (2048, 2048, 2036, 956, 956, 0, 0, 0, 0, 255): 4 MiB flash, FAT with 2036 clusters of 2 KiB |
| `storage.getmount('/').label` | CIRCUITPY |
| Auto-reload | off (the kit's `code.py` turns it off) |
| `dir(board)` | A0-A5, BLE_CLR/RX/TX, CAN_RX/STDBY/TX, CAP1, CS, D0-D13 (no D12), DAC, DEBUG_RX/TX, I2C, IMU_ADDR, IMU_INT, LCD_BL/CS/MOSI/SCK, LCD_SPI, LED, MISO, MOSI, NEOPIXEL, SCK, SCL, SDA, SD_CS/MISO/MOSI/SCK, SD_SPI, SPI, UART, VREF, board_id |

RNBD451 on `busio.UART(board.BLE_TX, board.BLE_RX, 115200)`:

| Sent | Reply |
|---|---|
| `$$$` | `CMD> ` |
| `V\r\n` | `RNBD451 V1.0.0.10 Jul 20 2022 (c)Microchip Technology Inc\r\nCMD> ` |
| `D\r\n` | (first lines lost in the 64-byte UART buffer) `..._IMU`, `Connected=no`, `Authen=2`, `Features=0080`, `Services=C0`, `CMD> ` |
| `---\r\n` | `END\r\n` |

The kit's `code.py` (the owner's robot-arm project, backed up at
`C:/pp/pykit/device-backup-2026-10-14`) reads the BNO085, configures the RNBD451 and streams
orientation over BLE.

## The twin against the kit

The twin runs Adafruit's official 10.1.4 build for the board, unmodified (the kit runs a
10.1.0 alpha). Same lines through `py -m tools.pp.circuitpython run <drive> --lines ...`:

| What | Twin | Same as the kit? |
|---|---|---|
| USB | 04D8:E52B, the same interface list | yes |
| `cpu.frequency` | 120000000 | yes |
| `cpu.uid` | the kit's (profile `serial_default`) | yes |
| `cpu.temperature`, `cpu.voltage` | 25.0 C, 3.29963 V | no: the ADC model's die temperature defaults to 25 C (property `temperature`); the TEMP_LOG calibration row is a placeholder |
| I2C scan | 0x4A, 0x69 with `bno085=on` (0x69 alone without) | yes |
| `os.statvfs('/')` | 2048-byte blocks, 2036 clusters | yes (the image is formatted as CircuitPython formats it) |
| RNBD451 replies | as in the table above | yes (from the kit) |
| The kit's own `code.py` (a copy) | runs: IMU ready, BLE configured, streaming, "[MOTOR] p=-0.0 r=-0.0 y=180.0" | yes |
| `dir(board)` | the same names | yes |

Known differences: prints made before the host opens the CDC port (DTR) are dropped, as on a
real PC; the first `code.py` lines can be missing from the console. Not modelled: CAN, SD card,
capacitive touch, the BME680/APDS-9999 plug-ins, BLE radio traffic (the module answers commands
only).

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
