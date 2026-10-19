# Setting up Peripheral Pretender on a new machine

Written for an AI coding agent (or a person) setting this up from scratch. Every step ends with
a check; do not go on until it passes. Windows 11 x64 is the tested host. Linux and macOS
should work for the QEMU and Python parts (see `qemu/README.md`), but no one has tried them yet.

Pick a path:

- **A. Just run it.** Install the VS Code extension. It carries QEMU, Python and gdb, so you
  need no compilers or builds. About 5 minutes.
- **B. Develop it.** Build the QEMU fork, firmware and tools, and run the test suites. About
  an hour, most of it installing tools; the QEMU build itself takes minutes.

Before touching hardware, read the rules in `CLAUDE.md`. A physical board is read-only unless
its owner approves programming, erasing, resetting or halting it.

## A. Just run it (VS Code extension)

1. Install [VS Code](https://code.visualstudio.com/). To debug on the twin you also need the
   C/C++ extension (`ms-vscode.cpptools`); the MPLAB extension pack includes it.
2. Download the extension from this repository's Releases and install it:

   ```
   gh release download --repo Clifford-Swartz/peripheral-pretender --pattern "*.vsix"
   code --install-extension peripheral-pretender-win32-x64-0.4.2.vsix
   ```

3. **Check:** reload VS Code. A chip icon appears in the activity bar on the left. Click it and
   then a board, for example "PIC32CM MC00 Curiosity Pro". The board opens beside the editor
   running its demo firmware, and its LED blinks.

How to use it (boards, Clicks, F5 debugging, MPLAB builds) is in `vscode/README.md`.

## B. Develop it (Windows 11)

### 1. Tools

| Tool | How | Needed for |
|---|---|---|
| Git, GitHub CLI | `winget install Git.Git GitHub.cli` | everything |
| Python 3.12 | `winget install Python.Python.3.12` (the `py` launcher) | tools, tests, generator |
| MSYS2 | `winget install MSYS2.MSYS2` (installs to `C:\msys64`) | QEMU build, Arm firmware |
| MPLAB XC8 v4.00 | microchip.com, default install path | AVR firmware (`firmware/avr-hello`) |
| MPLAB X v6.20 + XC32 | microchip.com | only `examples/mplab/` |
| Node.js 22 | `winget install OpenJS.NodeJS.LTS` | only rebuilding the VS Code extension |

In the **MSYS2 UCRT64** shell (Start menu: "MSYS2 UCRT64"):

```
pacman -Syu          # updates the core and closes the shell: open it again and repeat
pacman -Syu
pacman -S --needed base-devel git \
  mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-meson mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-glib2 mingw-w64-ucrt-x86_64-pixman \
  mingw-w64-ucrt-x86_64-dtc mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-gdb-multiarch \
  mingw-w64-ucrt-x86_64-arm-none-eabi-gcc mingw-w64-ucrt-x86_64-arm-none-eabi-newlib
```

**Check:** in UCRT64, `arm-none-eabi-gcc --version` and `ninja --version` both answer.

### 2. Clone and Python packages

The repository can live anywhere, even in a path with spaces. The QEMU fork cannot, because
QEMU's configure rejects spaces, so it goes in `C:\pp\pp-qemu` below. The MPLAB example has
paths up to 171 characters inside the repository: clone into a short folder (such as
`C:\src`), or enable long paths first, or the checkout fails with "Filename too long".

```
git config --global core.longpaths true
git clone https://github.com/Clifford-Swartz/peripheral-pretender.git
cd peripheral-pretender
py -m pip install --user -r requirements.txt
py scripts/fetch_packs.py
```

`fetch_packs.py` downloads the pinned device packs (PIC32CM-JH, PIC32CM-MC, AVR-Ex, SAME51) and
CMSIS into `%USERPROFILE%\.mchp_packs`.

**Check:** `py -m pytest tests/generator -q` passes; skips are fine at this point.

### 3. Build the QEMU fork

The fork is upstream QEMU v11.1.2 plus the patches in `qemu/patches/`. Never send any of it
upstream: QEMU declines AI-generated contributions. In the UCRT64 shell (`/c/...` paths):

```
cd "/c/<path to>/peripheral-pretender"
qemu/setup.sh /c/pp/pp-qemu
```

This clones QEMU, applies the patches with `git am` and builds `qemu-system-arm` (in
`build/`) and `qemu-system-avr` (in `build-avr/`). Measured on a 32-thread PC: 47 s to
configure and 5 min to compile the Arm build from scratch, the AVR build less; expect 10-30
minutes depending on cores, plus the clone. Changing a model later rebuilds only what it touches
(`ninja` in `build/`), usually seconds.

**Check:** `/c/pp/pp-qemu/build/qemu-system-arm.exe -M help | grep pic32cm` lists
`pic32cm-jh01-cpro` and `pic32cm-mc00-cpro`, and `/c/pp/pp-qemu/build-avr/qemu-system-avr.exe
-M help | grep cnano` lists `avr64ea48-cnano`.

### 4. Environment variables

The tools find QEMU through these. Set them once from PowerShell or cmd:

```
setx PP_QEMU      C:/pp/pp-qemu/build/qemu-system-arm.exe
setx PP_QEMU_AVR  C:/pp/pp-qemu/build-avr/qemu-system-avr.exe
setx PP_QEMU_SRC  C:/pp/pp-qemu
setx PP_MSYS2_BIN C:/msys64/ucrt64/bin
```

Open a new shell afterwards. `PP_MSYS2_BIN` is where QEMU's DLLs live. To run QEMU by hand
outside MSYS2, put that folder on `PATH` first.

### 5. Firmware

The test firmware runs unchanged on the twin and on the real boards. Build it in UCRT64. The
Makefiles need the pack folder spelled out, because `bash -lc` does not see `USERPROFILE`:

```
P="C:/Users/<you>/.mchp_packs"
make -C firmware/hello PACKS="$P"                              # JH01 Curiosity Pro
make -C firmware/hello BOARD=pic32cm_mc00_cpro PACKS="$P"      # MC00 Curiosity Pro
for f in firmware/hello/tests/*.c; do                          # the demos and checks
  make -C firmware/hello BOARD=pic32cm_mc00_cpro APP=$(basename "$f" .c) PACKS="$P" \
    >/dev/null 2>&1 || echo "skipped $(basename "$f" .c) (another board's)"
done
make -C firmware/avr-hello PACKS="$P"                          # AVR64EA48 (needs XC8)
```

**Check:** `py -m tools.pp run firmware/hello/build/pic32cm_mc00_cpro/hello.elf --machine
pic32cm-mc00-cpro` prints the firmware's transcript and exits 0.

### 6. Generated tables and the test gate

The generator turns the device packs' ATDF files into QEMU register tables and
`devices/*.json`. Its report files are not committed, so on a fresh clone run it once. The
exact command is in the assertion message of `tests/generator/test_outputs.py`; it starts
`py generator/atdf2qemu.py --atdf ...`.

```
py -m pytest tests harness/tests -q
```

**Check:** the run exits 0. On the original machine that is about 390 passed. A test whose
firmware was not built skips and names the `make` command it needs. Gate on the exit status,
not on the word "passed".

### 7. The web board view and the extension

```
py -m tools.pp_web --port 8478 --board ev15n46a
```

Open http://127.0.0.1:8478, drop an ELF on the page, plug Clicks into the headers.
`--watch <project folder>` runs each new build of an MPLAB project as it lands.

To rebuild the VS Code extension: `py scripts/build_vsix.py` (needs Node 22; in `vscode/`, run
`npm ci` once first). It stages QEMU, Python and gdb into `vscode/engine/`, smoke-tests them and
writes `vscode/peripheral-pretender-win32-x64-<version>.vsix`.

## Known gotchas

- **Spaces in paths.** QEMU's configure rejects them; that is why the fork lives in
  `C:\pp\pp-qemu`.
- **Line endings.** `.gitattributes` keeps the patches, the shell scripts and the generated
  data byte-exact. If `git am` or `setup.sh` fails with odd errors, check that
  `qemu/patches/*.patch` and `qemu/setup.sh` have LF line endings in your checkout.
- **QEMU DLLs.** `qemu-system-*.exe` needs `C:\msys64\ucrt64\bin` on `PATH`, or `PP_MSYS2_BIN`
  for the tools.
- **Debug port.** Debug QEMU with `-gdb tcp:127.0.0.1:<port>`, never `-s`: on Windows it binds
  every interface and Windows Firewall asks for admin.
- **AVR busy-wait delays.** `_delay_ms()` is not to scale on the AVR twin, because icount counts
  instructions, not cycles. Timer-based timing is exact. This is an open item in
  `docs/handoff.md`.

## What to read next

1. `CLAUDE.md`: the project's decisions and rules, written for coding agents.
2. `docs/handoff.md`: where everything stands, and the open items.
3. `playbooks/README.md`: adding a board or a Click board, step by step.
4. `docs/design/v0-vertical-slice.md` and `docs/walkthrough.md`: how the twin is built.
