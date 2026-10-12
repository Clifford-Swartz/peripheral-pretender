# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""VCOM: port selection, DTR, capture and the PP:EXIT protocol (no real port)."""
import pytest

from harness import vcom


@pytest.mark.parametrize("data, idle, code", [
    (b"PP:EXIT 0\r\n", False, 0),
    (b"boot\r\nPP:EXIT 12\n", False, 12),
    (b"PP:EXIT 1", False, None),            # the line may still grow ("12")
    (b"PP:EXIT 1", True, 1),
    (b"xPP:EXIT 3\n", False, None),         # must start a line
    (b"PP:EXIT \n", False, None),
])
def test_exit_code(data, idle, code):
    assert vcom.exit_code(data, idle=idle) == code


class FakePort:
    """Serves chunks like pyserial with a short timeout (b"" = nothing yet)."""

    def __init__(self, chunks):
        self.chunks = list(chunks)

    @property
    def in_waiting(self):
        return len(self.chunks[0]) if self.chunks else 0

    def read(self, size):
        return self.chunks.pop(0) if self.chunks else b""


def test_capture_stops_at_the_exit_line():
    port = FakePort([b"hello\r\nPP:EX", b"", b"IT 0\r\n", b"after\r\n"])
    echoed = []
    result = vcom.capture(port, timeout=5, echo=echoed.append)
    assert result.exit_code == 0
    assert result.text == "hello\nPP:EXIT 0\n"
    assert b"".join(echoed) == result.data and port.chunks == [b"after\r\n"]


def test_capture_times_out_with_partial_output():
    result = vcom.capture(FakePort([b"no exit here"]), timeout=0.2)
    assert result.exit_code is None and result.data == b"no exit here"


PORTS = [
    {"device": "COM3", "vid_pid": "8087:0ACA", "serial_number": "", "description": "BT",
     "debugger": ""},
    {"device": "COM7", "vid_pid": "03EB:2111", "serial_number": "ATML2130021800001234",
     "description": "EDBG Virtual COM Port", "debugger": "EDBG"},
    {"device": "COM9", "vid_pid": "03EB:2111", "serial_number": "ATML2130021800005678",
     "description": "EDBG Virtual COM Port", "debugger": "EDBG"},
]


def test_port_selection(monkeypatch):
    monkeypatch.setattr(vcom, "list_all_ports", lambda: PORTS)
    assert vcom.pick_port(probe="atml2130021800005678") == "COM9"
    assert vcom.pick_port(port="COM20") == "COM20"
    with pytest.raises(vcom.VcomError, match="several debugger COM ports"):
        vcom.pick_port()
    with pytest.raises(vcom.VcomError, match="no EDBG virtual COM port found for probe X"):
        vcom.pick_port(probe="X")


def test_open_asserts_dtr_before_opening(monkeypatch):
    import serial

    seen = {}

    class FakeSerial:
        def open(self):
            seen["dtr"], seen["baud"], seen["port"] = self.dtr, self.baudrate, self.port

        def reset_input_buffer(self):
            seen["flushed"] = True

    monkeypatch.setattr(serial, "Serial", FakeSerial)
    vcom.open_port("COM7")
    assert seen == {"dtr": True, "baud": 115200, "port": "COM7", "flushed": True}
