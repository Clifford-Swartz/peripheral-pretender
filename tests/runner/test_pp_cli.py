# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""CLI parsing, QEMU command construction and report formatting (no QEMU)."""

import io
import json
from pathlib import Path

import pytest

from tools.pp import cli, runner
from tools.pp.runner import RunOptions, RunResult, build_command

EMPTY_SUMMARY = {"total": 0, "counts": {"unimp": 0, "guest_error": 0,
                                        "other": 0},
                 "instances": {}, "groups": []}


def parse(*argv):
    return cli.build_parser().parse_args(["run", "fw.elf", *argv])


@pytest.mark.parametrize("text, expected", [
    ("abc", b"abc"),
    ("a\\r\\nb\\n", b"a\r\nb\n"),
    ("\\t\\0\\\\", b"\t\0\\"),
    ("\\x41\\x7f\\xff", b"A\x7f\xff"),
    ("\\q \\x4", b"\\q \\x4"),
    ("caf\u00e9", "caf\u00e9".encode()),
])
def test_unescape(text, expected):
    assert cli.unescape(text) == expected


def test_run_defaults():
    options = cli.options_from_args(parse())
    assert options.machine == "pic32cm-jh01-cpro"
    assert options.timeout_s == cli.DEFAULT_TIMEOUT_S
    assert options.gdb_port is None and options.input_text is None


def test_gdb_disables_the_default_timeout():
    assert cli.options_from_args(parse("--gdb")).timeout_s is None
    assert cli.options_from_args(parse("--gdb")).gdb_port == 1234
    options = cli.options_from_args(parse("--gdb", "3333", "--timeout", "5"))
    assert (options.gdb_port, options.timeout_s) == (3333, 5.0)


def test_input_and_remainder_args():
    options = cli.options_from_args(parse(
        "-M", "mps2-an385", "--input", "exit 3\\r\\n", "--trace", "a_*",
        "--trace", "b", "--extra-args", "-d", "in_asm", "-icount", "4"))
    assert options.machine == "mps2-an385"
    assert options.input_text == b"exit 3\r\n"
    assert list(options.trace) == ["a_*", "b"]
    assert options.extra_args == ["-d", "in_asm", "-icount", "4"]


def test_build_command():
    options = RunOptions("fw.elf", machine="mps2-an385", icount_shift=3,
                         gdb_port=1234, trace=["mchp_*"],
                         extra_args=["-d", "in_asm,unimp", "-s"])
    cmd = build_command(Path("qemu"), Path("fw.elf"), options,
                        ["-serial", "chardev:x"], Path("q.log"))
    assert cmd == ["qemu", "-M", "mps2-an385", "-display", "none",
                   "-monitor", "none", "-serial", "chardev:x",
                   "-d", "unimp,guest_errors,invalid_mem,in_asm",
                   "-D", "q.log",
                   "-kernel", "fw.elf", "-icount", "shift=3,sleep=off",
                   "-gdb", "tcp:127.0.0.1:1234", "-S",
                   "-trace", "mchp_*", "-s"]


def test_trace_patterns_from_extra_args():
    assert runner._trace_patterns(
        ["a"], ["-trace", "enable=b_*", "-trace", "c", "-trace",
                "events=f.txt"]) == ["a", "b_*", "c"]


@pytest.mark.parametrize("options, message", [
    (RunOptions("x", timeout_s=0), "timeout"),
    (RunOptions("x", icount_shift=11), "icount"),
])
def test_bad_options(options, message):
    with pytest.raises(runner.RunnerError, match=message):
        runner._check_options(options)


def test_live_echo_normalises_split_crlf_and_utf8():
    out = io.StringIO()
    echo = cli.LiveEcho(out)
    for chunk in (b"one\r", b"\ntwo \xc3", b"\xa9\r\n", b"bad \xff\r"):
        echo(chunk)
    assert out.getvalue() == "one\ntwo \u00e9\nbad \\xff"
    echo.close()
    assert out.getvalue().endswith("\\xff\r")


