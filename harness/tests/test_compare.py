# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Comparison rules for transcripts and register captures."""
import json

import pytest

from harness import compare, snapshot
from harness.regs import Reg


@pytest.fixture
def masks():
    return compare.load_masks([])


def capture(side, regs, events, reset="qemu-cold", skipped=()):
    """events: [(label, {name: value or Exception})]."""
    cap = snapshot.new_capture(side, "test", side, regs, list(skipped), reset=reset)
    for seq, (label, readings) in enumerate(events):
        values = {n: v for n, v in readings.items() if not isinstance(v, Exception)}
        errors = {n: str(v) for n, v in readings.items() if isinstance(v, Exception)}
        cap["events"].append(snapshot.make_event(seq, label, "breakpoint", values, errors))
    return cap


def save(tmp_path, name, doc):
    path = tmp_path / name
    path.write_text(json.dumps(doc), encoding="utf-8")
    return str(path)


GENCTRL0 = Reg("GCLK.GENCTRL[0]", 0x40001C20, 4, reset=0x106, module="GCLK", ip_id="U2122")
STATUS = Reg("OSCCTRL.STATUS", 0x4000100C, 4, reset=0x10, module="OSCCTRL", ip_id="U2119",
             dynamic=True)
CAL48M = Reg("OSCCTRL.CAL48M", 0x40001038, 4, reset=0x00200220, module="OSCCTRL",
             ip_id="U2119")
RCAUSE = Reg("RSTC.RCAUSE", 0x40000C00, 1, reset=0x01, module="RSTC", ip_id="U2239")


# --- default masks ----------------------------------------------------------------

def test_default_masks_parse_and_explain_themselves(masks):
    assert masks.registers and masks.uart
    assert all(reason for _, _, reason in masks.registers)
    bits, why = masks.ignored(["NVM.USER_PAGE_WORD_0"])
    assert bits == 0x03FE0000 and "BODCORE" in why
    assert masks.ignored(["DSU.DID"]) == (0, "")       # configure silicon-rev instead


# --- transcripts -------------------------------------------------------------------

HELLO = ("PP hello: PIC32CM5164JH01100 on PIC32CM JH01 Curiosity Pro (EV81X90A)\r\n"
         "step 5 clock: CAL48M=0x{cal} OSC48MDIV=0x00\r\n"
         "dsu: DID=0x11060100\r\n"
         "serial: 0x{serial} 0x00000001 0x00000002 0x00000003\r\n"
         "calrow: 0x{cal}{cal}\r\n"
         "PP:EXIT 0\r\n")


def test_uart_masks_per_chip_lines_only(masks):
    a = compare.transcript_lines(HELLO.format(cal="00200220", serial="50500000"), masks)
    b = compare.transcript_lines(
        "\x00�" + HELLO.format(cal="12345678", serial="ABCDEF01"), masks)
    assert a == b
    assert "dsu: DID=0x11060100" in a                  # revision is not masked


def test_uart_compare_reads_json_and_raw(tmp_path, masks, capsys):
    twin = save(tmp_path, "twin.json", {"exit_code": 0, "uart": "a\nPP:EXIT 0\n"})
    raw = tmp_path / "silicon.bin"
    raw.write_bytes(b"a\r\nPP:EXIT 1\r\n")
    args = compare.argparse.Namespace(a=twin, b=str(raw), start=None)
    assert compare.compare_uart(args, masks) == 1
    assert "-PP:EXIT 0" in capsys.readouterr().out
    raw.write_bytes(b"boot noise\r\na\r\nPP:EXIT 0\r\n")
    assert compare.compare_uart(compare.argparse.Namespace(a=twin, b=str(raw), start="^a$"),
                                masks) == 0


# --- register captures ---------------------------------------------------------------

def test_equal_captures(masks):
    a = capture("twin", [GENCTRL0], [("start", {GENCTRL0.name: "0x00000106"})])
    problems, notes, diffs, stats = compare.compare_captures(a, a, masks)
    assert (problems, notes, diffs) == ([], [], [])
    assert stats[("GCLK", "same")] == 1


