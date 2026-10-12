#!/usr/bin/env bash
# Build and run qtests. Usage: qtest.sh [test-name ...] (default: ours)
D=/c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
tests=("${@:-pic32cm-jh-test}")
targets=(qemu-system-arm.exe)
for t in "${tests[@]}"; do targets+=("tests/qtest/$t.exe"); done
ninja -k 0 "${targets[@]}" > "$D/qtest-build.log" 2>&1
echo "BUILD_EXIT=$?"
grep -v "cannot symlink" "$D/qtest-build.log" | grep -E "FAILED|error:|warning:" -A3 | head -40
export QTEST_QEMU_BINARY=./qemu-system-arm.exe
# pc-bios is not installed into the build tree on Windows (symlinks fail)
export QTEST_QEMU_ARGS="-L C:/Users/C19257/Desktop/Code/pp-qemu/pc-bios"
rc_all=0
for t in "${tests[@]}"; do
    echo "=== $t"
    start=$(date +%s)
    timeout 600 "./tests/qtest/$t.exe" --tap -k > "$D/qtest-$t.log" 2>&1
    rc=$?
    [ $rc -ne 0 ] && rc_all=$rc
    grep -E "^(ok|not ok)|^# (Start|Subtest)|Bail|ERROR|assert" "$D/qtest-$t.log" | grep -v "^# Start" | head -60
    echo "exit=$rc elapsed=$(( $(date +%s) - start ))s pass=$(grep -c '^ok' "$D/qtest-$t.log") fail=$(grep -c '^not ok' "$D/qtest-$t.log")"
done
exit $rc_all