def make_result(**overrides):
    fields = dict(exit_code=0, status="exit", firmware_exit=0,
                  uart="hi\nPP:EXIT 0\n", unimplemented_summary=EMPTY_SUMMARY,
                  trace=None, qemu_stderr_tail="", qemu_returncode=None,
                  duration_s=0.5, machine="mps2-an385", elf="fw.elf")
    fields.update(overrides)
    return RunResult(**fields)


@pytest.mark.parametrize("overrides, text", [
    ({}, "[pp] exit 0 (PP:EXIT 0) after 0.50 s on mps2-an385"),
    ({"exit_code": 124, "status": "timeout", "firmware_exit": None},
     "[pp] exit 124 (timeout, no PP:EXIT) after 0.50 s on mps2-an385"),
    ({"exit_code": 125, "status": "qemu-exited", "firmware_exit": None,
      "qemu_returncode": 1},
     "[pp] exit 125 (QEMU exited with status 1 before PP:EXIT) after "
     "0.50 s on mps2-an385"),
    ({"exit_code": 125, "status": "qemu-exited", "firmware_exit": None,
      "qemu_returncode": 0xC0000005},
     "[pp] exit 125 (QEMU exited with status 0xC0000005 "
     "(STATUS_ACCESS_VIOLATION) before PP:EXIT) after 0.50 s on mps2-an385"),
    ({"exit_code": 125, "status": "cpu-lockup", "firmware_exit": None,
      "qemu_returncode": 0xC0000409,
      "detail": "CPU lockup (x); CPU in thread mode at PC=0x00000092"},
     "[pp] exit 125 (CPU lockup, QEMU aborted before PP:EXIT) after "
     "0.50 s on mps2-an385\n"
     "[pp] CPU lockup (x); CPU in thread mode at PC=0x00000092"),
    ({"exit_code": 124, "status": "timeout", "firmware_exit": None,
      "detail": "timeout: CPU in HardFault (IPSR=3) at PC=0x000002D0"},
     "[pp] exit 124 (timeout, no PP:EXIT) after 0.50 s on mps2-an385\n"
     "[pp] timeout: CPU in HardFault (IPSR=3) at PC=0x000002D0"),
])
def test_verdict(overrides, text):
    assert cli.verdict(make_result(**overrides)) == text


def test_format_summary_table():
    summary = {"total": 4, "counts": {"unimp": 3, "guest_error": 1,
                                      "other": 0},
               "instances": {"sercom4": 4},
               "groups": [
                   {"kind": "unimp", "instance": "sercom4", "register":
                    "CTRLB", "count": 3, "reads": 1, "writes": 2,
                    "samples": []},
                   {"kind": "guest_error", "instance": "sercom4",
                    "register": "-", "count": 1, "reads": 0, "writes": 0,
                    "samples": []}]}
    assert cli.format_summary(summary) == [
        "[pp] model backlog: 4 log lines (unimp 3, guest_error 1)",
        "    count  kind         instance  register  r/w",
        "        3  unimp        sercom4   CTRLB     1r 2w",
        "        1  guest_error  sercom4   -         -",
    ]
    assert cli.format_summary(EMPTY_SUMMARY) == [
        "[pp] model backlog: no unimplemented or guest-error accesses"]
    capped = {"reads": 0, "writes": 8, "truncated": True}
    assert cli._rw(capped) == "8w+"


def test_report_warnings_and_kept_log(tmp_path):
    err = io.StringIO()
    missing = tmp_path / "gone" / "q.log"
    cli.report(make_result(warnings=["trace pattern 'x' matches no trace"],
                           log_path=str(missing)), err)
    text = err.getvalue()
    assert text.startswith("[pp] warning: trace pattern 'x'")
    assert "QEMU log kept" not in text          # QEMU never wrote it
    kept = _write(tmp_path / "q.log", b"")
    err = io.StringIO()
    cli.report(make_result(log_path=str(kept)), err)
    assert f"QEMU log kept at {kept}" in err.getvalue()


