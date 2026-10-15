# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""harness.cp_repl: the REPL probe is read-only and always soft-reboots."""

import pytest

from harness import cp_repl


class FakeSerial:
    """A board that answers the REPL; @fail_on raises on that write."""

    def __init__(self, fail_on: bytes | None = None):
        self.sent: list[bytes] = []
        self.replies: list[bytes] = []
        self.fail_on = fail_on

    def reset_input_buffer(self):
        pass

    def write(self, data: bytes):
        if self.fail_on and data == self.fail_on:
            raise OSError("unplugged")
        self.sent.append(data)
        if data == b"\x03":
            self.replies.append(b"KeyboardInterrupt\r\nPress any key to enter the REPL.")
        elif data == b"\r":
            self.replies.append(b"\r\n>>> ")
        elif data == b"\x04":
            self.replies.append(b"soft reboot\r\n")
        else:
            self.replies.append(data + b"\r\n42\r\n>>> ")
        return len(data)

    def read(self, n: int) -> bytes:
        return self.replies.pop(0) if self.replies else b""


def test_probe_runs_lines_and_soft_reboots():
    ser, text = FakeSerial(), []
    cp_repl.probe(ser, ["print(6*7)"], text.append, line_wait=0.2)
    assert ser.sent[-1] == b"\x04"
    assert b"print(6*7)\r" in ser.sent
    assert "42" in "".join(text)


def test_probe_soft_reboots_after_an_error():
    ser = FakeSerial(fail_on=b"print(1)\r")
    with pytest.raises(OSError):
        cp_repl.probe(ser, ["print(1)"], lambda t: None, line_wait=0.2)
    assert ser.sent[-1] == b"\x04"


@pytest.mark.parametrize("line", [
    "open('/code.py', 'w').write('x')",
    "f = open('boot_out.txt', \"a\")",
    "import storage; storage.remount('/', False)",
    "os.remove('code.py')",
    "microcontroller.reset()",
    "microcontroller.nvm[0] = 1",
])
def test_writes_are_refused(line):
    ser, text = FakeSerial(), []
    cp_repl.probe(ser, [line], text.append, line_wait=0.2)
    assert line.encode() + b"\r" not in ser.sent
    assert "refused" in "".join(text)
    assert ser.sent[-1] == b"\x04"


@pytest.mark.parametrize("line", [
    "print(open('boot_out.txt').read())",
    "i2c = board.I2C(); i2c.try_lock(); print(i2c.scan())",
    "print(os.statvfs('/'))",
])
def test_reads_are_allowed(line):
    assert not cp_repl.refused(line)
