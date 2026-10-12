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
| 0005 | **WIP** part B: SERCOM and PORT models, written but not wired in or tested |

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
--enable-fdt=system --disable-docs --disable-werror`. `-Werror` is off because GCC 16 is newer than
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
../pp-qemu/build/qemu-system-arm -M pic32cm-jh01-cpro -kernel firmware/hello/build/hello.elf \
    -display none -serial stdio -d unimp,guest_errors
python -m tools.pp run firmware/hello/build/hello.elf     # runner: UART transcript + exit code
```

For a debugger, use `-gdb tcp:127.0.0.1:1234 -S`, not `-s`. On Windows, `-s` listens on every
interface and triggers a Windows Firewall prompt (an admin dialog) for each QEMU binary location.
