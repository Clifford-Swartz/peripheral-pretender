# tools: `pp` runner and MCP server

Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

One engine, `tools.pp.run()`, boots a firmware ELF on the twin (QEMU) and returns the firmware's
exit code, the UART transcript and a summary of everything QEMU logged as unimplemented or as a
guest error (the *model backlog*). Three front ends use it: the CLI (`py -m tools.pp`), the MCP
server (`py -m tools.pp_mcp`) and the Python API. See [vscode-integration.md](vscode-integration.md)
for how the VS Code extension, CI and the AI assistant share it.

Requirements: Python 3.10+ (developed on 3.14). The runner uses only the standard library; the MCP
server needs the `mcp` package (1.28.1 tested). Run from the project root.

## Quick start

```powershell
py -m tools.pp run firmware/hello/build/hello.elf                  # live UART, verdict on stderr
py -m tools.pp run tests/assets/mps2/build/exit.elf -M mps2-an385 --json
py -m tools.pp run tests/assets/mps2/build/echo.elf -M mps2-an385 --input "hi\r\nexit 5\r\n"
py -m tools.pp machines                                             # what the QEMU build offers
py -m tools.pp_mcp                                                  # MCP server on stdio
```

## `pp run`

| option | meaning |
|---|---|
| `ELF` | firmware image, loaded with `-kernel`; must be an Arm ELF (checked: `\x7fELF` magic, `e_machine` 40), else exit 2 |
| `-M`, `--machine NAME` | QEMU machine, default `pic32cm-jh01-cpro` |
| `--qemu PATH` | QEMU binary (or a pp-qemu checkout or build directory); default `$PP_QEMU`, then `../pp-qemu/build/qemu-system-arm.exe`, then `PATH` |
| `--timeout S` | host seconds before giving up; default 10, none with `--gdb` |
| `--icount SHIFT` | deterministic virtual time: `-icount shift=SHIFT,sleep=off` (0-10, 2^SHIFT ns per instruction); see *Virtual time* |
| `--gdb [PORT]` | start paused (`-S`) with the gdbstub on `127.0.0.1:PORT` (default 1234). The run keeps going after `PP:EXIT` until the debugger kills or detaches from QEMU |
| `--trace PATTERN` | enable QEMU trace events (repeatable); matching lines are counted and listed apart from the backlog. A pattern that matches no event of the QEMU build (`-trace help`, cached per binary) gives a warning |
| `--input TEXT` | bytes for the UART; `\n \r \t \0 \\ \xNN` are expanded. Include the line ending the firmware expects |
| `--input-after TEXT` | send `--input` once TEXT has appeared on the UART (default: see *Input timing*) |
| `--log FILE` | keep QEMU's `-d` log (otherwise a temporary file); missing directories are created |
| `--json` | print one JSON object (below) and nothing else on stdout |
| `--extra-args ...` | everything after it goes to QEMU; must be last. A `-d ITEMS` in it is merged with the runner's own `-d unimp,guest_errors,invalid_mem`; a `-D FILE` becomes the log file (as `--log`, so the backlog is still parsed). `-M`/`-machine` pass through and QEMU merges them with `--machine`: `-machine sw0-pressed=on` adds a property, while a machine type there (`-M mps2-an385`, `-machine type=X,...`) replaces `--machine` and is what the result reports |

Without `--json`, stdout carries only the UART transcript (CRLF shown as LF), streamed live;
the verdict, the backlog table and, when QEMU failed, its output go to stderr:

```
pp runner test: exit
gpio read 0x00000000
PP:EXIT 0
[pp] exit 0 (PP:EXIT 0) after 0.35 s on mps2-an385
[pp] model backlog: 6 log lines (unimp 5, guest_error 1)
    count  kind         instance        register  r/w
        3  unimp        cmsdk-ahb-gpio  0x0       3r
        1  unimp        cmsdk-ahb-gpio  0x4       1w
        1  unimp        VGA             0x0       1w
        1  guest_error  CMSDK APB UART  0x20      1w
```

### Exit status

| status | when |
|---|---|
| `n` | the firmware printed `PP:EXIT n` (design §5.6). Values outside 0-255 become 255 so that `PP:EXIT 256` cannot read as success; JSON `firmware_exit` keeps the raw value |
| 124 | timeout before `PP:EXIT` (as GNU `timeout`); the partial transcript is kept |
| 125 | QEMU ended before `PP:EXIT` (bad machine, crash, gdb kill, CPU lockup); see `detail` and `qemu_stderr_tail` |
| 2 | usage error (missing ELF, not an Arm ELF, bad option value) |
| 127 | QEMU binary not found |
| 130 | interrupted (Ctrl+C) |

