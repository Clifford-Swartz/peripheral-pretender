# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The firmware exit protocol: a final UART line ``PP:EXIT <decimal>``.

See docs/design/v0-vertical-slice.md section 5.6. Firmware for silicon
cannot use semihosting, so the runner watches the UART instead.
"""

from __future__ import annotations

import re

EXIT_LINE = re.compile(rb"PP:EXIT (-?\d+)[ \t]*$")
_LINE_BREAK = re.compile(rb"\r\n|\r|\n")

# An exit line is short; a longer unterminated tail cannot hold one
# except at its end, so only the end is kept.
_MAX_PARTIAL = 256


class ExitScanner:
    """Find the ``PP:EXIT n`` line in a UART byte stream fed in chunks.

    A line counts once it is terminated by CR or LF, so a code split across
    chunks ("PP:EXIT 1" + "2\\n") is read whole. :meth:`pending_exit`
    reports an exit line that has not been terminated yet; the runner
    accepts it after the UART has been quiet for a moment.
    """

    def __init__(self) -> None:
        self.exit_code: int | None = None
        self._partial = b""

    def feed(self, data: bytes) -> int | None:
        """Scan *data*; return the exit code once the exit line is seen."""
        if self.exit_code is not None:
            return self.exit_code
        *lines, partial = _LINE_BREAK.split(self._partial + data)
        for line in lines:
            m = EXIT_LINE.search(line)
            if m:
                self.exit_code = int(m.group(1))
                return self.exit_code
        self._partial = partial[-_MAX_PARTIAL:]
        return None

    def pending_exit(self) -> int | None:
        """Exit code in the unterminated last line, if it is an exit line."""
        if self.exit_code is not None:
            return self.exit_code
        m = EXIT_LINE.search(self._partial)
        return int(m.group(1)) if m else None


def process_exit_status(firmware_exit: int) -> int:
    """Map a firmware exit value onto a process exit status.

    Statuses are 8 bits on POSIX, so values outside 0-255 become 255 rather
    than wrapping (PP:EXIT 256 must not read as success).
    """
    return firmware_exit if 0 <= firmware_exit <= 255 else 255
