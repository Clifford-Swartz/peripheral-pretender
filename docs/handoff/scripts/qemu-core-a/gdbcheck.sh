#!/usr/bin/env bash
# Run hello.elf under -s -S and inspect it with gdb-multiarch in batch mode.
D=/c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a
ELF=$D/hello.elf
cp "/c/Users/C19257/Desktop/Code/Peripheral Pretender/firmware/hello/build/hello.elf" "$ELF"
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel "$ELF" -display none \
    -serial null -monitor none -s -S -d unimp,guest_errors \
    -trace 'mchp_clk_update' -D "$D/gdbcheck-qemu.log" &
QPID=$!
sleep 2
gdb-multiarch -batch -nx -ex "file $(cygpath -m "$D/hello.elf")" -x "$D/gdbcheck.gdb" > "$D/gdbcheck.out" 2>&1
kill $QPID 2>/dev/null; wait $QPID 2>/dev/null
sed -n '1,/=== info qtree/p' "$D/gdbcheck.out" | head -90
echo "=== qtree clock lines"
grep -n -E 'dev: armv7m,|"cpuclk"|dev: mchp-mclk|"cpu"|"gclk-main"|"gen\[0\]"|"pch\[22\]"|"osc48m"' "$D/gdbcheck.out" | head -20
echo "=== qemu log"
cat "$D/gdbcheck-qemu.log"
