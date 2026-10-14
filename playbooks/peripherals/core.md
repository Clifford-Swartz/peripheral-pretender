# Cortex-M0+ core and MPU

pp-qemu adds a `cortex-m0plus` CPU (`target/arm`): MIDR 0x410CC601, an 8-region PMSAv7 MPU and
unprivileged execution (CONTROL.nPRIV).

## Silicon lessons (PIC32CM MC00)

- The M0+ MPU has no TEX field. RBAR's ADDR is bits [31:8]. Both differ from v7-M; see
  `hw/intc/armv7m_nvic.c` (the v6-M RASR/RBAR masks).
- Subregion disables needed a TLB fix: sub-page MPU regions must not be cached as whole pages.
- MPU_TYPE.DREGION reads 8. (MPLAB's simulator complains that it reads 0.)

## Other cores

A board with a different core (M4F, M7, M23, M33) needs its own CPU check. The matrix is
`docs/research/core-support-matrix.md`. Non-Arm families (8-bit PIC, AVR, PIC32MX/MZ MIPS) need
a QEMU target first; QEMU has AVR (classic cores only) and MIPS.
