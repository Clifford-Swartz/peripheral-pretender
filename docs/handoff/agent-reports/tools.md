# tools (spec sections 5.6, 6.3): pp runner CLI, MCP server, tests — done

The runner (`py -m tools.pp run|machines`), the MCP server (`py -m tools.pp_mcp`, tools `twin_run` and `twin_machines`), the tests and the docs are written and pass against real QEMU on mps2-an385: 85 passed, 2 skipped. The 2 skips are the PIC32CM end-to-end tests, because pp-qemu does not have the pic32cm-jh01-cpro machine yet (still true on 2026-10-12). So `pp run firmware/hello/build/hello.elf` has not run on the twin yet; the hello ELF itself is built.

UART transport: I tested both options on this Windows host, 3 runs each, with a 148,901-byte output burst and 100 echoed input lines.
- `-serial stdio` with pipes passes output byte-exact but echoed 0/102 input lines (the stdio input path on Windows drops a byte whenever the guest UART can't take it, and strips every CR).
- TCP with QEMU as the server works but needs a free-port guess and a 0.5 s connect retry loop.
- TCP where the runner listens on port 0 and QEMU connects as a client was byte-exact both ways (102/102) and connects in 0.065 s. That is what the runner uses.

Input is sent once the firmware prints its first byte (10/10 runs), or after `--input-after TEXT` appears, or after 2 s of silence. Bytes that arrive before the guest enables its receiver stay stuck unless the UART model calls `qemu_chr_fe_accept_input()`, and the CMSDK model does not.

Exit codes: n from `PP:EXIT n` (values outside 0-255 become 255), 124 timeout (transcript kept), 125 QEMU ended first, 2 usage error, 127 QEMU missing. `--json` and `twin_run` return the same fields: exit_code, status, firmware_exit, uart, unimplemented_summary, qemu_stderr_tail, duration_s, machine, elf.

The runner always passes `-d unimp,guest_errors -D <log>` and summarises the log into a model backlog grouped by kind, instance and register, with counts, reads/writes and up to 3 distinct sample lines. It recognises QEMU's unimplemented-device lines, invalid-memory lines, the usual `instance: message` form and `instance.REGISTER`. Trace lines (`--trace`) are counted separately. `--gdb` starts QEMU paused and keeps it running after `PP:EXIT` until the debugger ends the session.

Test images under tests/assets/mps2 print `PP:EXIT` (no semihosting): exit (0 and 3), a line echo, and a 2000-line burst. tools/README.md records the transport measurements; tools/vscode-integration.md covers the VS Code extension, CI and MCP, with tasks.json and launch.json snippets (cppdbg and gdb-multiarch, plus a Cortex-Debug variant) that have not been run in VS Code.

## Handoff
State: done for v0 except the PIC32CM end-to-end run, which waits for the machine.

Run the tests from the project root (PowerShell):
  py -m pytest tests/runner tests/mcp_server tests/e2e -rs
Optionally set $env:PP_QEMU to a copy of qemu-system-arm.exe, or to a directory containing it, so the build output isn't locked while others relink it. Test images build automatically; to build them by hand: py tests/assets/mps2/build.py --force.

When pic32cm-jh01-cpro lands in pp-qemu/build:
  py -m tools.pp run firmware/hello/build/hello.elf
  py -m pytest tests/e2e -rs -s
The -s shows the transcript and the backlog. Expect exit 0; the backlog lists the registers the regblocks don't model yet. If the hello run times out with input involved, check the SERCOM requirement below.

For the core QEMU (part B / SERCOM) agent:
(1) mchp-sercom must call qemu_chr_fe_accept_input() when the receiver becomes enabled (CTRLB.RXEN / CTRLA.ENABLE) and after each DATA read. Otherwise input that arrives before RX is enabled stays stuck: on Windows the socket's FD_READ fires once and isn't re-armed until QEMU reads.
(2) For LOG_UNIMP and LOG_GUEST_ERROR messages, use "<lowercase instance>: <REGISTER>[n] ... not modelled" (or the instance.REGISTER form), so unimp.py groups by instance and register. Use one of the words unimplemented / not implemented / not modelled / unsupported for LOG_UNIMP; the log doesn't record which -d mask produced a line. Examples are in tools/README.md, section "Model backlog". After the regfile lands, add a few real message lines to tests/runner/test_pp_unimp.py::test_prefixed_messages.

Code map:
- tools.pp.run(RunOptions) -> RunResult in tools/pp/runner.py. _UartLink is the TCP listener, _Pump the read/send loop (run / follow / _drain), _stop stops QEMU.
- Exit-line detection: ExitScanner in tools/pp/protocol.py.
- Backlog: parse_line and LogSummary in tools/pp/unimp.py.
- QEMU discovery: find_qemu and qemu_env in tools/pp/qemu.py.
- MCP tools: twin_run and twin_machines in tools/pp_mcp/server.py.
- Constants: EXIT_TIMEOUT=124, EXIT_NO_EXIT_LINE=125, SILENT_INPUT_S=2.0, QUIET_EXIT_S=0.2, DRAIN_S=0.05.

The transport experiment scripts are in the scratchpad: tools/transport_experiment*.py, gdb_debug*.py, mcp_smoke.py. Progress log: scratchpad/tools/progress.md.

## Tests
- [pass] tests/runner unit tests (protocol, unimp, qemu, cli): Part of 85 passed; no QEMU needed. Also pyflakes and pycodestyle (79 columns) are clean on tools/ and my tests.
- [pass] tests/runner/test_pp_mps2_e2e.py (real QEMU, mps2-an385): 14 tests: exit 0 with backlog rows [unimp cmsdk-ahb-gpio 0x0 x3, unimp cmsdk-ahb-gpio 0x4, unimp VGA 0x0, guest_error CMSDK APB UART 0x20]; exit 3; echo with input -> 5; 100-line input-after; timeout -> 124 with partial transcript; byte-exact 146,900-char burst; unknown machine -> 125 with QEMU's error; trace (55 cmsdk_apb_uart_tx events); --log; --icount 4; CLI subprocess stream/json/timeout; gdb session continue -> PP:EXIT -> QEMU stays up -> ^C + kill -> returncode 0.
- [pass] tests/mcp_server/test_mcp_server.py: Starts py -m tools.pp_mcp with the mcp 1.28.1 stdio client; both tools listed with input and output schemas; twin_machines lists 95 machines; twin_run exit.elf -> exit_code 0 with backlog total 6; echo.elf with input_text -> 9; missing ELF -> isError 'ELF not found'.
- [skip] tests/e2e/test_hello_twin.py (hello.elf on pic32cm-jh01-cpro): Both tests skip with: 'QEMU build has no pic32cm-jh01-cpro machine yet' (pp-qemu/build/qemu-system-arm.exe is still the 2026-10-09 08:52 build; no regfile/SoC sources in pp-qemu as of 2026-10-12). firmware/hello/build/hello.elf exists.
- [pass] Stability: whole tools suite repeated and run in parallel: 5 sequential runs of the e2e + MCP suites, then 4 and later 3 concurrent runs of the full tools suites: all '85 passed, 2 skipped' (or 14 passed for the e2e subset).
- [pass] Transport experiment (stdio vs TCP server vs TCP client), scratch scripts: stdio: burst OK, input 0/102 (garbled). tcp-server: burst OK, input-after-prompt 102/102, connect 0.5 s. tcp-client: burst OK, 102/102, connect 0.065 s. Early input stuck for both TCP variants (CMSDK model); sending after the first output byte 10/10. Raw results: scratchpad/tools/transport_experiment.out.
- [pass] Missing-DLL hint (PATH without MSYS2, PP_MSYS2_BIN invalid): QEMU exits with 3221225781 (0xC0000135); runner returns 125 and prints '[pp] QEMU could not load its DLLs; set PP_MSYS2_BIN ...'.
- [fail] Combined run: py -m pytest tests (including tests/generator): With PP_QEMU set the way the runner and harness use it, 3 generator tests fail: tests/generator/test_compile.py::test_device_types_header_exists, ::test_each_header_compiles_alone, ::test_all_headers_together (AssertionError at test_compile.py:95), because tests/generator/conftest.py reads PP_QEMU as the pp-qemu source root. Without PP_QEMU the generator suite passes on its own (82 passed). Not my code; see open_issues.

## Deviations
- MCP twin_run has an extra optional parameter machine (default pic32cm-jh01-cpro), so the required MCP smoke test can run the mps2 image. The section 6.3 parameters are unchanged; it also returns status, firmware_exit, machine and elf on top of the specified fields.
- CLI additions beyond section 6.3: a 'machines' subcommand, -M as short for --machine, --input-after TEXT, --log FILE. --gdb takes an optional PORT (default 1234), turns off the default timeout, and keeps QEMU running after PP:EXIT until the debugger ends the session.
- --input bytes are not sent at QEMU start. They go out after the first UART output, or after --input-after TEXT appears, or after 2 s of silence, because UART models without accept_input on RX enable never see earlier bytes (measured).
- PP_QEMU and --qemu also accept a pp-qemu checkout or build directory, not only the binary path.
- The default QEMU path is computed as <project>/../pp-qemu/build/qemu-system-arm.exe. That equals the path given in the spec on this machine and also works in other checkouts.
- I created tests/conftest.py at the tests root. It adds the project root to sys.path, registers the 'qemu' marker and defines non-autouse fixtures. It sits above tests/generator, but there are no fixture-name clashes with tests/generator/conftest.py.
- Installed pyflakes 4.0.3 and pycodestyle 2.15.0 with py -m pip install --user, for linting only. No runtime dependency on them.

## Open issues
- PP_QEMU means two different things: tests/generator/conftest.py treats it as the pp-qemu source root, while tools/pp, harness/twin.py and the orchestrator brief treat it as the QEMU binary. With PP_QEMU set to a binary or binary directory, tests/generator/test_compile.py (3 tests) fail with AssertionError at line 95. Suggest the generator agent renames theirs to PP_QEMU_SRC (or similar). I did not edit their file.
- The pic32cm-jh01-cpro machine does not exist yet: pp-qemu/build/qemu-system-arm.exe is from 2026-10-09 08:52 and has no mchp regfile/regblock/SoC sources as of 2026-10-12. So the section 6.6 acceptance check ('py -m tools.pp run firmware/hello/build/hello.elf returns 0') is not verified, and the mchp LOG_UNIMP message format is not verified against the parser.
- mchp-sercom has to call qemu_chr_fe_accept_input() on receiver enable and after DATA reads. The runner works around models that don't (input waits for the first output), but firmware that enables RX after printing, or a gdb session paused before RX enable, would still hit it.
- If the runner process is killed hard (not Ctrl+C), QEMU can outlive it on Windows. A job object would fix this; the MCP SDK client already puts the server in one.
- Only serial 0 (SERCOM4 VCOM) is connected; the board's other UARTs get no backend.
- The tasks.json, launch.json and mcp.json snippets in tools/vscode-integration.md are written from the documented schemas and have not been run in VS Code.

## Commands
- Build the mps2 runner test images: `py tests/assets/mps2/build.py --force`
- Run an ELF on the twin with live UART and backlog (default machine pic32cm-jh01-cpro): `py -m tools.pp run firmware/hello/build/hello.elf`
- Run an mps2 test image with JSON output: `py -m tools.pp run tests/assets/mps2/build/exit.elf -M mps2-an385 --json`
- Send UART input: `py -m tools.pp run tests/assets/mps2/build/echo.elf -M mps2-an385 --input "hi\r\nexit 5\r\n"`
- Start paused for gdb-multiarch on 127.0.0.1:1234: `py -m tools.pp run firmware/hello/build/hello.elf --gdb`
- List machines of the QEMU build: `py -m tools.pp machines --json`
- Start the MCP server (stdio): `py -m tools.pp_mcp`
- Run the tools tests (PowerShell; optional PP_QEMU = copy of the binary or its directory): `$env:PP_QEMU = 'C:/Users/C19257/AppData/Local/Temp/claude/c--Users-C19257-Desktop-Code-Peripheral-Pretender/35dcd6cf-c2f4-468f-97c2-8e7f0ea8c116/scratchpad/tools/qemu'; py -m pytest tests/runner tests/mcp_server tests/e2e -rs`
- Only the fast tests (no QEMU): `py -m pytest tests/runner tests/mcp_server -m "not qemu"`
- Lint: `py -m pyflakes tools tests/conftest.py tests/runner tests/mcp_server tests/e2e tests/assets/mps2/build.py; py -m pycodestyle --max-line-length=79 tools tests/conftest.py tests/runner tests/mcp_server tests/e2e tests/assets/mps2/build.py`

## Files
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/__init__.py: Package marker for the host tools
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/__init__.py: Public API: run, RunOptions, RunResult, find_qemu, machines_info, summarize_log, exit-code constants
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/__main__.py: Entry point for py -m tools.pp
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/cli.py: CLI: run (live transcript on stdout, verdict and backlog on stderr, --json) and machines; --input escape handling; exit statuses
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/runner.py: Engine: builds the QEMU command, runs the loopback TCP UART link, reads until PP:EXIT/timeout/QEMU exit, gated input, gdb follow mode, stops QEMU, summarises the log
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/protocol.py: ExitScanner: finds PP:EXIT n in a chunked byte stream (CR/LF/CRLF, split codes, unterminated pending); exit-status mapping
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/unimp.py: Model-backlog summariser for -d unimp,guest_errors logs (format regexes, grouping, trace digest, cache for repeated lines)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp/qemu.py: Finds QEMU (--qemu, PP_QEMU binary or directory, sibling pp-qemu build, PATH), adds MSYS2 DLL dir to PATH (PP_MSYS2_BIN), parses -M help, machines_info
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp_mcp/__init__.py: MCP package marker
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp_mcp/__main__.py: Entry point for py -m tools.pp_mcp
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/pp_mcp/server.py: FastMCP stdio server (mcp 1.28.1): twin_run (TypedDict output schema, runs in a worker thread) and twin_machines
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/README.md: Usage, options, exit codes, JSON fields, backlog format and recommended LOG_UNIMP message convention, transport measurements and reasoning, MCP setup, environment, tests, limits
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tools/vscode-integration.md: One page: how the VS Code extension, CI and the AI assistant use the same engine; tasks.json, launch.json (cppdbg + gdb-multiarch, Cortex-Debug variant), mcp.json
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/conftest.py: Puts the project root on sys.path, registers the qemu marker, session fixtures qemu_path/qemu_machines/mps2_images (build on demand, skip with reason)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/runner/test_pp_protocol.py: Unit tests for PP:EXIT detection and exit-status mapping
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/runner/test_pp_unimp.py: Unit tests for log line classification, grouping, distinct samples, trace digest, CRLF log files
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/runner/test_pp_qemu.py: Unit tests for QEMU discovery order, directory handling, -M help parsing, MSYS2 PATH
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/runner/test_pp_cli.py: Unit tests for argument parsing, input escapes, QEMU command build, live echo, report formatting, CLI exit statuses (fake runner)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/runner/test_pp_mps2_e2e.py: End-to-end tests on real QEMU mps2-an385: exit 0/3 + backlog, input echo, input-after, timeout 124, byte-exact burst, unknown machine 125, trace + kept log, icount, CLI subprocess (stream/json/timeout), gdb session over a minimal remote-protocol client
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/mcp_server/test_mcp_server.py: MCP smoke test: starts py -m tools.pp_mcp via the SDK stdio client, checks tool schemas, calls twin_machines and twin_run (mps2 exit/echo, missing ELF -> isError)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/e2e/test_hello_twin.py: End-to-end: firmware/hello/build/hello.elf on pic32cm-jh01-cpro via API and CLI; skipped with a reason until the machine and the ELF both exist
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/build.py: Stdlib build script for the mps2 test images (PP_ARM_GCC, PATH, or MSYS2 gcc; rebuilds only when sources change); output in tests/assets/mps2/build/ (git-ignored)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/exit.c: Test image: prints, touches unimplemented GPIO/VGA and a bad UART offset, then PP:EXIT 0 (exit3 build: -DEXIT_CODE=3)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/echo.c: Test image: echoes UART lines; 'exit n' ends with PP:EXIT n; waits forever without input (timeout test)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/burst.c: Test image: 2000 deterministic lines for transport integrity
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/startup.c: Minimal Cortex-M3 vector table and reset handler (clears .bss)
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/cmsdk_uart.h: Polled CMSDK APB UART0 helpers and pp_exit()
- C:/Users/C19257/Desktop/Code/Peripheral Pretender/tests/assets/mps2/mps2.ld: Linker script (flash at 0x0, RAM at 0x20000000; rejects initialised .data)
