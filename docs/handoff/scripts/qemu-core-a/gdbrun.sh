#!/usr/bin/env bash
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
which gdb gdb-multiarch 2>&1 | head -3
GDB=$(command -v gdb || command -v gdb-multiarch)
"$GDB" -batch -ex run -ex bt -ex "info registers rip" --args ./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel "/c/Users/C19257/Desktop/Code/Peripheral Pretender/firmware/hello/build/hello.elf" -display none -serial null -monitor none 2>&1 | grep -v "^\[New Thread\|^\[Thread" | tail -40