def test_gdb_hint_quotes_the_elf_path(monkeypatch, capsys):
    monkeypatch.setattr(cli, "run", lambda options: make_result())
    cli.main(["run", "my dir/fw.elf", "--gdb", "3333"])
    err = capsys.readouterr().err
    assert ('"my dir/fw.elf"' in err) or ("'my dir/fw.elf'" in err)
    assert "127.0.0.1:3333" in err


def test_cmd_run_json_and_exit_code(monkeypatch, capsys):
    seen = {}

    def fake_run(options):
        seen["options"] = options
        return make_result(exit_code=3, firmware_exit=3)

    monkeypatch.setattr(cli, "run", fake_run)
    assert cli.main(["run", "fw.elf", "--json"]) == 3
    out = json.loads(capsys.readouterr().out)
    assert out["exit_code"] == 3 and out["uart"] == "hi\nPP:EXIT 0\n"
    assert seen["options"].on_uart is None


def test_cmd_run_human_report(monkeypatch, capsys):
    def fake_run(options):
        options.on_uart(b"hi\r\nPP:EXIT 0\r\n")
        return make_result()

    monkeypatch.setattr(cli, "run", fake_run)
    assert cli.main(["run", "fw.elf"]) == 0
    captured = capsys.readouterr()
    assert captured.out == "hi\nPP:EXIT 0\n"
    assert captured.err.startswith("[pp] exit 0 (PP:EXIT 0)")


def test_main_error_statuses(tmp_path, monkeypatch, capsys):
    assert cli.main(["run", str(tmp_path / "missing.elf")]) == cli.EXIT_USAGE
    elf = tmp_path / "fw.elf"
    elf.write_bytes(arm_elf_header())
    monkeypatch.setenv("PP_QEMU", str(tmp_path / "no-qemu.exe"))
    assert cli.main(["run", str(elf)]) == cli.EXIT_QEMU_MISSING
    assert "PP_QEMU" in capsys.readouterr().err


def arm_elf_header(machine=40):
    """The first 20 bytes of a little-endian ELF32 executable."""
    return (b"\x7fELF\x01\x01\x01" + bytes(9) + (2).to_bytes(2, "little")
            + machine.to_bytes(2, "little"))


@pytest.mark.parametrize("content, message", [
    (b"MZ\x90\x00 not an elf at all", "not an ELF file"),
    (b"\x7fELF\x01", "truncated ELF header"),
    (arm_elf_header(machine=62), r"not an Arm or AVR ELF \(e_machine 62"),
])
def test_non_arm_elf_images_are_usage_errors(tmp_path, content, message):
    image = tmp_path / "fw.bin"
    image.write_bytes(content)
    with pytest.raises(runner.RunnerError, match=message):
        runner.check_elf(image)
    assert runner.check_elf(_write(tmp_path / "ok.elf",
                                   arm_elf_header())) == "arm"
    assert runner.check_elf(_write(tmp_path / "avr.elf",
                                   arm_elf_header(machine=83))) == "avr"


def _write(path, data):
    path.write_bytes(data)
    return path


def test_directory_and_non_elf_from_the_cli(tmp_path, capsys):
    assert cli.main(["run", str(tmp_path)]) == cli.EXIT_USAGE
    assert "is a directory, expected an ELF file" in capsys.readouterr().err
    text = _write(tmp_path / "notes.txt", b"hello")
    assert cli.main(["run", str(text)]) == cli.EXIT_USAGE
    assert "not an ELF file" in capsys.readouterr().err


# --- extra QEMU args -------------------------------------------------------------

