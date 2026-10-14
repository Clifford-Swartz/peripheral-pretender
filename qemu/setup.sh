#!/usr/bin/env bash
# Recreate the pp-qemu fork next to this repo and build qemu-system-arm
# (build/) and qemu-system-avr (build-avr/, the 8-bit AVR boards).
#
#   qemu/setup.sh [QEMU_DIR]      default: <repo>/../pp-qemu
#
# Clones upstream QEMU v11.1.2, creates branch pp/main, applies
# qemu/patches/*.patch with 'git am', then configures an arm-softmmu build
# in build/ and an avr-softmmu build in build-avr/ and runs ninja in both.  Works on Linux, macOS and MSYS2 UCRT64 (Windows).
# Re-running on an existing checkout skips the clone and the patches.
#
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
set -euo pipefail

REPO=$(cd "$(dirname "$0")/.." && pwd)
QEMU_DIR=${1:-$(cd "$REPO/.." && pwd)/pp-qemu}
QEMU_URL=${QEMU_URL:-https://github.com/qemu/qemu.git}
QEMU_TAG=v11.1.2

case "$QEMU_DIR" in
    *[[:space:]]*)
        echo "error: QEMU's configure rejects paths with spaces: $QEMU_DIR" >&2
        echo "pass a path without spaces as the first argument" >&2
        exit 1 ;;
esac

if [ ! -d "$QEMU_DIR/.git" ]; then
    git -c core.autocrlf=false clone --branch "$QEMU_TAG" --depth 1 \
        "$QEMU_URL" "$QEMU_DIR"
    git -C "$QEMU_DIR" config core.autocrlf false
    git -C "$QEMU_DIR" switch -c pp/main
    if ! git -C "$QEMU_DIR" config user.email >/dev/null; then
        git -C "$QEMU_DIR" config user.name "Peripheral Pretender setup"
        git -C "$QEMU_DIR" config user.email "pp-setup@localhost"
    fi
    git -C "$QEMU_DIR" am "$REPO"/qemu/patches/*.patch
else
    echo "using existing checkout $QEMU_DIR ($(git -C "$QEMU_DIR" log --oneline -1))"
fi

# arm-softmmu needs libfdt; use the system library when present.
if pkg-config --exists libfdt 2>/dev/null; then fdt=system; else fdt=internal; fi

mkdir -p "$QEMU_DIR/build"
cd "$QEMU_DIR/build"
if [ ! -f build.ninja ]; then
    ../configure \
        --target-list=arm-softmmu \
        --without-default-features \
        --enable-fdt="$fdt" \
        --disable-docs \
        --disable-werror
fi
ninja

exe=qemu-system-arm
[ -f qemu-system-arm.exe ] && exe=qemu-system-arm.exe
echo
"./$exe" -M help | grep -i pic32cm || {
    echo "error: pic32cm-jh01-cpro machine missing from the build" >&2
    exit 1
}
echo "built $QEMU_DIR/build/$exe"

# 8-bit AVR boards (avr64ea48-cnano) run on qemu-system-avr: tools.pp picks
# the binary from the ELF ($PP_QEMU_AVR, or build-avr/ beside $PP_QEMU).
mkdir -p "$QEMU_DIR/build-avr"
cd "$QEMU_DIR/build-avr"
if [ ! -f build.ninja ]; then
    ../configure         --target-list=avr-softmmu         --without-default-features         --disable-docs         --disable-werror
fi
ninja
exe=qemu-system-avr
[ -f qemu-system-avr.exe ] && exe=qemu-system-avr.exe
"./$exe" -M help | grep -i avr64ea48 || {
    echo "error: avr64ea48-cnano machine missing from the build" >&2
    exit 1
}
echo "built $QEMU_DIR/build-avr/$exe"
