#!/usr/bin/env bash
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
./qemu-system-arm.exe -M help | grep -i -E "pic32|microbit"
echo "---- run hello.elf 3s"
timeout 3 ./qemu-system-arm.exe -M pic32cm-jh01-cpro -kernel "/c/Users/C19257/Desktop/Code/Peripheral Pretender/firmware/hello/build/hello.elf" -display none -serial null -monitor none -d unimp,guest_errors -D /tmp/pp-run1.log
echo "exit=$?"
wc -l /tmp/pp-run1.log; sort /tmp/pp-run1.log | uniq -c | sort -rn | head -40
