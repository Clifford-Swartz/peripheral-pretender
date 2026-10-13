# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The runner against real QEMU on mps2-an385 (CMSDK UART test images).

This proves the runner before the PIC32CM machine exists: exit codes,
input, timeout, transcript integrity and the model backlog.
"""

import json
import re
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

import pytest

from tools.pp import EXIT_NO_EXIT_LINE, EXIT_TIMEOUT, RunOptions, run

pytestmark = pytest.mark.qemu

PROJECT_ROOT = Path(__file__).resolve().parents[2]
MPS2_MACHINE = "mps2-an385"
PATTERN = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"


def run_mps2(elf, **kwargs):
    return run(RunOptions(elf, machine=MPS2_MACHINE, **kwargs))


def test_exit_zero_with_backlog(mps2_images):
    result = run_mps2(mps2_images["exit"])
    assert (result.exit_code, result.status, result.firmware_exit) == \
        (0, "exit", 0)
    assert result.uart == ("pp runner test: exit\n"
                           "gpio read 0x00000000\n"
                           "PP:EXIT 0\n")
    assert result.qemu_returncode is None    # stopped by the runner
    summary = result.unimplemented_summary
    rows = [(g["kind"], g["instance"], g["register"], g["count"])
            for g in summary["groups"]]
    assert rows == [("unimp", "cmsdk-ahb-gpio", "0x0", 3),
                    ("unimp", "cmsdk-ahb-gpio", "0x4", 1),
                    ("unimp", "VGA", "0x0", 1),
                    ("guest_error", "CMSDK APB UART", "0x20", 1)]
    assert result.trace is None


def test_nonzero_exit_code(mps2_images):
    result = run_mps2(mps2_images["exit3"])
    assert (result.exit_code, result.firmware_exit) == (3, 3)


def test_input_is_echoed(mps2_images):
    result = run_mps2(mps2_images["echo"],
                      input_text="hello twin\r\nsecond line\nexit 5\r\n")
    assert result.exit_code == 5
    assert result.uart == ("pp runner test: echo ready\n"
                           "echo: hello twin\n"
                           "echo: second line\n"
                           "echo: exit 5\n"
                           "PP:EXIT 5\n")


def test_input_after_prompt(mps2_images):
    lines = [f"line {i:03d} {PATTERN[:i % 60]}" for i in range(100)]
    text = "".join(f"{line}\r\n" for line in lines) + "exit 0\r\n"
    result = run_mps2(mps2_images["echo"], input_text=text,
                      input_after="ready")
    assert result.exit_code == 0
    echoed = [line[len("echo: "):] for line in result.uart.splitlines()
              if line.startswith("echo: ")]
    assert echoed == lines + ["exit 0"]


def test_timeout_keeps_partial_transcript(mps2_images):
    result = run_mps2(mps2_images["echo"], timeout_s=1.5)
    assert (result.exit_code, result.status) == (EXIT_TIMEOUT, "timeout")
    assert result.uart == "pp runner test: echo ready\n"
    assert 1.5 <= result.duration_s < 5


def test_burst_transcript_is_byte_exact(mps2_images):
    expected = "".join(
        f"burst {i} {PATTERN[i % 62:] + PATTERN[:i % 62]}\n"
        for i in range(2000)) + "PP:EXIT 0\n"
    result = run_mps2(mps2_images["burst"], timeout_s=30)
    assert result.exit_code == 0
    assert result.uart == expected


def test_unknown_machine_reports_qemu_error(mps2_images):
    result = run(RunOptions(mps2_images["exit"], machine="no-such-board"))
    assert (result.exit_code, result.status) == \
        (EXIT_NO_EXIT_LINE, "qemu-exited")
    assert result.qemu_returncode == 1
    assert "no-such-board" in result.qemu_stderr_tail


def test_trace_and_kept_log(mps2_images, tmp_path):
    log = tmp_path / "qemu.log"
    result = run_mps2(mps2_images["exit"], trace=["cmsdk_apb_uart_tx"],
                      log_path=log)
    assert result.exit_code == 0
    assert result.trace["events"] == {"cmsdk_apb_uart_tx": 55}
    assert result.unimplemented_summary["total"] == 6
    assert result.log_path == str(log.resolve())
    assert "VGA: unimplemented device write" in log.read_text()


def test_bus_fault_to_unmapped_address_is_in_the_backlog(mps2_images):
    result = run_mps2(mps2_images["fault"])
    assert (result.exit_code, result.status) == (1, "exit")
    assert result.uart == ("pp runner test: fault\nhardfault\nPP:EXIT 1\n")
    rows = [(g["kind"], g["instance"], g["register"], g["writes"])
            for g in result.unimplemented_summary["groups"]]
    assert rows == [("guest_error", "(unassigned)", "@0x30000000", 1)]


def test_cpu_lockup_has_its_own_status(mps2_images):
    result = run_mps2(mps2_images["lockup"])
    assert (result.exit_code, result.status) == \
        (EXIT_NO_EXIT_LINE, "cpu-lockup")
    assert result.detail.startswith("CPU lockup (can't escalate 3 to "
                                    "HardFault")
    assert "CPU in thread mode at PC=0x" in result.detail
    assert result.unimplemented_summary["total"] == 0  # no CPU dump in it
    assert "qemu: fatal: Lockup" in result.qemu_stderr_tail


def test_timeout_reports_where_the_cpu_is(mps2_images):
    result = run_mps2(mps2_images["echo"], timeout_s=1.0)
    assert result.status == "timeout"
    assert re.fullmatch(r"timeout: CPU in thread mode at PC=0x[0-9A-F]{8}",
                        result.detail)


def test_extra_D_keeps_the_backlog_and_M_sets_the_machine(mps2_images,
                                                          tmp_path):
    log = tmp_path / "sub" / "user.log"
    result = run(RunOptions(mps2_images["exit"],
                            extra_args=["-D", str(log), "-M", MPS2_MACHINE]))
    assert result.exit_code == 0
    assert result.machine == MPS2_MACHINE
    assert result.unimplemented_summary["total"] == 6
    assert result.log_path == str(log.resolve())
    assert "VGA: unimplemented device write" in log.read_text()


def test_misspelt_trace_pattern_warns(mps2_images):
    result = run_mps2(mps2_images["exit"],
                      trace=["cmsdk_apb_uart_tx", "cmsdk_apb_uart_txx"])
    assert result.trace["events"] == {"cmsdk_apb_uart_tx": 55}
    assert len(result.warnings) == 1
    assert "'cmsdk_apb_uart_txx'" in result.warnings[0]


def test_icount(mps2_images):
    result = run_mps2(mps2_images["exit"], icount_shift=4)
    assert result.exit_code == 0
    assert "-icount" in result.command


class GdbRemote:
    """Just enough of the GDB remote protocol to drive QEMU's gdbstub."""

    def __init__(self, port, timeout_s=10.0):
        deadline = time.monotonic() + timeout_s
        while True:
            try:
                self.sock = socket.create_connection(("127.0.0.1", port),
                                                     timeout=timeout_s)
                return
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.05)

    def send(self, payload):
        checksum = sum(payload.encode()) % 256
        self.sock.sendall(f"+${payload}#{checksum:02x}".encode())

    def interrupt(self):
        """Ctrl-C; returns once the stop reply has arrived."""
        self.sock.sendall(b"\x03")
        data = b""
        while not re.search(rb"\$[^#]*#..", data):
            data += self.sock.recv(4096)

    def close(self):
        self.sock.close()


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def test_gdb_session_outlives_the_exit_line(mps2_images):
    port = free_port()
    saw_exit = threading.Event()
    uart = bytearray()
    outcome = {}

    def watch(data):    # UART bytes arrive in chunks of a few bytes
        uart.extend(data)
        if b"PP:EXIT 0\r\n" in uart:
            saw_exit.set()

    def run_paused():
        outcome["result"] = run_mps2(mps2_images["exit"], gdb_port=port,
                                     timeout_s=30, on_uart=watch)

    runner_thread = threading.Thread(target=run_paused)
    runner_thread.start()
    gdb = GdbRemote(port)
    try:
        assert not saw_exit.wait(0.5)      # -S: nothing runs before gdb
        gdb.send("c")
        assert saw_exit.wait(10)
        runner_thread.join(0.5)
        assert runner_thread.is_alive()    # QEMU stays up for the debugger
        gdb.interrupt()
        gdb.send("k")
    finally:
        gdb.close()
        runner_thread.join(30)
    result = outcome["result"]
    assert (result.exit_code, result.status) == (0, "exit")
    assert result.qemu_returncode == 0     # QEMU ended by the gdb kill
    assert result.duration_s < 30


