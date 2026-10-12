# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Register lists: device-description loader, text lists, selection, safety."""
import json

import pytest

from harness import regs
from harness.regs import Reg


def by_name(reg_list):
    return {r.name: r for r in reg_list}


def test_device_json_default_views_and_overrides(device_json):
    got = by_name(regs.load(device_json))
    assert got["GCLK.GENCTRL[0]"].reset == 0x106          # reset override
    assert got["GCLK.GENCTRL[1]"].reset == 0x0
    assert got["GCLK.GENCTRL[1]"].address == 0x40001C24
    assert got["SERCOM4.BAUD"].size == 2                  # USART_EXT = mode at reset
    assert got["SERCOM4.CTRLA"].ip_id == "U2201"
    assert got["SERCOM4.CTRLA"].module == "SERCOM"


def test_view_per_instance_and_per_module(device_json):
    got = by_name(regs.load(device_json, views={"SERCOM4": "SPIM"}))
    assert (got["SERCOM4.BAUD"].size, got["SERCOM5.BAUD"].size) == (1, 2)
    got = by_name(regs.load(device_json, views={"SERCOM": "SPIM"}))
    assert (got["SERCOM4.BAUD"].size, got["SERCOM5.BAUD"].size) == (1, 1)


@pytest.mark.parametrize("views, message", [
    ({"SERCOM4": "BOGUS"}, "has no view 'BOGUS'"),
    ({"SERCOM9": "SPIM"}, "names no instance or module"),
])
def test_bad_view_is_an_error(device_json, views, message):
    with pytest.raises(ValueError, match=message):
        regs.load(device_json, views=views)


def test_schema_is_checked(tmp_path, device_doc):
    device_doc["schema"] = "pp-device/0"
    path = tmp_path / "old.json"
    path.write_text(json.dumps(device_doc), encoding="utf-8")
    with pytest.raises(ValueError, match="expected 'pp-device/1'"):
        regs.load(path)


def test_invented_reset_is_unknown(device_json):
    assert by_name(regs.load(device_json))["MPU.TYPE"].reset is None


def test_markers_from_device_json(device_json):
    got = by_name(regs.load(device_json))
    assert got["WDT.CTRLA"].reset_from == ("USER_FUSES.USER_WORD_0.WDT_ENABLE",)
    assert got["GCLK.SYNCBUSY"].dynamic and not got["GCLK.SYNCBUSY"].sensitive
    assert got["SERCOM4.DATA"].dynamic
    assert got["SERCOM4.DATA"].sensitive.startswith("read pops the receive buffer")
    assert got["WDT.CLEAR"].sensitive == "write-only register"


def test_nvm_row_words(device_json):
    nvm = [r for r in regs.load(device_json) if r.module == "NVM"]
    assert [(r.name, r.address, r.reset) for r in nvm] == [
        ("NVM.CAL_WORD_0", 0x800000, 0xFFFFFFFF),
        ("NVM.CAL_WORD_1", 0x800004, 0xFFFFFFFF),
        ("NVM.USER_PAGE_WORD_0", 0x804000, 0xB15088FB),     # fuse words only
        ("NVM.USER_PAGE_WORD_1", 0x804004, 0xFFFFFFFF),
        ("NVM.SERIAL_WORD_0", 0x80A00C, 0x50500000),
    ]
    assert all(r.access == "R" for r in nvm)


def test_text_round_trip(device_json, tmp_path):
    original = regs.load(device_json, views={"SERCOM4": "SPIM"})
    path = tmp_path / "list.txt"
    path.write_text(regs.to_text(original), encoding="utf-8")
    again = regs.load(path)
    key = lambda r: (r.name, r.address, r.size, r.access, r.reset, bool(r.sensitive), r.dynamic)
    assert [key(r) for r in again] == [key(r) for r in original]