`status` tells the 125 cases apart: `cpu-lockup` when QEMU aborted with `qemu: fatal: Lockup: ...`
(a fault QEMU cannot escalate, e.g. a fault in HardFault or with FAULTMASK set; QEMU does not model
lockup and exits with 0xC0000409 on Windows), else `qemu-exited`. `detail` then reads, for example,
`CPU lockup (can't escalate 3 to HardFault (current priority -1)); CPU in thread mode at PC=0x00000092`
or `QEMU exit status 0xC0000135 (STATUS_DLL_NOT_FOUND)` (Windows NTSTATUS values are shown in hex).
On timeout the runner asks QEMU where the CPU is before stopping it, over a QMP socket it opens for
that (`-qmp tcp:127.0.0.1:<port>`, HMP `info registers`, 1 s limit; reads CPU state only), and
`detail` reads e.g. `timeout: CPU in HardFault (IPSR=3) at PC=0x000002D0`.

An exit line is `PP:EXIT <decimal>` at the end of a line (text before it is allowed). It counts once
CR or LF ends it. An unterminated one is accepted after 0.2 s of UART silence. After the exit line
the runner keeps reading for 50 ms (to collect the LF of a CRLF), then stops QEMU, unless `--gdb`
is set.

### JSON result

`py -m tools.pp run --json` and MCP `twin_run` use the same field names (`twin_run` returns the
fields from `exit_code` to `elf`):

| field | content |
|---|---|
| `exit_code` | process exit status as in the table above |
| `status` | `exit`, `timeout`, `qemu-exited` or `cpu-lockup` |
| `firmware_exit` | raw `PP:EXIT` value, or null |
| `uart` | transcript; UTF-8, undecodable bytes as `\xNN`, CRLF → LF |
| `unimplemented_summary` | model backlog, below |
| `qemu_stderr_tail` | last 40 lines of QEMU's stdout+stderr |
| `duration_s` | host seconds from launch to stop |
| `machine`, `elf` | what ran |
| `trace` | with `--trace`: `{count, events: {name: n}, lines (first 10000), truncated}`; else null |
| `qemu_returncode` | QEMU's own exit status, or null when the runner stopped it |
| `command` | the QEMU command line (the UART port is ephemeral) |
| `log_path` | the kept log with `--log` (or `-D` in `--extra-args`), else null |
| `detail` | one line on why the run ended (lockup, QEMU exit status, CPU location on timeout), or null |
| `warnings` | e.g. a `--trace` pattern that matches no event |

## Model backlog (`unimplemented_summary`)

QEMU always runs with `-d unimp,guest_errors,invalid_mem -D <log>` (`invalid_mem`, QEMU 9.1+, carries
the bus faults to unmapped addresses). Each log line becomes a `(kind, instance, register)` group with
`count`, `reads`, `writes`, up to three distinct `samples` and `truncated`.
Groups are sorted by count. `counts` totals per kind and `instances` per instance.

```json
{"total": 6, "counts": {"unimp": 5, "guest_error": 1, "other": 0},
 "instances": {"cmsdk-ahb-gpio": 4, "VGA": 1, "CMSDK APB UART": 1},
 "groups": [{"kind": "unimp", "instance": "cmsdk-ahb-gpio", "register": "0x0", "count": 3,
             "reads": 3, "writes": 0, "samples": ["cmsdk-ahb-gpio: unimplemented device read  (size 4, offset 0x000)"],
             "truncated": false},
            "..."]}
```

Recognised formats (`tools/pp/unimp.py`):

| source | example | instance / register |
|---|---|---|
| `hw/misc/unimp.c` (`create_unimplemented_device`) | `VGA: unimplemented device write (size 4, offset 0x000000, value 0x41)` | `VGA` / `0x0` (offset in the device) |
| `system/memory.c`, `physmem.c` | `Invalid read at addr 0x41000000, size 4, region '(null)', reason: rejected` | region or `(unassigned)` / `@0x41000000` |
| QEMU device convention `<instance>[ read\|write]: <message>` | `CMSDK APB UART write: bad offset 0x20` | `CMSDK APB UART` / `0x20` |
| `instance.REGISTER` anywhere in the line | `mchp_regblock: tc0.CTRLA write 0x2 not implemented` | `tc0` / `CTRLA` |
| mchp generic register block, after 8 writes | `tc0.CTRLBSET: further writes not logged` | marks `tc0` / `CTRLBSET` `truncated`; not counted |
| `cpu_abort` | `qemu: fatal: Lockup: ...` | ends the backlog: the register dump after it is ignored |
| anything else | | `(unparsed)` / `-`, kind `other` |

