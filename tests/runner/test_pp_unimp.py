# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Classification and grouping of QEMU -d unimp,guest_errors log lines."""

import pytest

from tools.pp.unimp import (GUEST_ERROR, NO_REGISTER, OTHER, UNIMP, UNPARSED,
                            LogSummary, parse_line, summarize_lines,
                            summarize_log, trace_event)

# Verbatim from a run of tests/assets/mps2/exit.c on mps2-an385.
MPS2_LOG = "".join(line + "\n" for line in [
    "cmsdk-ahb-gpio: unimplemented device read  (size 4, offset 0x000)",
    "cmsdk-ahb-gpio: unimplemented device read  (size 4, offset 0x000)",
    "cmsdk-ahb-gpio: unimplemented device read  (size 4, offset 0x000)",
    "cmsdk-ahb-gpio: unimplemented device write (size 4, offset 0x004, "
    "value 0x00000001)",
    "VGA: unimplemented device write (size 4, offset 0x000000, "
    "value 0x00000041)",
    "CMSDK APB UART write: bad offset 0x20",
])


def entry_fields(line):
    e = parse_line(line)
    return e.kind, e.instance, e.register, e.access


@pytest.mark.parametrize("line, expected", [
    ("cmsdk-ahb-gpio: unimplemented device read  (size 4, offset 0x000)",
     (UNIMP, "cmsdk-ahb-gpio", "0x0", "read")),
    ("VGA: unimplemented device write (size 4, offset 0x000000, "
     "value 0x00000041)",
     (UNIMP, "VGA", "0x0", "write")),
    ("CMSDK APB peripheral region @0x40000000: unimplemented device read  "
     "(size 2, offset 0x0abc)",
     (UNIMP, "CMSDK APB peripheral region @0x40000000", "0xabc", "read")),
    ("CMSDK APB UART write: bad offset 0x20",
     (GUEST_ERROR, "CMSDK APB UART", "0x20", "write")),
    ("CMSDK APB UART read: bad offset 2c",
     (GUEST_ERROR, "CMSDK APB UART", "0x2c", "read")),
    ("NVIC: Bad read offset 0xd90",
     (GUEST_ERROR, "NVIC", "0xd90", "read")),
    ("Invalid read at addr 0x41000000, size 4, region '(null)', "
     "reason: rejected",
     (GUEST_ERROR, "(unassigned)", "@0x41000000", "read")),
    ("Invalid write at addr 0x1F, size 4, region 'sercom4', "
     "reason: unaligned",
     (GUEST_ERROR, "sercom4", "@0x1f", "write")),
    ("Invalid access to non-RAM device at addr 0x40001000, size 4, "
     "region 'gclk'",
     (GUEST_ERROR, "gclk", "@0x40001000", None)),
])
def test_known_qemu_formats(line, expected):
    assert entry_fields(line) == expected


@pytest.mark.parametrize("line, expected", [
    # Register-named messages from the mchp models (section 5.7).
    ("sercom4: CTRLA.MODE 0x3 (SPI host) not modelled",
     (UNIMP, "sercom4", "CTRLA", None)),
    ("mchp_regblock: tc0.CTRLA write 0x00000002 not implemented",
     (UNIMP, "tc0", "CTRLA", "write")),
    ("gclk: PCHCTRL[22] write while enabled ignored",
     (GUEST_ERROR, "gclk", "PCHCTRL[22]", "write")),
    ("nvmctrl: CTRLA command 0x02 (ER) unsupported",
     (UNIMP, "nvmctrl", "CTRLA", None)),
    ("M profile: zero FTYPE in exception return",
     (GUEST_ERROR, "M profile", "FTYPE", None)),
    ("pm: something odd happened",
     (GUEST_ERROR, "pm", NO_REGISTER, None)),
])
def test_prefixed_messages(line, expected):
    assert entry_fields(line) == expected


def test_unprefixed_lines():
    assert entry_fields("R00=00000000 R01=00000001 R02=00000002") == \
        (OTHER, UNPARSED, NO_REGISTER, None)
    assert entry_fields("Unimplemented opcode")[0] == UNIMP
    assert parse_line("   \r\n") is None


def test_trace_lines_need_an_enabled_pattern():
    line = "cmsdk_apb_uart_tx CMSDK APB UART: character 0x70 sent to backend"
    assert trace_event(line, ["cmsdk_apb_uart_*"]) == "cmsdk_apb_uart_tx"
    assert trace_event("12@1700000000.000123:" + line,
                       ["cmsdk_apb_uart_tx"]) == "cmsdk_apb_uart_tx"
    assert trace_event(line, ["mchp_*"]) is None
    assert trace_event("sercom4: CTRLA write", ["sercom4"]) is None


def test_summary_of_mps2_log():
    summary = summarize_lines(MPS2_LOG.splitlines()).as_dict()
    assert summary["total"] == 6
    assert summary["counts"] == {UNIMP: 5, GUEST_ERROR: 1, OTHER: 0}
    assert summary["instances"] == {"cmsdk-ahb-gpio": 4, "CMSDK APB UART": 1,
                                    "VGA": 1}
    rows = [(g["kind"], g["instance"], g["register"], g["count"], g["reads"],
             g["writes"]) for g in summary["groups"]]
    assert rows == [
        (UNIMP, "cmsdk-ahb-gpio", "0x0", 3, 3, 0),
        (UNIMP, "cmsdk-ahb-gpio", "0x4", 1, 0, 1),
        (UNIMP, "VGA", "0x0", 1, 0, 1),
        (GUEST_ERROR, "CMSDK APB UART", "0x20", 1, 0, 1),
    ]
    assert summary["groups"][3]["samples"] == [
        "CMSDK APB UART write: bad offset 0x20"]


def test_samples_are_capped_and_counts_are_not():
    lines = [f"sercom4: DATA read {i} not modelled" for i in range(10)]
    summary = summarize_lines(lines).as_dict()
    (group,) = summary["groups"]
    assert group["count"] == 10 and group["reads"] == 10
    assert group["samples"] == lines[:3]


def test_repeated_lines_count_fully_with_distinct_samples():
    busy = "sercom4: SYNCBUSY read not modelled"
    lines = [busy] * 5000 + ["sercom4: SYNCBUSY read (again) not modelled"]
    summary = summarize_lines(lines).as_dict()
    (group,) = summary["groups"]
    assert (group["count"], group["reads"]) == (5001, 5001)
    assert group["samples"] == [busy, lines[-1]]


def test_trace_digest_is_separate_from_backlog():
    summary = LogSummary(["led_*"], max_trace_lines=2)
    for line in ["led_set_intensity LED desc:'LED0' color:green "
                 "intensity: 0%"] * 3 + ["VGA: unimplemented device read  "
                                         "(size 4, offset 0x00)"]:
        summary.add_line(line)
    assert summary.as_dict()["total"] == 1
    trace = summary.trace_dict()
    assert trace["count"] == 3
    assert trace["events"] == {"led_set_intensity": 3}
    assert len(trace["lines"]) == 2 and trace["truncated"]


def test_summarize_log_reads_crlf_files(tmp_path):
    log = tmp_path / "qemu.log"
    log.write_bytes(MPS2_LOG.replace("\n", "\r\n").encode())
    assert summarize_log(log).as_dict()["total"] == 6


def test_summarize_missing_log_is_empty(tmp_path):
    summary = summarize_log(tmp_path / "absent.log").as_dict()
    assert summary["total"] == 0 and summary["groups"] == []
