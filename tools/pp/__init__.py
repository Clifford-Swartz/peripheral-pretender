# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The ``pp`` twin runner (``py -m tools.pp``).

Python API::

    from tools.pp import RunOptions, run
    result = run(RunOptions("firmware/hello/build/pic32cm_jh01_cpro/hello.elf"))
    result.exit_code, result.uart, result.unimplemented_summary
"""

from .qemu import (DEFAULT_MACHINE, QemuNotFound, find_qemu, list_machines,
                   machines_info)
from .runner import (EXIT_NO_EXIT_LINE, EXIT_TIMEOUT, RunnerError, RunOptions,
                     RunResult, run)
from .unimp import LogSummary, parse_line, summarize_log

__all__ = [
    "DEFAULT_MACHINE", "EXIT_NO_EXIT_LINE", "EXIT_TIMEOUT", "LogSummary",
    "QemuNotFound", "RunOptions", "RunResult", "RunnerError", "find_qemu",
    "list_machines", "machines_info", "parse_line", "run", "summarize_log",
]
