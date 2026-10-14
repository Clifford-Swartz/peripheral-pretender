# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""harness.nano_run (drag-and-drop on Curiosity Nano) and harness.mdb_sim
(MPLAB simulator transcript) without hardware or MPLAB X."""

import pytest

from harness import mdb_sim, nano_run


def _record(addr, kind, data=b""):
    raw = bytes([len(data)]) + addr.to_bytes(2, "big") + bytes([kind]) + data
    return ":" + (raw + bytes([-sum(raw) & 0xFF])).hex().upper()


def test_hex_ranges_follow_extended_addresses():
    text = "\n".join([_record(0, 0, b"\x0c\x94"),
                      _record(0, 4, b"\x00\x82"),      # 0x820000: fuses
                      _record(0, 0, b"\x00"),
                      _record(0, 1)])
    assert nano_run.hex_ranges(text) == [(0, 2), (0x820000, 0x820001)]


def test_check_image_refuses_fuses_and_lock_bits(tmp_path):
    image = tmp_path / "fw.hex"
    image.write_text("\n".join([_record(0, 0, b"\xff\xcf"),
                                _record(0, 4, b"\x00\x82"),
                                _record(0, 0, b"\x00"), _record(0, 1)]))
    with pytest.raises(nano_run.NanoRunError, match="refused"):
        nano_run.check_image(image, 0x10000)
    image.write_text(_record(0, 0, b"\xff\xcf") + "\n" + _record(0, 1))
    assert nano_run.check_image(image, 0x10000) == [(0, 2)]
    bad = tmp_path / "fw.elf"
    bad.write_bytes(b"\x7fELF")
    with pytest.raises(nano_run.NanoRunError, match="Intel hex"):
        nano_run.check_image(bad, 0x10000)


def test_kit_info_parsing():
    info = nano_run.parse_kit_info(
        "Kit name:              AVR64EA48 Curiosity Nano\n"
        "Kit USB serial number: MCHP3428021800000314\n"
        "Device:                AVR64EA48\n")
    assert info["Device"] == "AVR64EA48"
    assert info["Kit USB serial number"] == "MCHP3428021800000314"


LOG = """\
run
Running

wait 5000

Single breakpoint: @0xA0
Simulator halted
Stop at
\taddress:0xa0

print r18
r18=144

print r25
r25=72

continue

wait 5000

Single breakpoint: @0x112
Simulator halted
Stop at
\taddress:0x112

print r18
r18=105

print r25
r25=0

continue

wait 5000

print r18
r18=0

print r25
r25=0

halt
Simulator halted
Stop at
\taddress:0x10c
W0035-CORE: Unimplemented RAM memory access, occurred from instruction at 177.
"""


def test_mdb_log_to_transcript():
    res = mdb_sim.parse(LOG, {0xA0: 25, 0x112: 18})
    assert res.data == b"Hi"
    assert res.stops == 3
    assert res.warnings == ["W0035-CORE: Unimplemented RAM memory access, "
                            "occurred from instruction at 177."]


def test_mdb_command_file_breaks_on_every_store():
    text = mdb_sim.command_file("AVR64EA48", nano_run.Path("C:/x/fw.elf"),
                                {0xA0: 25, 0x112: 18}, 2, 100)
    assert "break *0xa0\nbreak *0x112\nrun\n" in text
    assert text.count("print r18") == 2 and text.endswith("halt\nquit\n")