**Write counts are capped.** The mchp generic register block logs at most 8 unmodelled writes per
register, then one `further writes not logged` line, so `writes` (and `count`) for such a register is
a lower bound; the group has `truncated: true` and the CLI table shows `8w+`. NUL bytes in the log
are dropped (a log cut short by killing QEMU on Windows can end in a long run of them).

In a free-form message, the register is the first `offset 0x…`, else `addr 0x…`, else the first
upper-case identifier (`PCHCTRL[22]`, `CTRLA`), else `-`. The log does not record which `-d` mask
produced a line, so the unimp/guest-error split for free-form messages comes from the wording:
"unimplemented", "not implemented", "unimp", "not modelled/modeled", "unsupported", "not supported"
mean `unimp`.

**Convention for the mchp models** (design §5.7: always name instance and register). Start with the
lower-case ATDF instance name, name the register in upper case (with `[n]` for array elements),
and use one of the words above for `LOG_UNIMP`, for example:

```c
qemu_log_mask(LOG_UNIMP, "%s: %s write 0x%08x not modelled\n", rf->name, ri->name, val);
/* -> "sercom4: CTRLB write 0x00030000 not modelled" -> unimp / sercom4 / CTRLB */
qemu_log_mask(LOG_GUEST_ERROR, "%s: %s[%u] enable-protected bits written while enabled\n", ...);
/* -> "gclk: PCHCTRL[22] enable-protected ..."        -> guest_error / gclk / PCHCTRL[22] */
```

Trace output (`--trace`) shares the log file; a line is a trace line when its first word is the
name of an event matching an enabled pattern. Patterns given through `--extra-args -trace X` count
too; `-trace events=FILE` does not.

## UART transport (measured on this Windows host)

The runner listens on an ephemeral loopback port and QEMU connects to it as a client chardev:
`-chardev socket,id=pp_uart0,host=127.0.0.1,port=<port> -serial chardev:pp_uart0`. Results for the
mps2-an385 CMSDK UART, 3 runs each: a 148,901-byte burst, and 100 echoed input lines plus `exit 7`
(102 expected lines):

| transport | output burst | input sent early | input after the prompt | connect |
|---|---|---|---|---|
| `-serial stdio` + pipes | byte-exact | 0/102 lines | 0/102 lines (garbled) | n/a |
| TCP, QEMU server (`server=on,wait=on`), Python connects | byte-exact | stuck | 102/102 | 0.5 s (retry loop; free-port race) |
| **TCP, QEMU client, Python listens on port 0** | byte-exact | stuck | 102/102 | 0.065 s |

`-serial stdio` loses input because the Windows stdio chardev reads a pipe one byte at a time,
drops a byte whenever the guest UART cannot take it, and strips every CR
(`chardev/char-win-stdio.c`, `win_stdio_thread*`). TCP gives the guest backpressure: QEMU reads the
socket only when the UART can accept. Of the two TCP variants, the client one has no free-port race
and no connect retry loop. Output costs about 12 µs per byte (CMSDK writes one byte per send), about
1.8 s for 149 KB, which is irrelevant for normal transcripts.

**Input is never lost, unlike on silicon.** TCP backpressure holds every byte until the UART
model takes it: input sent before the firmware enables its receiver (RXEN), or faster than it reads
DATA, waits instead of being dropped or raising an overrun. A firmware that only works because the
twin buffers its input can fail on the board; test overrun handling on silicon.

**Input timing.** Bytes that reach QEMU before the guest enables its receiver stay queued until the
UART model calls `qemu_chr_fe_accept_input()`. On Windows the socket's FD_READ notification fires
once and is not re-armed until QEMU reads, so a model that does not call it on receiver enable (the
CMSDK UART does not) never sees them ("stuck" above). The runner therefore writes `--input` once the
firmware has printed its first byte (10/10 runs), once `--input-after TEXT` has appeared, or after
2 s of silence. **SERCOM model requirement:** call `qemu_chr_fe_accept_input()` when the receiver
becomes enabled (CTRLB.RXEN / CTRLA.ENABLE) and after each DATA read, so that input sent at any time
is delivered.

## Virtual time (`--icount`)

Without `--icount` the guest runs as fast as the host lets it: busy loops run about 50 times faster
than on a 48 MHz Cortex-M0+, and timing depends on host load. `--icount SHIFT` makes each instruction
take 2^SHIFT ns of virtual time, so timers and delays become deterministic, but only for runs that
do not read the UART: input still arrives in host time, so the instruction at which it lands varies.
For the 48 MHz PIC32CM (20.8 ns per cycle) use shift 5 (32 ns per instruction, about 1.5 cycles,
a typical Cortex-M0+ average) to approximate silicon timing, or 4 (16 ns) for slightly faster runs;
the ratio is approximate because QEMU does not count cycles. Measured with `hello.elf` on this host: shift 0 took 57.7 s, 2 took 14.4 s,
4 took 3.7 s and 7 took 0.55 s, so small shifts make delay-heavy firmware slow in host time.

