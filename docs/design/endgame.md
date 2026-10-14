# Endgame: the board in a browser

Owner's vision (2026-10-14). A web page first, and later a VS Code extension that uses the
same frontend. Someone sees a 3D model of a real Microchip board and plugs Mikroe Click
add-on boards into it. They upload their own firmware, or have the AI coding assistant write
it, and watch it run on the board in real time: LEDs light, displays draw, sensors respond.

Decisions so far:
- **Web first.** The VS Code extension embeds the same page in a webview.
- **Add-ons are Mikroe Click boards** (mikroBUS). The Curiosity Pro boards take them on the
  EXT1/EXT2 headers through an adapter.
- **Board geometry comes from Microchip's published design files**: Gerbers and
  pick-and-place for 2D and part positions, Altium CAD for 3D models later.

## Layers

1. **Chip twin** (done for the Tier 1 MC00 peripherals, checked against silicon). This is
   pp-qemu with firmware unmodified.
2. **Pin level.** Every peripheral function reaches the pads through the pin mux: TCC
   waveforms, SERCOM SPI/I2C/UART pins, ADC inputs, EIC inputs. Today only GPIO and the EIC
   inputs do.
3. **Board and part layer.** A board file gives connectors, nets and on-board parts. A part
   library holds LEDs, buttons, potentiometers and Click boards; parts that need code run
   out of process through an SDK.
4. **State stream.** Pad levels with virtual timestamps (so PWM shows as brightness), UART,
   and display framebuffers, over one WebSocket protocol.
5. **Frontend.** three.js: the board as a slab textured from the Gerber renders, then real
   component models. Clickable buttons, glowing LEDs, drag-and-drop Click boards.
6. **Hosting.** One sandboxed twin per visitor (a container running QEMU), with firmware
   upload and a build service for assistant-written code. The MCP server (`tools/pp_mcp`)
   is the assistant's interface.

## Roadmap

1. ~~First slice~~ (`tools/pp_web`, 2026-10-14). The EV15N46A rendered from its Gerbers.
   LED0 follows PA24 in real time; SW0 and RESET are clickable; the VCOM console works; ELF
   upload switches firmware.
2. ~~PWM brightness~~ (2026-10-14). Peripheral outputs reach the pads through the pin mux
   (PORT `pmux-in`, wired from the signal table), starting with the TCC waveform outputs.
   PORT accounts each pad's driven-high and driven-low time, and the page shows duty cycle as
   brightness (`breathe` demo). The TCC1 WO[2] pin on PA24/PA08 was checked on silicon: duty
   CC/(PER+1); floating with the pull off while the TCC is disabled; the start-of-cycle level
   after STOP.
3. ~~SERCOM SPI and I2C~~ (2026-10-14). Host modes on QEMU's SSI and I2C buses, checked on
   silicon. On the board that means the EDBG data gateway at 0x28 (the ATECC608A footprint is
   not fitted) and an SPI loopback through the MOSI pad.
3a. ~~First Click board~~ (2026-10-14). Thermo 8 Click (MCP9808) on EXT1 or EXT2
   (`-M pic32cm-mc00-cpro,ext1=thermo8`). The page has a Click picker per header and a live
   temperature slider; ALERT on the INT pin shows as a signal light, and the `thermo` demo
   lights LED0 from it. Modelled from the data sheet only (no physical Click to check against).
   **Next:** an LED/button Click (GPIO), a potentiometer Click (needs an ADC model), an SPI
   Click.
4. **3D board.** A glTF slab with top and bottom textures, components from the pick-and-place
   heights, and later models from the Altium design.
5. **Hosted demo.** A containerized twin per session, a firmware build service, and the
   assistant writing code against the board.
