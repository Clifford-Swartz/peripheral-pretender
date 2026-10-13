# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""MCP server (stdio transport) exposing the twin runner to AI assistants.

Tools: ``twin_run`` runs an ELF on the twin through the same engine as
``py -m tools.pp run``; ``twin_machines`` lists what the QEMU build offers.
Nothing here writes to stdout: that is the protocol channel.
"""

from pathlib import Path
from typing import Any, TypedDict

import anyio

try:                    # mcp 2.x renamed FastMCP to MCPServer
    from mcp.server.mcpserver import MCPServer as FastMCP
    from mcp.server.mcpserver.exceptions import ToolError
except ImportError:     # mcp 1.x
    from mcp.server.fastmcp import FastMCP
    from mcp.server.fastmcp.exceptions import ToolError

from ..pp import (DEFAULT_MACHINE, QemuNotFound, RunnerError, RunOptions,
                  machines_info, run)
from ..pp.qemu import PROJECT_ROOT

MAX_TIMEOUT_S = 600.0
# The transcript goes into the assistant's context: keep its head and tail.
MAX_UART_CHARS = 64 * 1024
MAX_TRACE_LINES = 200

INSTRUCTIONS = f"""\
Peripheral Pretender runs Microchip MCU firmware on a QEMU digital twin.
twin_machines shows which machines the QEMU build offers; the default board
is {DEFAULT_MACHINE} (PIC32CM5164JH01100 Curiosity Pro, UART = SERCOM4 VCOM).
twin_run boots an ELF and waits for the firmware to print 'PP:EXIT <n>':
exit_code is n, 124 on timeout, 125 if QEMU ended first (status
cpu-lockup for a CPU lockup; see detail and qemu_stderr_tail). On timeout,
detail says where the CPU was (e.g. stuck in HardFault). unimplemented_summary
groups the register accesses the twin does not model yet, by instance and
register: use it to tell firmware bugs from model gaps. A long uart
transcript is cut in the middle (uart_truncated, uart_length)."""


class TwinRunResult(TypedDict):
    exit_code: int
    status: str
    firmware_exit: int | None
    uart: str
    uart_truncated: bool
    uart_length: int
    unimplemented_summary: dict[str, Any]
    qemu_stderr_tail: str
    duration_s: float
    machine: str
    elf: str
    detail: str | None
    trace: dict[str, Any] | None
    warnings: list[str]


mcp = FastMCP("peripheral-pretender", instructions=INSTRUCTIONS,
              log_level="WARNING")


def resolve_elf(elf_path: str) -> Path:
    """Relative paths are tried against the working directory, then the
    project root (clients start servers in arbitrary directories)."""
    path = Path(elf_path).expanduser()
    if path.is_absolute() or path.exists():
        return path
    in_project = PROJECT_ROOT / path
    return in_project if in_project.exists() else path


def bound_uart(text: str, limit: int = MAX_UART_CHARS) -> tuple[str, bool]:
    """text, or its head and tail around an omission marker when longer
    than limit characters; and whether it was cut."""
    if len(text) <= limit:
        return text, False
    half = limit // 2
    omitted = len(text) - 2 * half
    return (text[:half] + f"\n[... {omitted} characters omitted ...]\n"
            + text[-half:]), True


def bound_trace(trace: dict | None) -> dict | None:
    if trace is None or len(trace["lines"]) <= MAX_TRACE_LINES:
        return trace
    return {**trace, "lines": trace["lines"][:MAX_TRACE_LINES],
            "truncated": True}


def machine_extra_args(machine_options: str | None) -> list[str]:
    """machine_options as "-machine OPTIONS" (QEMU merges it into -M); a
    machine type there would silently replace the machine argument."""
    if not machine_options:
        return []
    first = machine_options.split(",", 1)[0]
    if "=" not in first or first.startswith("type="):
        raise ToolError("machine_options takes only key=value options "
                        "(e.g. 'sw0-pressed=on'); name the machine in "
                        "'machine'")
    return ["-machine", machine_options]


@mcp.tool()
async def twin_run(elf_path: str, timeout_s: float = 10.0,
                   input_text: str | None = None,
                   icount_shift: int | None = None,
                   machine: str = DEFAULT_MACHINE,
                   machine_options: str | None = None,
                   input_after: str | None = None,
                   trace: list[str] | None = None) -> TwinRunResult:
    """Run a firmware ELF on the digital twin and report the outcome.

    Args:
        elf_path: ELF file (absolute, or relative to the project root).
        timeout_s: host seconds to wait for 'PP:EXIT <n>' (max 600).
        input_text: text sent to the UART once the firmware has printed
            something; include the line endings it expects (e.g. "\\r\\n").
        icount_shift: deterministic virtual time, 2^shift ns per
            instruction (0-10); omit for real-time pacing. Only output-only
            runs are reproducible: UART input arrives in host time.
        machine: QEMU machine name (see twin_machines).
        machine_options: extra machine properties, comma separated, e.g.
            "sw0-pressed=on" (button SW0 held) on pic32cm-jh01-cpro.
        input_after: send input_text once this text has appeared on the
            UART (e.g. a prompt) instead of after the first output.
        trace: QEMU trace event patterns (e.g. ["mchp_sercom_*"]); matching
            events are counted in trace (first 200 lines kept).

    Returns exit_code (firmware PP:EXIT value, 124 timeout, 125 QEMU ended
    first), status (exit, timeout, qemu-exited, cpu-lockup), detail, uart
    transcript (LF line endings; head and tail of at most 64 KB, see
    uart_truncated and uart_length), unimplemented_summary (model backlog),
    trace, warnings, qemu_stderr_tail, duration_s.
    """
    if not 0 < timeout_s <= MAX_TIMEOUT_S:
        raise ToolError(f"timeout_s must be in (0, {MAX_TIMEOUT_S:g}]")
    options = RunOptions(elf=resolve_elf(elf_path), machine=machine,
                         timeout_s=timeout_s, icount_shift=icount_shift,
                         input_text=input_text, input_after=input_after,
                         trace=list(trace or ()),
                         extra_args=machine_extra_args(machine_options))
    # mcp 2.x hides the message of any exception but ToolError
    try:
        result = await anyio.to_thread.run_sync(run, options)
    except (RunnerError, QemuNotFound, ValueError) as exc:
        raise ToolError(str(exc)) from exc
    uart, uart_truncated = bound_uart(result.uart)
    return TwinRunResult(
        exit_code=result.exit_code,
        status=result.status,
        firmware_exit=result.firmware_exit,
        uart=uart,
        uart_truncated=uart_truncated,
        uart_length=len(result.uart),
        unimplemented_summary=result.unimplemented_summary,
        qemu_stderr_tail=result.qemu_stderr_tail,
        duration_s=result.duration_s,
        machine=result.machine,
        elf=result.elf,
        detail=result.detail,
        trace=bound_trace(result.trace),
        warnings=result.warnings,
    )


@mcp.tool()
async def twin_machines() -> dict[str, Any]:
    """List the QEMU binary in use and the machines it offers.

    default_machine_available tells whether the Peripheral Pretender board
    is built in.
    """
    return await anyio.to_thread.run_sync(machines_info)


def main() -> None:
    mcp.run("stdio")
