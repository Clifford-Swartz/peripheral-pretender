#!/usr/bin/env bash
# Incremental build of qemu-system-arm; prints compiler diagnostics only.
set -uo pipefail
cd /c/Users/C19257/Desktop/Code/pp-qemu/build
LOG=/c/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/qemu-core-a/build.log
start=$(date +%s)
ninja -k 0 "${@:-qemu-system-arm.exe}" > "$LOG" 2>&1
rc=$?
echo "NINJA_EXIT=$rc ELAPSED=$(( $(date +%s) - start ))s"
grep -v "cannot symlink" "$LOG" | grep -n -E "FAILED|error:|warning:" -A3 | head -120
tail -3 "$LOG"
