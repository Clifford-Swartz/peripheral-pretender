# firmware component progress log

## State
- [run 2] Started. Prior run left only scratchpad/firmware/harmony-ref/*.c (Harmony reference sources) and probe.sh. No firmware/ in project yet.

## Next
- read boot-path.md, microchip-data.md (NVM rows), validation-build.md
- probe toolchain (probe.sh), write firmware/hello

## Commands that work
- [run 2] Toolchain probed OK (gcc 16.1.0, make 4.4.1, v6-m nano/nosys libs present). Run via: $env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/.../script.sh
- [run 2] Created firmware/hello/{dfp,src}; copied DFP startup + flash.ld verbatim (cmp identical).
- Design: init = exact Harmony sequence (sys_init.c); report lines printed AFTER uart up by reading regs; console.c = polled TX + IRQ13 one-byte-per-interrupt print with SysTick timeout; main.c = report, blink, PP:EXIT.
- [run 2] Sources written: src/{main.c,console.[ch],sys_init.[ch]}, Makefile, check-elf.sh. Link: -nostartfiles + crti.o/crtn.o + nano.specs (default startfiles pulled crt0->exit->stdio and produced libnosys "not implemented" link warnings; nosys not needed).
- Scratch scripts: scratchpad/firmware/{build.sh,full.sh,inspect.sh,linktest.sh}; run with: $env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -l /c/.../scratchpad/firmware/full.sh
- Next: run full.sh (clean build + check), verify disasm of 64-bit cal read, write README with transcript.
- [run 2] DONE: clean build warning-free (text 2596, data 0, bss 1544); make check 11/11 ok; negtest.sh proves check-elf.sh fails on missing SERCOM4_Handler and on BKPT Dummy_Handler. Disasm of sys_initialize verified access-by-access vs boot-path table.
- Twin machine not in pp-qemu yet (no hw/arm/pic32*). Next: Unicorn-based functional mock in scratch (mock/run_mock.py) to execute ELF + generate transcript; then README.md.
- [run 2] Mock (scratchpad/firmware/mock/run_mock.py, unicorn 2.1.4 user-installed; run WITHOUT -I so user site is visible) runs ELF to PP:EXIT 0; IRQ13 handler 60 calls (=60 bytes), SysTick polls 1200, LED0 on/off x3; MMIO trace == boot-path table. Next: README.md, final report.
- [run 2] console_irq_cancel() now returns unsent remainder (FAIL line replaces irq13 line cleanly). text=2604. Mock scenarios verified: normal (EXIT 0), --no-irq (EXIT 1), --irq-limit 10 (partial+FAIL, EXIT 1), --stuck reg (annotated, EXIT 1). README.md written; transcript block diffed == mock output.
- Remaining: final review of files, StructuredOutput report.
- [run 2] FINAL: text 2604/data 0/bss 1544; make check 11/11; negtest ok; mock transcript == README. Style pass done (80 cols, no tabs, /* */). Only StructuredOutput report left.