def run_cli(*args):
    return subprocess.run([sys.executable, "-m", "tools.pp", *args],
                          cwd=PROJECT_ROOT, capture_output=True, text=True,
                          timeout=60)


def test_cli_streams_transcript_and_reports(mps2_images):
    proc = run_cli("run", str(mps2_images["exit3"]), "-M", MPS2_MACHINE)
    assert proc.returncode == 3
    assert proc.stdout == ("pp runner test: exit\n"
                           "gpio read 0x00000000\n"
                           "PP:EXIT 3\n")
    assert proc.stderr.startswith("[pp] exit 3 (PP:EXIT 3)")
    assert "cmsdk-ahb-gpio" in proc.stderr


def test_cli_json_with_input(mps2_images):
    proc = run_cli("run", str(mps2_images["echo"]), "-M", MPS2_MACHINE,
                   "--json", "--input", "hi\\r\\nexit 7\\r\\n")
    assert proc.returncode == 7
    result = json.loads(proc.stdout)
    assert result["exit_code"] == 7
    assert result["uart"].endswith("echo: hi\necho: exit 7\nPP:EXIT 7\n")
    assert set(result) >= {"exit_code", "uart", "unimplemented_summary",
                           "qemu_stderr_tail", "duration_s"}


def test_cli_timeout_status(mps2_images):
    proc = run_cli("run", str(mps2_images["echo"]), "-M", MPS2_MACHINE,
                   "--timeout", "1")
    assert proc.returncode == EXIT_TIMEOUT
    assert proc.stdout == "pp runner test: echo ready\n"
