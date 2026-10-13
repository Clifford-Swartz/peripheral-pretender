# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""PORT: three flattened groups, SET/CLR/TGL aliases, IN/WRCONFIG semantics."""

import pytest

ALIASES = {"DIRCLR": ("DIR", "ALIAS_CLEAR"), "DIRSET": ("DIR", "ALIAS_SET"),
           "DIRTGL": ("DIR", "ALIAS_TOGGLE"), "OUTCLR": ("OUT", "ALIAS_CLEAR"),
           "OUTSET": ("OUT", "ALIAS_SET"), "OUTTGL": ("OUT", "ALIAS_TOGGLE")}


@pytest.fixture(scope="module")
def port(ds):
    return ds.ip("PORT").view("")


@pytest.mark.parametrize("group", range(3))
@pytest.mark.parametrize("name", sorted(ALIASES))
def test_aliases_stay_in_group(port, group, name):
    storage, flag = ALIASES[name]
    r = port.reg(f"GROUP[{group}].{name}")
    target = port.regs[r.alias_index]
    assert target.name == f"GROUP[{group}].{storage}"
    assert flag in r.flags and "PAC" in r.flags
    assert r.wmask == 0xFFFFFFFF
    assert "HOOK_POST_WRITE" in target.flags


def test_in_and_wrconfig(port):
    pin = port.reg("GROUP[2].IN")
    assert pin.flags == ["DYNAMIC", "READ_ONLY"]
    wr = port.reg("GROUP[0].WRCONFIG")
    assert {"WRITE_ONLY", "HOOK_POST_WRITE", "PAC"} <= set(wr.flags)
    assert wr.rdz == wr.impl == 0xDF47FFFF and wr.access_size == 4
    assert "HOOK_POST_WRITE" in port.reg("GROUP[1].PINCFG").flags


def test_group_constants_in_header(generated):
    gen = generated / "qemu/include/hw/mchp/gen"
    text = ((gen / "port_u2210.h").read_text() +
            (gen / "pic32cm5164jh01100/port_u2210.h").read_text())
    for line in ("#define PIC32CM5164JH01100_PORT_REGS_SIZE 0x180",
                 "#define MCHP_PORT_GROUP_STRIDE 0x80",
                 "#define MCHP_PORT_GROUP_COUNT 3",
                 "REG8(MCHP_PORT_GROUP_PMUX, 0x30)",
                 "#define MCHP_PORT_GROUP_PMUX_COUNT 16",
                 "{ .offset = 0x3F, .size = 1, .value = 0x06 }, "
                 "/* GROUP[0].PMUX[15] */"):
        assert line in text