@pytest.mark.parametrize("line, message", [
    ("X.A 0x40000000 3 RW 0", "size must be 1, 2 or 4"),
    ("X.A 0x40000000 4 RO 0", "access must be R, W or RW"),
    ("X.A zz 4", "invalid literal"),
])
def test_text_errors_name_the_line(tmp_path, line, message):
    path = tmp_path / "bad.txt"
    path.write_text("# comment\n" + line + "\n", encoding="utf-8")
    with pytest.raises(ValueError, match=rf"bad.txt:2: .*{message}"):
        regs.load(path)


def test_globs_treat_brackets_literally(device_json):
    picked = regs.load(device_json, only=["GCLK.GENCTRL[0]", "SERCOM5"])
    assert [r.name for r in picked] == ["GCLK.GENCTRL[0]", "SERCOM5.CTRLA",
                                        "SERCOM5.BAUD", "SERCOM5.DATA"]
    assert [r.name for r in regs.select(picked, skip=["sercom*"])] == ["GCLK.GENCTRL[0]"]


def test_partition_skips_sensitive_and_never_forbidden():
    data = regs.classify(Reg("SERCOM4.DATA", 0x42001428, 2))
    iobus = regs.classify(Reg("PORT_IOBUS.GROUP[0].IN", 0x60000020, 4))
    plain = regs.classify(Reg("GCLK.CTRLA", 0x40001C00, 1))
    to_read, skipped = regs.partition([data, iobus, plain])
    assert to_read == [plain] and skipped == [data, iobus]
    to_read, skipped = regs.partition([data, iobus, plain], include_sensitive=True)
    assert to_read == [data, plain] and skipped == [iobus]


def test_merge_views_aliases_and_taints_overlaps():
    merged = regs.merge_views([
        Reg("SERCOM4.DATA", 0x42001428, 2, sensitive="pops"),
        Reg("SERCOM4.I2C_DATA", 0x42001428, 2),
        Reg("SERCOM4.SPI_DATA", 0x42001428, 4),
    ])
    assert merged[0].aliases == ("SERCOM4.I2C_DATA",)
    assert merged[1].sensitive == "overlaps SERCOM4.DATA: pops"


# --- the real generated description and the built-in list ------------------------

needs_device = pytest.mark.skipif(not regs.DEVICE_JSON.exists(),
                                  reason=f"{regs.DEVICE_JSON} not generated yet")


@needs_device
def test_real_device_description():
    listed = regs.load(regs.DEVICE_JSON)
    got = by_name(listed)
    assert len(got) == len(listed), "register names must be unique"
    assert len({(r.address, r.size) for r in listed}) == len(listed)
    assert got["GCLK.GENCTRL[0]"].reset == 0x00000106
    assert got["PORT.GROUP[0].PMUX[15]"].reset == 0x06
    assert got["DSU.DID"].address == 0x41002018
    assert got["NVM.SERIAL_WORD_0"].address == 0x0080A00C
    _, skipped = regs.partition(listed)
    assert {f"SERCOM{i}.DATA" for i in range(8)} <= {r.name for r in skipped}
    assert not [r for r in listed if regs.is_forbidden(r)]


BOOT_INSTANCES = ("SystemControl SysTick NVIC MPU DSU PAC PM MCLK RSTC OSCCTRL OSC32KCTRL "
                  "SUPC GCLK WDT NVMCTRL PORT SERCOM4 DIVAS NVM").split()


@needs_device
def test_boot_list_matches_device_description():
    key = lambda r: (r.name, r.address, r.size, r.access, r.reset, bool(r.sensitive), r.dynamic)
    exported = regs.load(regs.DEVICE_JSON, only=BOOT_INSTANCES, views={"SERCOM4": "USART_INT"})
    command = ("py -m harness.regs --export harness/regsets/pic32cm5164jh01100-boot.txt "
               "--view SERCOM4=USART_INT " + " ".join(f"--only {i}" for i in BOOT_INSTANCES))
    assert [key(r) for r in regs.load("boot")] == [key(r) for r in exported], \
        f"the device description changed; regenerate the boot list with: {command}"
