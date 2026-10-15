# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Overlay loading, version guard and name validation."""

import shutil

import pytest
import yaml

from gen_support import OVERLAYS
from mchpgen import overlay, semantics
from mchpgen.overlay import OverlayError

EXPECTED = {("SERCOM", "U2201"), ("PORT", "U2210"), ("OSCCTRL", "U2119"),
            ("OSC32KCTRL", "U2246"), ("GCLK", "U2122"), ("MCLK", "U2234"),
            ("NVMCTRL", "03929"), ("SUPC", "U2117"), ("PM", "U2240"),
            ("RSTC", "U2239"), ("WDT", "U2251"), ("PAC", "U2120"),
            ("DSU", "U2209"), ("DIVAS", "U2258"), ("EVSYS", "U2256"),
            # reset values only (from silicon)
            ("MTB", "U2002"), ("HMATRIXB", "I7638"),
            ("EIC", "U2254"), ("TC", "U2249"), ("RTC", "U2250"),
            ("TCC", "U2213"),
            # AVR64EA48 (AVR8X; AVR ATDF modules have no version)
            ("USART", "uart_autobd_v4"), ("CPUINT", "int_8bit_v3"),
            ("CLKCTRL", "avrea"), ("NVMCTRL", "nvm_ctrl_avr_v3"),
            ("SPI", "spi_8bit_v2"), ("TCB", "tmr_16b_capture_v1"),
            ("TWI", "i2c_8bit_avr_v3"),
            # ATSAME51J20A (SAM E51: its own clock, NVM, QSPI, TRNG, USB IPs)
            ("OSCCTRL", "U2401"), ("OSC32KCTRL", "U2400"), ("MCLK", "U2408"),
            ("NVMCTRL", "U2409"), ("QSPI", "U2008"), ("TRNG", "U2242"),
            ("USB", "U2222"), ("DMAC", "U2503"),
            ("ADC", "U2500"), ("DAC", "U2502"), ("EVSYS", "U2504"),
            # PIC32CM JH01 and MC00 ADC and DAC
            ("ADC", "U2247"), ("DAC", "U2214")}


def edited(tmp_path, name, change):
    """Copy overlays/<name>, apply change(dict) and load the copy."""
    data = yaml.safe_load((OVERLAYS / name).read_text(encoding="utf-8"))
    change(data)
    path = tmp_path / name
    path.write_text(yaml.safe_dump(data), encoding="utf-8")
    return overlay.load(str(path))


def build(dev, ov):
    return semantics.build_ip(dev, dev.modules[ov.ip], {(ov.ip, ov.id): ov})


def test_all_spec_overlays_present_and_applied(ds_all, overlays):
    """Every overlay applies on each device that has its IP, and every
    overlaid IP exists on at least one device."""
    assert set(overlays) == EXPECTED
    for ip, ip_id in EXPECTED:
        found = [d for d in ds_all
                 if ip in d.device.modules and d.device.modules[ip].id == ip_id]
        assert found, ip
        for d in found:
            assert d.ip(ip).overlay_status == "applied", (d.device.name, ip)


def test_version_guard_falls_back_to_heuristics(dev, tmp_path):
    ov = edited(tmp_path, "sercom_u2201.yaml",
                lambda d: d.update(versions=["2.0.0"]))
    ip = build(dev, ov)
    assert ip.overlay_status == "version-mismatch"
    assert any("NOT applied" in d.msg for d in ip.diags if d.level == "warning")
    intflag = ip.view("USART_INT").reg("INTFLAG")
    assert intflag.w1c == 0xBF                     # ATDF atomic-op heuristic
    assert ip.view("USART_INT").reg("STATUS").w1c == 0


def test_unknown_register_is_an_error(dev, tmp_path):
    ov = edited(tmp_path, "gclk_u2122.yaml",
                lambda d: d["registers"].update(GENCTRLX={"pac_protected": True}))
    with pytest.raises(OverlayError, match="GENCTRLX"):
        build(dev, ov)


def test_unknown_field_is_an_error(dev, tmp_path):
    def change(d):
        d["modes"]["USART_INT"]["registers"]["INTFLAG"]["w1c"].append("DREX")
    ov = edited(tmp_path, "sercom_u2201.yaml", change)
    with pytest.raises(OverlayError, match="DREX"):
        build(dev, ov)


def test_unknown_sync_bit_is_an_error(dev, tmp_path):
    ov = edited(tmp_path, "oscctrl_u2119.yaml",
                lambda d: d["registers"]["OSC48MDIV"].update(
                    sync="OSC48MSYNCBUSY.NOPE"))
    with pytest.raises(OverlayError, match="NOPE"):
        build(dev, ov)


def test_unknown_reset_override_target_is_an_error(dev, tmp_path):
    ov = edited(tmp_path, "port_u2210.yaml",
                lambda d: d["reset_overrides"].update({"GROUP[0].PMUXX[1]": 1}))
    with pytest.raises(OverlayError, match="PMUXX"):
        build(dev, ov)


def test_unknown_key_is_an_error(tmp_path):
    with pytest.raises(OverlayError, match="unknown keys"):
        edited(tmp_path, "pm_u2240.yaml",
               lambda d: d["registers"]["SLEEPCFG"].update(w1s=["X"]))


def test_file_name_must_match_ip_and_id(tmp_path):
    shutil.copy(OVERLAYS / "pm_u2240.yaml", tmp_path / "pm.yaml")
    with pytest.raises(OverlayError, match="pm_u2240.yaml"):
        overlay.load(str(tmp_path / "pm.yaml"))


def test_modes_must_match_atdf(dev, tmp_path):
    def change(d):
        d["registers"] = d.pop("modes")["USART_INT"]["registers"]
    ov = edited(tmp_path, "sercom_u2201.yaml", change)
    with pytest.raises(OverlayError, match="modes"):
        build(dev, ov)


def test_atdf_conflicts_are_reported(ds):
    notes = [d.msg for d in ds.ip("SERCOM").diags if d.level == "info"]
    assert any("INTFLAG: ATDF atomic-op=clear:INTFLAG implies w1c=0xBF, "
               "overlay says 0xBA" in n for n in notes)
