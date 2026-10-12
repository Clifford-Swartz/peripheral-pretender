# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""MCP server (stdio transport) exposing the twin runner to AI assistants.

Tools: ``twin_run`` runs an ELF on the twin through the same engine as
``py -m tools.pp run``; ``twin_machines`` lists what the QEMU build offers.
Nothing here writes to stdout: that is the protocol channel.
"""

from pathlib import Path
from typing import Any, TypedDict

import anyio
from mcp.server.fastmcp import FastMCP

from ..pp import DEFAULT_MACHINE, RunOptions, machines_info, run
from ..pp.qemu import PROJECT_ROOT

MAX_TIMEOUT_S = 600.0

INSTRUCTIONS = f"""\
Peripheral Pretender runs Microchip MCU firmware on a QEMU digital twin.
twin_machines shows which machines the QEMU build offers; the default board
is {DEFAULT_MACHINE} (PIC32CM5164JH01100 Curiosity Pro, UART = SERCOM4 VCOM).
twin_run boots an ELF and waits for the firmware to print 'PP:EXIT <n>':
exit_code is n, 124 on timeout, 125 if QEMU ended first (see
qemu_stderr_tail). unimplemented_summary groups the register accesses the
twin does not model yet, by instance and register: use it to tell firmware
bugs from model gaps."""


class TwinRunResult(TypedDict):
    exit_code: int
    status: str
    firmware_exit: int | None
    uart: str
    unimplemented_summary: dict[str, Any]
    qemu_stderr_tail: str
    duration_s: float
    machine: str
    elf: str


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


@mcp.tool()
async def twin_run(elf_path: str, timeout_s: float = 10.0,
                   input_text: str | None = None,
                   icount_shift: int | None = None,
                   machine: str = DEFAULT_MACHINE) -> TwinRunResult:
    """Run a firmware ELF on the digital twin and report the outcome.

    Args:
        elf_path: ELF file (absolute, or relative to the project root).
        timeout_s: host seconds to wait for 'PP:EXIT <n>' (max 600).
        input_text: text sent to the UART once the firmware has printed
            something; include the line endings it expects (e.g. "\\r\\n").
        icount_shift: deterministic virtual time, 2^shift ns per
            instruction (0-10); omit for real-time pacing.
        machine: QEMU machine name (see twin_machines).

    Returns exit_code (firmware PP:EXIT value, 124 timeout, 125 QEMU ended
    first), status, uart transcript (LF line endings),
    unimplemented_summary (model backlog), qemu_stderr_tail, duration_s.
    """
    if not 0 < timeout_s <= MAX_TIMEOUT_S:
        raise ValueError(f"timeout_s must be in (0, {MAX_TIMEOUT_S:g}]")
    options = RunOptions(elf=resolve_elf(elf_path), machine=machine,
                         timeout_s=timeout_s, icount_shift=icount_shift,
                         input_text=input_text)
    result = await anyio.to_thread.run_sync(run, options)
    return TwinRunResult(
        exit_code=result.exit_code,
        status=result.status,
        firmware_exit=result.firmware_exit,
        uart=result.uart,
        unimplemented_summary=result.unimplemented_summary,
        qemu_stderr_tail=result.qemu_stderr_tail,
        duration_s=result.duration_s,
        machine=result.machine,
        elf=result.elf,
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
