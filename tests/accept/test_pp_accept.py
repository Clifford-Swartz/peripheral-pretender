# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""tools.pp_accept: scenario parsing, expectations, project discovery, and
one scenario on the twin."""

import asyncio
from pathlib import Path

import pytest
import yaml

from tools.pp_accept.__main__ import check
from tools.pp_accept.build import projects
from tools.pp_accept.scenario import ScenarioResult, board_leds, parse_steps, play

ROOT = Path(__file__).resolve().parents[2]
BLINKY = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro" / "blinky.elf"


def test_parse_steps():
    assert parse_steps(r"wait 1; press ;release; send ab\r; reset") == [
        ("wait", "1"), ("press", ""), ("release", ""), ("send", "ab\r"),
        ("reset", "")]
    assert parse_steps("send  two spaces ") == [("send", " two spaces ")]
    with pytest.raises(ValueError):
        parse_steps("jump 3")
    with pytest.raises(ValueError):
        parse_steps("wait soon")


def test_check():
    res = ScenarioResult("m", "e.elf", leds={"LED0": [(0, False), (1, True)]},
                         uart="hello 42", backlog={"total": 3})
    assert check({"led_toggles": {"LED0": [1, 1]}, "uart": ["42"]}, res) == []
    fails = check({"led_toggles": {"LED0": [2, 5]}, "uart": ["x"],
                   "backlog_max": 0}, res)
    assert len(fails) == 3


def test_projects_naming(tmp_path):
    for rel in ["apps/port/port_led/firmware/kit.X",
                "apps/sercom/usart/usart_lin/master/firmware/kit.X",
                "apps/port/port_led/firmware/other_kit.X"]:
        (tmp_path / rel).mkdir(parents=True)
    assert [n for n, _ in projects(tmp_path, "kit")] == [
        "port__port_led", "sercom__usart__usart_lin__master"]


def test_projects_one_repo_per_example(tmp_path):
    """8-bit kits: example repositories side by side, some with several
    projects; configuration and toolchain from configurations.xml."""
    from tools.pp_accept.build import project_conf
    for rel in ["avr-blink/avr-blink.X/nbproject",
                "avr-filters/iir/iir.X/nbproject",
                "avr-filters/median/median.X/nbproject"]:
        (tmp_path / rel).mkdir(parents=True)
    (tmp_path / "avr-blink/avr-blink.X/nbproject/configurations.xml").write_text(
        '<conf name="free" type="2"><languageToolchain>XC8</languageToolchain>')
    found = projects(tmp_path, "avr64ea48_curiosity_nano")
    assert [n for n, _ in found] == ["avr-blink", "avr-filters__iir",
                                     "avr-filters__median"]
    assert project_conf(found[0][1]) == ("free", "XC8")
    assert project_conf(found[1][1]) == ("default", "XC32")


def test_board_leds():
    assert [(l.id, l.pin) for l in board_leds("avr64ea48-cnano,mb1=8x8g")] == [
        ("LED0", "PB3")]
    assert [(l.id, l.pin) for l in board_leds("pic32cm-jh01-cpro")] == [
        ("LED0", "PC05")]
    assert [(l.id, l.pin) for l in board_leds("pic32cm-mc00-cpro,ext1=thermo8")] == [
        ("LED0", "PA24")]


def test_acceptance_files_are_well_formed():
    for path in (ROOT / "acceptance").glob("*.yaml"):
        suite = yaml.safe_load(path.read_text(encoding="utf-8"))
        assert suite["machine"] and suite["kit"]
        for name, ex in suite["examples"].items():
            parse_steps(ex.get("steps", "wait 2"))
            assert ex["verdict"] in ("pass", "gap", "nt"), name
            assert ex.get("note") or ex["verdict"] == "pass", name


@pytest.mark.qemu
def test_scenario_on_the_twin(qemu_path, qemu_machines):
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not BLINKY.is_file():
        pytest.skip("blinky.elf not built")
    res = asyncio.run(play(BLINKY, "pic32cm-mc00-cpro",
                           parse_steps("wait 1.2; press; wait 1; release")))
    assert "SW0 pressed" in res.uart
    assert res.toggles("LED0") >= 8          # 2 Hz, then 8 Hz while held
    assert res.backlog.get("total", 0) == 0
