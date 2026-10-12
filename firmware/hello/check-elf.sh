#!/bin/sh
# Static checks on the hello image: vector table at 0x0, initial SP, reset
# and IRQ13 (SERCOM4) vectors, an endless-loop default handler, and no BKPT,
# SVC or semihosting code.  QEMU aborts on lockup (a BKPT in a fault
# handler) and BKPT 0xAB faults on silicon without a debugger
# (docs/design/v0-vertical-slice.md 5.6, 5.7).
#
# Usage: check-elf.sh [ELF] [MAP]; CROSS selects the toolchain prefix.
#
# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

set -eu

elf=${1:-build/hello.elf}
map=${2:-build/hello.map}
cross=${CROSS:-arm-none-eabi-}
failures=0
image=$(mktemp)
trap 'rm -f "$image"' EXIT

pass() { echo "ok   $*"; }
fail() { echo "FAIL $*"; failures=$((failures + 1)); }
hex() { printf '0x%08X' "$1"; }

# Value of a strong symbol as a decimal number, or -1 when there is none.
# Weak aliases are skipped: the DFP's default handlers are weak aliases of
# Dummy_Handler, so a missing SERCOM4_Handler must not look like a match.
sym() {
    "${cross}nm" "$elf" |
        awk -v s="$1" '$3 == s && $2 !~ /^[UVvWw]$/ { v = $1 }
                       END { print v == "" ? -1 : v }' |
        { read -r v; [ "$v" = -1 ] && echo -1 || echo $((0x$v)); }
}

# Little-endian 32-bit word at a flash address (the image starts at 0x0).
word() {
    od -A n -t u1 -j "$1" -N 4 "$image" |
        awk '{ print $1 + 256 * $2 + 65536 * $3 + 16777216 * $4 }'
}

check_word() {      # description address expected-value
    got=$(word "$2")
    if [ "$3" -lt 0 ]; then
        fail "$1: symbol not defined"
    elif [ "$got" -eq "$3" ]; then
        pass "$1 = $(hex "$got")"
    else
        fail "$1 = $(hex "$got"), expected $(hex "$3")"
    fi
}

check_vector() {    # exception-number handler
    addr=$(sym "$2")
    [ "$addr" -lt 0 ] || addr=$((addr | 1))     # Thumb bit
    check_word "vector $1 -> $2" $(($1 * 4)) "$addr"
}

"${cross}objcopy" -O binary -j .text "$elf" "$image"

if [ "$(sym exception_table)" -eq 0 ]; then
    pass "exception_table at 0x00000000"
else
    fail "exception_table is not at 0x00000000"
fi
if grep -Eq '^ \.vectors +0x0+ ' "$map"; then
    pass ".vectors placed at 0x00000000 in $map"
else
    fail ".vectors not at 0x00000000 in $map"
fi

check_word "vector 0 (initial SP) = _estack" 0 "$(sym _estack)"
check_vector 1 Reset_Handler
check_vector 3 Dummy_Handler            # HardFault: the DFP default
check_vector 15 Dummy_Handler           # SysTick: polled, never enabled
check_vector $((16 + 13)) SERCOM4_Handler

entry=$("${cross}readelf" -h "$elf" | awk '/Entry point/ { print $4 }')
if [ $((entry | 1)) -eq $(($(sym Reset_Handler) | 1)) ]; then
    pass "ELF entry $entry is Reset_Handler"
else
    fail "ELF entry $entry is not Reset_Handler"
fi

# B to itself (0xE7FE); a BKPT here would make QEMU cpu_abort on lockup.
dummy=$(sym Dummy_Handler)
if [ "$dummy" -ge 0 ] &&
        [ $(($(word "$dummy") & 0xFFFF)) -eq $((0xE7FE)) ]; then
    pass "Dummy_Handler is an endless loop (b.n .)"
else
    fail "Dummy_Handler does not start with b.n to itself"
fi

if "${cross}objdump" -d "$elf" |
        grep -Eq '[[:space:]](bkpt|svc)[[:space:]]'; then
    fail "BKPT or SVC instruction present"
else
    pass "no BKPT or SVC instruction"
fi
if "${cross}nm" "$elf" |
        grep -Eiq 'semihost|monitor_handles|rdimon|_swi(open|read|write)'; then
    fail "semihosting symbols present"
else
    pass "no semihosting symbols"
fi

echo "$failures failure(s)"
[ "$failures" -eq 0 ]
