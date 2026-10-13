# One engine for VS Code, CI and the AI assistant

Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).

The design (§1) requires the engine to work headless, with nothing essential living only in the
IDE. All three delivery channels therefore call the same code, `tools.pp.run()`, through one of
two stable interfaces. Neither of them talks to QEMU directly.

```
VS Code extension ──┐  py -m tools.pp run <elf> --json        (child process, JSON on stdout)
CI job ─────────────┤  py -m tools.pp run <elf> [--json]      (exit status = verdict)
AI assistant ───────┘  MCP twin_run / twin_machines           (py -m tools.pp_mcp, stdio)
                           │
                     tools.pp.run(RunOptions) ─► qemu-system-arm -M pic32cm-jh01-cpro ...
                           ▲  UART over loopback TCP, -d unimp,guest_errors log → model backlog
```

The contract is the JSON result, which is the same for the CLI and MCP: `exit_code`, `status`,
`firmware_exit`, `uart`, `unimplemented_summary`, `qemu_stderr_tail`, `duration_s`, plus the CLI
extras described in [README.md](README.md). Exit codes: `n` from `PP:EXIT n`, 124 timeout, 125 QEMU
ended first. Register metadata for presentation (offsets, reset values, access, docs links) comes
from the generated `devices/pic32cm5164jh01100.json`, keyed by the same lower-case instance names
and upper-case register names that the backlog uses.

## VS Code extension

- **Run on twin** command or test-controller item: spawn `py -m tools.pp run <elf> --json` with
  `cwd` = workspace root and parse stdout. Show `uart` in an output channel and map `exit_code` to
  pass/fail (124 = timeout, 125 = error with `qemu_stderr_tail`). Render
  `unimplemented_summary.groups` as a tree or as Problems entries ("sercom4.CTRLB written 3x, not
  modelled"), linked to the register in the device JSON. For a live console, spawn without `--json`:
  stdout is then the raw transcript and the verdict arrives on stderr.
- **Debug on twin**: a background task starts the run paused with the gdbstub, then any GDB front end
  attaches with `gdb-multiarch`. The session keeps running after `PP:EXIT` until the debugger stops
  it. Register views read through the gdbstub, whose accesses carry `attrs.debug`, so reading SERCOM
  DATA does not pop the RX FIFO (design §5.7).

`.vscode/tasks.json`:

```jsonc
{
  "version": "2.0.0",
  "tasks": [{
    "label": "twin: start paused",
    "type": "process",
    "command": "py",
    "args": ["-m", "tools.pp", "run", "${workspaceFolder}/firmware/hello/build/pic32cm_jh01_cpro/hello.elf",
             "--gdb", "1234"],
    "options": { "cwd": "${workspaceFolder}" },
    "isBackground": true,
    "problemMatcher": {
      "owner": "pp",
      "pattern": { "regexp": "^\\[pp\\] never matches$" },
      "background": { "activeBegins": true,
                      "beginsPattern": "^\\[pp\\] QEMU is paused",
                      "endsPattern": "^\\[pp\\] QEMU is paused" }
    }
  }]
}
```

`.vscode/launch.json` (C/C++ extension, `cppdbg`):

```jsonc
{
  "version": "0.2.0",
  "configurations": [{
    "name": "hello on the twin",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/firmware/hello/build/pic32cm_jh01_cpro/hello.elf",
    "cwd": "${workspaceFolder}",
    "MIMode": "gdb",
    "miDebuggerPath": "C:/msys64/ucrt64/bin/gdb-multiarch.exe",
    "miDebuggerServerAddress": "127.0.0.1:1234",
    "targetArchitecture": "arm",
    "stopAtConnect": true,
    "preLaunchTask": "twin: start paused"
  }]
}
```

`[pp] QEMU is paused` is printed just before QEMU starts. gdb's TCP auto-retry (on by default, 15 s)
covers the gap until the gdbstub listens. With Cortex-Debug, use `"type": "cortex-debug"`,
`"request": "attach"`, `"servertype": "external"`, `"gdbTarget": "127.0.0.1:1234"`,
`"gdbPath": "C:/msys64/ucrt64/bin/gdb-multiarch.exe"` and `"executable"` set to the ELF. Check both
snippets against the installed extension versions; they have not been run in VS Code yet.

## CI

The same command is the test step. Its exit status is the verdict, and the JSON is the artifact
that dashboards and the silicon harness diff against (`harness/hw_run.py` uses the same exit codes
and `uart` field):

```sh
py -m tools.pp run firmware/hello/build/pic32cm_jh01_cpro/hello.elf --icount 4 --timeout 60 --json > twin-hello.json
```

`--icount` makes guest time deterministic (instruction-count based, idle time skipped), so timing
does not depend on the CI machine's load. The container needs Python 3.10+, the pp-qemu build
(`PP_QEMU`) and, to build images, `arm-none-eabi-gcc`. A model-backlog gate, such as failing when
`unimplemented_summary.counts.guest_error` grows, is a few lines of `jq` over the JSON.

## AI assistant (MCP)

Register the stdio server once per workspace, e.g. `.vscode/mcp.json`:

```jsonc
{
  "servers": {
    "peripheral-pretender": {
      "type": "stdio",
      "command": "py",
      "args": ["-m", "tools.pp_mcp"],
      "env": { "PYTHONPATH": "${workspaceFolder}" }
    }
  }
}
```

The assistant calls `twin_machines` to see whether the board is in the QEMU build, then
`twin_run(elf_path, timeout_s, input_text, icount_shift)` after each build. It reads `exit_code` and
`uart` to judge the firmware, and `unimplemented_summary` to tell a firmware bug from a gap in the
twin. If the backlog names a register the firmware depends on, the result shows the gap, not the
firmware's behaviour. The server's instructions say this. Microchip's assistant, Claude Code (`claude
mcp add peripheral-pretender -- py -m tools.pp_mcp`) and any other MCP client get the same tools.