def test_value_difference_and_masked_bits(masks):
    regs = [GENCTRL0, CAL48M]
    a = capture("twin", regs, [("start", {GENCTRL0.name: "0x00000106",
                                          CAL48M.name: "0x00200220"})])
    b = capture("silicon", regs, [("start", {GENCTRL0.name: "0x00010106",
                                             CAL48M.name: "0x00230136"})])   # TCAL, FCAL
    _, _, diffs, stats = compare.compare_captures(a, b, masks)
    assert [(d.name, d.a, d.b) for d in diffs] == [("GCLK.GENCTRL[0]", "0x00000106",
                                                    "0x00010106")]
    assert "diff=0x00010000" in diffs[0].line()
    assert stats[("OSCCTRL", "masked")] == 1


def test_read_fault_on_one_side_is_a_difference(masks):
    a = capture("twin", [GENCTRL0], [("start", {GENCTRL0.name: "0x00000106"})])
    b = capture("silicon", [GENCTRL0], [("start", {GENCTRL0.name: OSError("bus fault")})])
    _, _, diffs, _ = compare.compare_captures(a, b, masks)
    assert diffs[0].note == "read fault on B: bus fault"


def test_registers_read_on_one_side_are_noted_not_different(masks):
    a = capture("twin", [GENCTRL0, CAL48M], [("start", {GENCTRL0.name: "0x00000106",
                                                        CAL48M.name: "0x00200220"})])
    b = capture("silicon", [GENCTRL0], [("start", {GENCTRL0.name: "0x00000106"})])
    problems, notes, diffs, _ = compare.compare_captures(a, b, masks)
    assert (problems, diffs) == ([], [])
    assert notes == ["not compared: 1 registers read only in A, 0 only in B"]


def test_rcause_compared_only_for_the_same_reset(masks):
    a = capture("twin", [RCAUSE], [("dump", {RCAUSE.name: "0x01"})], reset="qemu-cold")
    b = capture("silicon", [RCAUSE], [("dump", {RCAUSE.name: "0x40"})], reset="dfp-reset-halt")
    assert compare.compare_captures(a, b, masks)[2] == []
    b["reset"] = "qemu-cold"
    assert len(compare.compare_captures(a, b, masks)[2]) == 1


def test_event_order_is_a_problem(masks):
    a = capture("twin", [GENCTRL0], [("start", {}), ("main", {})])
    b = capture("silicon", [GENCTRL0], [("start", {}), ("timeout", {})])
    problems, *_ = compare.compare_captures(a, b, masks)
    assert problems and problems[0].startswith("event order differs")


def test_reset_command_checks_documented_values(tmp_path, capsys):
    silicon = capture("silicon", [GENCTRL0, RCAUSE, Reg("X.NORESET", 0x40009000, 4)],
                      [("dump", {GENCTRL0.name: "0x00000100", RCAUSE.name: "0x40",
                                 "X.NORESET": "0x00000005"})], reset="dfp-reset-halt")
    path = save(tmp_path, "golden.json", silicon)
    assert compare.main(["reset", path]) == 1
    out = capsys.readouterr().out
    assert "GCLK.GENCTRL[0]" in out and "RSTC.RCAUSE" not in out
    assert "1 registers read only in A" in out          # X.NORESET has no reset value


def test_dump_pc_is_a_note_breakpoint_pc_a_problem(masks):
    a = capture("twin", [GENCTRL0], [("dump", {GENCTRL0.name: "0x00000106"})])
    b = capture("twin", [GENCTRL0], [("dump", {GENCTRL0.name: "0x00000106"})])
    a["events"][0]["pc"], b["events"][0]["pc"] = "0xFFFFFFFE", "0x000006A8"
    a["tool"] = "dump_regs"           # twin dumped without --kernel
    problems, notes, diffs, _ = compare.compare_captures(a, b, masks)
    assert (problems, diffs) == ([], [])
    assert "pc A=0xFFFFFFFE" in notes[0] and "--kernel" in notes[0]
    a["tool"] = b["tool"] = "gdb_snapshot"
    problems, *_ = compare.compare_captures(a, b, masks)
    assert problems == ["event #0 (dump): pc A=0xFFFFFFFE B=0x000006A8"]
    pc_masked = compare.Masks(masks.registers, [(compare.name_pattern("pc"),
                                                 0xFFFFFFFF, "test")])
    assert compare.compare_captures(a, b, pc_masked)[0] == []


