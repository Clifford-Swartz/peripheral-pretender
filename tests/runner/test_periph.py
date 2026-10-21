# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""tools.periph: transcript diffs, scaffold templates, sorted inserts
(no QEMU, no board)."""

import re
from string import Template

import pytest

from tools.periph import cli

MC00 = cli.BOARDS["mc00"]


def test_fields_split_head_and_values():
    assert cli.fields("E7 enable | on: a=1 b=0x2 c=") == (
        "E7 enable | on:", [("a", "1"), ("b", "0x2"), ("c", "")])


def test_field_diff_names_moved_fields():
    assert cli.field_diff("E1 x a=1 b=2 a=3", "E1 x a=1 b=5 a=4") == [
        "b: 2 -> 5", "a: 3 -> 4"]


def test_field_diff_refuses_other_shapes():
    assert cli.field_diff("E1 x a=1", "E2 x a=1") is None
    assert cli.field_diff("E1 x a=1 b=2", "E1 x a=1") is None


def write(path, lines):
    path.write_bytes("".join(f"{x}\r\r\n" for x in lines).encode())


def test_diff_pairs_lines_and_ignores_runner_lines(tmp_path, monkeypatch,
                                                  capsys):
    monkeypatch.setattr(cli, "WORK", tmp_path)
    write(cli.transcript(MC00, "x", "silicon"),
          ["PP x", "E0 reset a=0x0", "E1 run n=3 f=1", "E2 odd q=1",
           "PP:EXIT 0"])
    write(cli.transcript(MC00, "x", "twin"),
          ["[pp] machine pic32cm-mc00-cpro", "PP x", "E0 reset a=0x0",
           "E1 run n=4 f=1", "E2 odd q=1 r=2", "PP:EXIT 0",
           "    0 unimplemented accesses"])
    assert not cli.cmd_diff(MC00, "x")
    out = capsys.readouterr().out
    assert "E1 run:  silicon -> twin\n    n: 3 -> 4\n" in out
    assert "- E2 odd q=1\n+ E2 odd q=1 r=2\n" in out
    assert "2 lines differ" in out


def test_diff_match(tmp_path, monkeypatch):
    monkeypatch.setattr(cli, "WORK", tmp_path)
    for where in ("silicon", "twin"):
        write(cli.transcript(MC00, "y", where), ["E0 a=1", "PP:EXIT 0"])
    assert cli.cmd_diff(MC00, "y")


def test_exit_code_takes_the_last(tmp_path):
    p = tmp_path / "t.txt"
    write(p, ["PP:EXIT 3", "PP:EXIT 0"])
    assert cli.exit_code(p) == 0


@pytest.mark.parametrize("instance,name", [("WDT", "wdt"), ("CCL", "ccl"),
                                           ("PDEC", "pdec")])
def test_templates_fill_every_variable(instance, name):
    subst = cli.scaffold_subst(MC00, instance, name)
    for tpl in sorted(cli.TEMPLATES.iterdir()):
        text = Template(tpl.read_text(encoding="utf-8")).substitute(subst)
        assert not re.search(r"\$\{?[A-Za-z_]", text), tpl.name


def test_scaffold_subst_instance_facts():
    s = cli.scaffold_subst(MC00, "WDT", "wdt")
    assert s["BASE"] == "0x40002000"
    assert s["TYPE"] == "TYPE_MCHP_WDT_U2251"
    assert "| 0x00 | CTRLA | 8 |" in s["REGS"]


def test_unknown_instance():
    with pytest.raises(cli.PeriphError):
        cli.scaffold_subst(MC00, "NOPE", "nope")


def test_insert_sorted(tmp_path):
    p = tmp_path / "meson.build"
    p.write_text("x = files(\n  'a.c',\n  'c.c',\n))\n", encoding="utf-8")
    assert cli.insert_sorted(p, "files(\n", "))", "  'b.c',")
    assert not cli.insert_sorted(p, "files(\n", "))", "  'b.c',")
    assert p.read_text(encoding="utf-8") == \
        "x = files(\n  'a.c',\n  'b.c',\n  'c.c',\n))\n"


def test_silicon_needs_the_owners_permission(tmp_path, monkeypatch):
    desk = tmp_path / "desk.json"
    desk.write_text('{"mc00": {"probe": "P", "port": "COM9"}}',
                    encoding="utf-8")
    monkeypatch.setattr(cli, "DESK", desk)
    with pytest.raises(cli.PeriphError, match="owner"):
        cli.cmd_silicon(MC00, "x")