def test_extra_args_log_file_and_machine():
    extra = runner.split_extra_args(
        ["-d", "in_asm", "-D", "my.log", "-machine", "sw0-pressed=on", "-s"])
    assert extra.rest == ["-machine", "sw0-pressed=on", "-s"]
    assert (extra.log_items, extra.log_file, extra.machine) == \
        (["in_asm"], "my.log", None)
    assert runner.split_extra_args(
        ["-M", "pic32cm-jh01-cpro,sw0-pressed=on"]).machine == \
        "pic32cm-jh01-cpro"
    assert runner.split_extra_args(
        ["-M", "a", "-machine", "type=mps2-an385,x=1"]).machine == \
        "mps2-an385"


def test_extra_D_replaces_the_runner_log_in_the_command():
    options = RunOptions("fw.elf", extra_args=["-D", "user.log", "-s"])
    cmd = build_command(Path("qemu"), Path("fw.elf"), options, [],
                        Path("user.log"))
    assert cmd.count("-D") == 1 and cmd[-1] == "-s"


def test_kept_log_creates_its_directory_and_rejects_two_files(tmp_path):
    log = tmp_path / "new" / "dir" / "q.log"
    assert runner._kept_log(str(log), None) == log.resolve()
    assert log.parent.is_dir()
    assert runner._kept_log(None, str(log)) == log.resolve()
    assert runner._kept_log(None, None) is None
    with pytest.raises(runner.RunnerError, match="different files"):
        runner._kept_log(str(log), str(tmp_path / "other.log"))


# --- diagnosis helpers ----------------------------------------------------------

def test_describe_returncode(monkeypatch):
    assert runner.describe_returncode(1) == "1"
    assert runner.describe_returncode(0xC0000409) == \
        "0xC0000409 (STATUS_STACK_BUFFER_OVERRUN, e.g. abort())"
    assert runner.describe_returncode(0xC0001234) == "0xC0001234"
    monkeypatch.setattr(runner.sys, "platform", "linux")
    assert runner.describe_returncode(-6) == "signal 6"
    monkeypatch.setattr(runner.sys, "platform", "win32")
    assert runner.describe_returncode(-1073740791) == \
        "0xC0000409 (STATUS_STACK_BUFFER_OVERRUN, e.g. abort())"


LOCKUP_DUMP = """
R00=000000dc R01=00000000 R02=40004000 R03=00000000
R12=00000000 R13=2000fff0 R14=00000091 R15=00000092
XPSR=41000000 -Z-- T priv-thread
FPSCR: 00000000"""


def test_cpu_location():
    assert runner.cpu_location(LOCKUP_DUMP) == \
        "CPU in thread mode at PC=0x00000092"
    hardfault = LOCKUP_DUMP.replace("XPSR=41000000", "XPSR=61000003")
    assert runner.cpu_location(hardfault) == \
        "CPU in HardFault (IPSR=3) at PC=0x00000092"
    irq = LOCKUP_DUMP.replace("XPSR=41000000", "XPSR=01000013")
    assert "IRQ 3 (IPSR=19)" in runner.cpu_location(irq)
    assert runner.cpu_location("no registers") is None


def test_unknown_trace_patterns_warn(monkeypatch):
    monkeypatch.setattr(runner, "_cached_trace_events",
                        lambda qemu: ["cmsdk_apb_uart_tx", "mchp_sercom_rx"])
    assert runner.unknown_trace_patterns(Path("q"), []) == []
    warnings = runner.unknown_trace_patterns(
        Path("q"), ["cmsdk_apb_uart_*", "mchp_sercomm_*"])
    assert len(warnings) == 1 and "'mchp_sercomm_*'" in warnings[0]


def test_trace_event_list_is_cached(monkeypatch, tmp_path):
    qemu = _write(tmp_path / "qemu.exe", b"binary")
    monkeypatch.setattr(runner.tempfile, "gettempdir", lambda: str(tmp_path))
    calls = []

    def fake_events(path):
        calls.append(path)
        return ["a_event", "b_event"]

    monkeypatch.setattr(runner, "trace_events", fake_events)
    assert runner._cached_trace_events(qemu) == ["a_event", "b_event"]
    assert runner._cached_trace_events(qemu) == ["a_event", "b_event"]
    assert len(calls) == 1
