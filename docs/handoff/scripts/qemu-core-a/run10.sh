#!/usr/bin/env bash
# hello.elf for 10 s with -d unimp,guest_errors; reports how QEMU ended.
D=/c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
timeout 10 ./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel "$D/hello.elf" \
    -display none -serial null -monitor none -d unimp,guest_errors \
    -D "$D/run10.log" 2> "$D/run10.stderr"
echo "qemu exit=$? (124 = still running at the 10 s timeout)"
echo "stderr:"; cat "$D/run10.stderr"
echo "log ($(wc -l < "$D/run10.log") lines):"; cat "$D/run10.log"