def test_reset_uses_architectural_values_and_masks_fuse_loaded(tmp_path, capsys):
    aircr = Reg("SystemControl.AIRCR", 0xE000ED0C, 4, reset=0, module="SYSTEMCONTROL")
    ccr = Reg("SystemControl.CCR", 0xE000ED14, 4, reset=0x204, module="SYSTEMCONTROL")
    lock = Reg("NVMCTRL.LOCK", 0x41004020, 2, reset=0, module="NVMCTRL", ip_id="03929",
               reset_from=("USER_FUSES.USER_WORD_1.NVMCTRL_REGION_LOCKS",))
    intflag = Reg("SUPC.INTFLAG", 0x40001808, 4, reset=0, module="SUPC", ip_id="U2117")
    calib = Reg("SysTick.CALIB", 0xE000E01C, 4, reset=0, module="SYSTICK")
    twin = capture("twin", [aircr, ccr, lock, intflag, calib],
                   [("dump", {aircr.name: "0xFA050000", ccr.name: "0x00000208",
                              lock.name: "0xFFFF", intflag.name: "0x00000005",
                              calib.name: "0x80000000"})])
    path = save(tmp_path, "twin.json", twin)
    assert compare.main(["reset", path]) == 0
    out = capsys.readouterr().out
    assert "NVMCTRL.LOCK=0xFFFF" in out and "USER_PAGE_WORD" in out
    assert out.rstrip().endswith("0 differences")
    # The reset-only masks do not apply to twin-vs-silicon comparisons.
    masks = compare.load_masks([])
    assert masks.ignored(["SUPC.INTFLAG"]) == (0, "")
    assert masks.for_reset().ignored(["SUPC.INTFLAG"])[0] == 0x5


# --- reset_overrides suggestions ------------------------------------------------------

def test_overrides_grouped_per_overlay_and_kind(masks):
    sercoms = [Reg(f"SERCOM{i}.CTRLA", 0x42000400 + 0x400 * i, 4, reset=0,
                   module="SERCOM", ip_id="U2201") for i in range(3)]
    wdt = Reg("WDT.CTRLA", 0x40002000, 1, reset=0, module="WDT", ip_id="U2251",
              reset_from=("USER_FUSES.USER_WORD_0.WDT_ENABLE",))
    nvm = Reg("NVM.USER_PAGE_WORD_0", 0x804000, 4, reset=0xB15088FB, module="NVM")
    mpu = Reg("MPU.TYPE", 0xE000ED90, 4, module="MPU")
    regs = [GENCTRL0, STATUS, wdt, nvm, mpu] + sercoms
    twin = {GENCTRL0.name: "0x00000106", STATUS.name: "0x00000010", wdt.name: "0x00",
            nvm.name: "0xB15088FB", mpu.name: "0x00000000"}
    twin.update({r.name: "0x00000000" for r in sercoms})
    silicon = {GENCTRL0.name: "0x00010106", STATUS.name: "0x00000011", wdt.name: "0x82",
               nvm.name: "0xFFFFFFFF", mpu.name: "0x00000800",
               "SERCOM0.CTRLA": "0x00000002", "SERCOM1.CTRLA": "0x00000002",
               "SERCOM2.CTRLA": "0x00000000"}
    a = capture("twin", regs, [("dump", twin)])
    b = capture("silicon", regs, [("dump", silicon)])
    _, _, diffs, _ = compare.compare_captures(a, b, masks)
    text = compare.overrides_yaml(b, diffs, "b")
    assert 'overlays/gclk_u2122.yaml:\n  reset_overrides:\n    "GENCTRL[0]": 0x00010106' in text
    assert '# "CTRLA": 0x00000002 only on SERCOM0, SERCOM1 of 3 instances' in text
    assert "overlays/sercom_u2201.yaml" in text
    for kind in ("NVM rows", "NVM user row", "computed by the model", "Arm core"):
        assert kind in text
    assert "from USER_FUSES.USER_WORD_0.WDT_ENABLE" in text
