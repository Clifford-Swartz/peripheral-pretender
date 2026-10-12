# tools component progress log

## Status
- [x] read contract + research
- [x] tests/assets/mps2: startup.c, cmsdk_uart.h, mps2.ld, exit.c (exit/exit3), echo.c, burst.c, build.py
      (py tests/assets/mps2/build.py [--force] -> tests/assets/mps2/build/*.elf). Verified by hand.
- [x] transport experiment (scratch transport_experiment*.py, results in transport_experiment.out)
- [x] tools/pp runner: qemu.py (find/env/machines), protocol.py (ExitScanner), unimp.py (LogSummary),
      runner.py (run/RunOptions/RunResult, TCP client chardev), cli.py (run|machines), __main__.py.
      Manually verified: exit 0/3, echo --input exit 5, timeout 124, bad machine 125, burst exact,
      --json, --trace, ELF missing 2, QEMU missing 127.
- [x] tools/pp_mcp server (server.py FastMCP: twin_run(+machine), twin_machines), smoke-tested
- [x] tests: tests/conftest.py, tests/runner/test_pp_{protocol,unimp,qemu,cli,mps2_e2e}.py,
      tests/mcp_server/test_mcp_server.py, tests/e2e/test_hello_twin.py.
      82 passed, 2 skipped (no pic32cm machine yet); 5x repeat + 4x parallel stable.
      cmd: $env:PP_QEMU=<scratch qemu>; py -m pytest tests/runner tests/mcp_server tests/e2e -q -rs
- [x] tools/README.md, tools/vscode-integration.md

## Transport decision (measured on this host, 3 runs each, mps2-an385 CMSDK UART)
- stdio pipes: 148901-byte burst OK; input 0/102 lines (bytes dropped when guest RX full,
  '\r' stripped: chardev/char-win-stdio.c win_stdio_thread_wait_func).
- TCP QEMU server (server=on,wait=on) + Python connect: burst OK, connect 0.5 s (retry loop), port race.
- TCP QEMU client -> Python listener on port 0: burst OK (1.8 s for 149 KB), connect 0.065 s,
  input 102/102 when sent after RX enabled; input sent before RX enable is STUCK (not lost):
  win32 FD_READ fires once while can_read==0 and CMSDK never calls qemu_chr_fe_accept_input on RX enable.
  Gating input on first UART output byte: 10/10 OK.
- => runner uses TCP client mode; input sent after first output byte (or --input-after TEXT),
  fallback after 2 s of silence. SERCOM model must call qemu_chr_fe_accept_input() on RX enable.

## Commands that work
- PowerShell: py tests/assets/mps2/build.py --force
- QEMU copy for tests: scratchpad/tools/qemu/qemu-system-arm.exe (PP_QEMU)
- Bash: relative -serial file: paths only (Git Bash doesn't translate paths inside option values)

- [x] --gdb: runner keeps following the UART after PP:EXIT until QEMU exits (debugger kill);
      test_gdb_session_outlives_the_exit_line drives QEMU's gdbstub with a minimal RSP client.
      (Earlier failure was the test: UART chunks are 1-5 bytes, accumulate before matching.)

- [x] find_qemu accepts a pp-qemu checkout/build dir in PP_QEMU/--qemu.
- [x] LogSummary caches parsed lines (1M repeated lines 3.9 s -> 0.73 s); samples are distinct.
- [x] lint: pyflakes + pycodestyle (79 cols) clean (installed --user).
- [x] final: 85 passed, 2 skipped (pic32cm machine missing); 3x parallel stable.

## Open issues found (report in handoff)
- PP_QEMU conflict: tests/generator/conftest.py reads PP_QEMU as the pp-qemu ROOT; runner + harness
  read it as the BINARY. With PP_QEMU set to a binary/dir, tests/generator/test_compile.py
  (3 tests) fail. Generator should rename (e.g. PP_QEMU_SRC).
- pp-qemu has no mchp regfile/SoC yet (2026-10-12): hello e2e skipped; LOG_UNIMP format unverified.

## Status: DONE (StructuredOutput next)
