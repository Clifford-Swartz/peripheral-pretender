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
                   "-d", "unimp,guest_errors,in_asm", "-D", "q.log",
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
    elf.write_bytes(b"\x7fELF")
    monkeypatch.setenv("PP_QEMU", str(tmp_path / "no-qemu.exe"))
    assert cli.main(["run", str(elf)]) == cli.EXIT_QEMU_MISSING
    assert "PP_QEMU" in capsys.readouterr().err