## MCP server

`py -m tools.pp_mcp` speaks MCP over stdio (FastMCP from the `mcp` SDK) and never writes to stdout
itself. Tools:

- `twin_run(elf_path, timeout_s=10, input_text=None, icount_shift=None, machine="pic32cm-jh01-cpro",
  machine_options=None, input_after=None, trace=None)` returns the JSON fields above from
  `exit_code` to `elf`, plus `detail`, `trace` and `warnings`. `machine` is an addition to design §6.3
  (needed to run the mps2 smoke images); `machine_options` adds properties (`"sw0-pressed=on"`,
  passed as `-machine`; a machine type there is refused); `input_after` and `trace` (list of
  patterns) are the CLI's `--input-after` and `--trace`. `uart` is cut to its first and last 32 K
  characters around a `[... N characters omitted ...]` line when longer than 64 K; `uart_truncated`
  and `uart_length` (the full length) say so. `trace.lines` keeps the first 200 lines. Relative
  `elf_path` is tried against the server's working directory, then the project root. `timeout_s` is
  limited to 600. Errors (missing or non-ELF file, no QEMU) come back as tool errors.
- `twin_machines()` returns `{qemu, qemu_version, default_machine, default_machine_available, machines}`.

Register it with a client from the project root, e.g. Claude Code
`claude mcp add peripheral-pretender -- py -m tools.pp_mcp`, or see
[vscode-integration.md](vscode-integration.md) for VS Code. A client that starts the server in
another directory must set `PYTHONPATH` to the project root.

## Environment

| variable | use |
|---|---|
| `PP_QEMU` | QEMU binary, or a pp-qemu checkout/build directory containing it |
| `PP_MSYS2_BIN` | directory with QEMU's DLLs, prepended to `PATH` on Windows (default `C:/msys64/ucrt64/bin`). Without it QEMU exits with 0xC0000135 and the runner says so |
| `PP_ARM_GCC` | `arm-none-eabi-gcc` for the test images (default: `PATH`, then MSYS2 UCRT64) |

## Tests

```powershell
$env:PP_QEMU = "<copy of qemu-system-arm.exe>"   # optional; avoids locking the build output
py -m pytest tests/runner tests/mcp_server tests/e2e -rs
```

- `tests/runner/`: unit tests (exit protocol, log parsing, CLI, QEMU discovery) and
  `test_pp_mps2_e2e.py`, which runs the real QEMU on `mps2-an385`. It covers exit codes, input,
  timeout (with the CPU location), a byte-exact 2000-line burst, an unknown machine, trace,
  `--icount`, a bus fault to an unmapped address, a CPU lockup, `-D`/`-M` in the extra args, the CLI
  as a subprocess, and a `--gdb` session driven over the GDB remote protocol.
- `tests/mcp_server/`: starts `py -m tools.pp_mcp` with the SDK's stdio client, lists the tools and
  calls `twin_machines` and `twin_run` on the mps2 images.
- `tests/e2e/test_hello_twin.py`: `firmware/hello/build/hello.elf` on `pic32cm-jh01-cpro`, through
  the API and the CLI. It skips, with the reason, until both the machine and the ELF exist.
- Test images: `tests/assets/mps2/` (`exit.c`, built as `exit` and `exit3`, `fault.c`, built as
  `fault` and `lockup`, plus `echo.c`, `burst.c`, `startup.c`, `cmsdk_uart.h`, `mps2.ld`). `py tests/assets/mps2/build.py [--force]`
  builds them into `tests/assets/mps2/build/` (git-ignored); the fixtures do this on demand and
  skip without a toolchain. They print `PP:EXIT` instead of using semihosting.
- Tests that launch QEMU carry the `qemu` marker (`-m "not qemu"` runs only the fast ones).

## Limits and follow-ups

- If the runner process is killed hard (not Ctrl+C), QEMU can outlive it. A Windows job object
  would fix that; the MCP SDK client already puts the server in one.
- Only `serial_hd(0)` is connected. The board's other UARTs (EXT1-3, LIN) get no backend; a
  `--uart N` option can add more links.
- Input is all sent at once; expect-style dialogues (wait for prompt A, send B, ...) would need
  a list of (wait-for, send) pairs.
- The QMP channel is only used for the CPU location on timeout. It could also carry `qom-set`
  (e.g. board property `sw0-pressed` during a run) and a clean `quit`. Today QEMU is killed, which
  is safe because QEMU flushes the log after every message (`util/log.c` `qemu_log_unlock`).
