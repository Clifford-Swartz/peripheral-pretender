# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""The pic32cm-mc00-cpro twin against data captured from a real board.

golden/pic32cm1216mc00048-a1/ holds what an EV15N46A (PIC32CM1216MC00048,
rev A1) gave on 2026-10-13: every register right after a reset
(reset.json) and the VCOM output of firmware/hello applications
(<app>-vcom.bin: hello, and the peripheral tests in tests/). The
twin, configured as that part (revision and serial number), must
reproduce both; the only register differences allowed are listed in
EXPECTED with the reason.
"""

import json
import socket
import subprocess
import time
from pathlib import Path

import pytest

from tools.periph.cli import fields
from tools.pp import RunOptions, run

pytestmark = pytest.mark.qemu

ROOT = Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "golden" / "pic32cm1216mc00048-a1"
MACHINE = "pic32cm-mc00-cpro"
SOC = "pic32cm1216mc00048-soc"
BUILD = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro"
# firmware/hello applications whose silicon VCOM output is in GOLDEN
APPS = sorted(p.name[:-len("-vcom.bin")] for p in GOLDEN.glob("*-vcom.bin"))

# Differences that are not twin bugs, or are known and open
EXPECTED = {
    "RSTC.RCAUSE": "captured after SYSRESETREQ (SYST); the twin starts at POR",
    "OSC32KCTRL.XOSC32K": "reset only at power-on: kept the factory firmware's "
                          "setting across the warm reset",
    "PORT.GROUP[0].IN": "pad levels of the board",
    "DSU.STATUSB": "DBGPRES: a debugger was attached",
    "DSU.DCC[0]": "debug communication data left in the register",
    "DSU.DCC[1]": "debug communication data left in the register",
    "DSU.DCFG[0]": "device configuration seen by the debugger (unconfirmed)",
    "DSU.DCFG[1]": "device configuration seen by the debugger (unconfirmed)",
    "MTB.POSITION": "trace state, UNKNOWN at reset",
    "MTB.MASTER": "trace state, UNKNOWN at reset",
    "MTB.FLOW": "trace state, UNKNOWN at reset",
    "SUPC.INTFLAG": "open: undocumented bit 3",
    "SUPC.STATUS": "open: undocumented bits 3 and 5",
}


def golden():
    return json.loads((GOLDEN / "reset.json").read_text(encoding="utf-8"))


def register_sizes() -> dict[str, int]:
    """Register element name (as in reset.json) -> access size."""
    dev = json.loads((ROOT / "devices" / "pic32cm1216mc00048.json")
                     .read_text(encoding="utf-8"))
    out = {}
    for inst in dev["instances"]:
        ip = dev["ips"].get(inst.get("ip") or "")
        if not ip or ip.get("core") or not ip["views"]:
            continue
        view = next((v for v in ip["views"] if v["mode"] == ip["default_view"]),
                    ip["views"][0])
        for r in view["registers"]:
            for i in range(r["count"]):
                name = f"{inst['name']}.{r['name']}" + (f"[{i}]" if r["count"] > 1 else "")
                out[name] = (inst["base"] + r["offset"] + i * r["stride"], r["size"])
    return out


class Monitor:
    """QEMU paused at reset (-S), read through QMP human-monitor-command."""

    def __init__(self, qemu: Path, args: list[str]):
        from tools.pp.qemu import qemu_env
        with socket.socket() as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]
        self.proc = subprocess.Popen(
            [str(qemu), "-M", MACHINE, "-S", "-display", "none", "-serial", "null",
             "-monitor", "none", *args,
             "-qmp", f"tcp:127.0.0.1:{port},server=on,wait=off"],
            env=qemu_env(), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 20
        while True:
            try:
                self.sock = socket.create_connection(("127.0.0.1", port), timeout=10)
                break
            except OSError:
                if time.monotonic() > deadline or self.proc.poll() is not None:
                    self.close()
                    pytest.fail("QEMU did not open its QMP socket")
                time.sleep(0.1)
        self.f = self.sock.makefile("rw", encoding="utf-8")
        self._recv()
        self._send({"execute": "qmp_capabilities"})

    def _recv(self) -> dict:
        while True:
            msg = json.loads(self.f.readline())
            if "event" not in msg:
                return msg

    def _send(self, cmd: dict) -> dict:
        self.f.write(json.dumps(cmd) + "\n")
        self.f.flush()
        return self._recv()

    def read(self, addr: int, size: int) -> int:
        # 'x' reads with debug attributes: no side effects (the MPU is off)
        fmt = {1: "b", 2: "h", 4: "w"}[size]
        text = self._send({"execute": "human-monitor-command",
                           "arguments": {"command-line": f"x /1{fmt}x 0x{addr:x}"}})
        return int(text["return"].split(":")[1].split()[0], 16)

    def close(self):
        self.proc.kill()
        self.proc.wait()


@pytest.fixture(scope="module")
def mc00(qemu_path, qemu_machines):
    if MACHINE not in qemu_machines:
        pytest.skip(f"QEMU build has no {MACHINE} machine yet")
    return qemu_path


def test_reset_values_match_silicon(mc00):
    gold = golden()
    sizes = register_sizes()
    mon = Monitor(mc00, ["-global", f"{SOC}.silicon-rev={gold['revision']}"])
    try:
        twin = {name: mon.read(*sizes[name]) for name in gold["registers"]}
    finally:
        mon.close()
    diffs = {name: (value, twin[name]) for name, value in gold["registers"].items()
             if value != twin[name]}
    unexpected = {n: f"silicon 0x{s:X}, twin 0x{t:X}"
                  for n, (s, t) in diffs.items() if n not in EXPECTED}
    assert not unexpected, unexpected
    assert len(gold["registers"]) > 800


@pytest.mark.parametrize("app", APPS)
def test_transcript_matches_silicon(mc00, app):
    elf = BUILD / f"{app}.elf"
    if not elf.is_file():
        pytest.skip(f"{elf.relative_to(ROOT)} not built (make -C firmware/hello "
                    f"BOARD=pic32cm_mc00_cpro APP={app})")
    gold = golden()
    # Deterministic virtual time: peripheral tests compare tick counts with
    # SysTick, which real-time pacing makes jittery
    result = run(RunOptions(elf, machine=MACHINE, timeout_s=60, icount_shift=5,
                            extra_args=[
        "-global", f"{SOC}.silicon-rev={gold['revision']}",
        "-global", f"{SOC}.serial-number={gold['serial_number']}"]))
    silicon = (GOLDEN / f"{app}-vcom.bin").read_bytes().decode("ascii")
    assert result.exit_code == 0, result.uart
    assert result.uart == silicon.replace("\r\n", "\n")


# Checked applications: <app>-silicon.txt is what silicon printed passing
# them (py -m tools.periph check mc00 <app> puts it there)
CHECKED = sorted(p.name[:-len("-silicon.txt")]
                 for p in GOLDEN.glob("*-silicon.txt"))


@pytest.mark.parametrize("app", CHECKED)
def test_checked_apps_pass(mc00, app):
    """The checked applications test themselves against what silicon did
    (values that follow the part, its supply or its temperature within a
    range, so no byte golden); the twin passes the same checks, in
    deterministic time (they time things with SysTick)."""
    elf = BUILD / f"{app}.elf"
    if not elf.is_file():
        pytest.skip(f"{elf.relative_to(ROOT)} not built (make -C firmware/hello "
                    f"BOARD=pic32cm_mc00_cpro APP={app})")
    result = run(RunOptions(elf, machine=MACHINE, timeout_s=60,
                            icount_shift=5))
    assert result.firmware_exit == 0, result.uart
    assert "FAIL" not in result.uart, result.uart
    golden = (GOLDEN / f"{app}-silicon.txt").read_text(encoding="utf-8",
                                                       errors="replace")
    assert structure(result.uart) == structure(golden), (
        f"the golden is from another version of {app}.c: refresh it with "
        f"py -m tools.periph check mc00 {app}")


def structure(text: str) -> list[tuple[str, tuple[str, ...]]]:
    """Each line's head and field names: what the app checks, not the
    values, so a golden from an older version of the app shows."""
    out = []
    for line in text.replace("\r", "").split("\n"):
        if not line.strip() or line.startswith(("[pp]", "    ")):
            continue
        head, kv = fields(line)
        out.append((head, tuple(k for k, _ in kv)))
        if line.startswith("PP:EXIT"):
            break                       # what follows depends on capture
    return out
