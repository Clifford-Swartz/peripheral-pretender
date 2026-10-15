# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""tools.pp.fatfs (CIRCUITPY images) and tools.pp.uf2 (UF2 to ELF)."""

import struct

import pytest

from tools.pp import fatfs, uf2


def test_format_matches_circuitpython_layout():
    """The geometry CircuitPython 10.1.4 gives a 4 MiB SST26VF032B (read
    from the image it formatted on the twin): 8184 sectors, 4 per
    cluster, one FAT of 6 sectors, 512 root entries, FAT12."""
    data = fatfs.format_image(4 << 20)
    bps, spc, rsv, nfat, nroot, tot16, media, fatsz = struct.unpack(
        "<HBHBHHBH", data[11:24])
    assert (bps, spc, rsv, nfat, nroot, tot16, media, fatsz) == (
        512, 4, 1, 1, 512, 8184, 0xF8, 6)
    assert data[510:512] == b"\x55\xaa"
    fat = fatfs.FatImage(data)
    assert fat.bits == 12
    assert fat.nclusters == 2036          # os.statvfs on the kit: 2036
    assert list(fat.walk()) == []


def test_files_round_trip():
    files = {
        "code.py": b"print('hello')\n",
        "lib/adafruit_st7789.mpy": bytes(range(256)) * 3,
        "API/a_long_module_name.py": b"x = 1\n" * 1000,
        "Images/Meatball_32x30_16color.bmp": b"BM" + bytes(5000),
    }
    data = fatfs.build_image(files)
    fat = fatfs.FatImage(data)
    for path, content in files.items():
        assert fat.read(path) == content
        assert fat.read("/" + path.upper().replace("/", "/")) == content
    names = {p for p, e in fat.walk() if not e.is_dir}
    assert names == set(files)
    # 8.3 names stay short; long ones get VFAT entries with a ~1 short name
    shorts = {e.name: e.short for _, e in fat.walk()}
    assert shorts["code.py"] == b"CODE    PY "
    assert shorts["a_long_module_name.py"].startswith(b"A_LONG~1")


def test_overwrite_and_remove_free_space():
    data = fatfs.build_image({"code.py": b"a" * 10000})
    fat = fatfs.FatImage(data)
    free = fat.free_bytes()
    fat.write("code.py", b"b" * 3000)
    assert fat.read("code.py") == b"b" * 3000
    assert fat.free_bytes() > free
    fat.remove("code.py")
    assert fat.lookup("code.py") is None
    assert fat.free_bytes() == fatfs.FatImage(
        fatfs.format_image()).free_bytes()


def test_blank_flash_is_not_a_volume():
    with pytest.raises(fatfs.FatError):
        fatfs.FatImage(bytearray(b"\xff" * (4 << 20)))


def _uf2_block(addr, payload, n, total):
    blk = struct.pack("<8I", uf2.UF2_MAGIC0, uf2.UF2_MAGIC1,
                      uf2.UF2_FLAG_FAMILY, addr, len(payload), n, total,
                      0x55114460)
    blk += payload.ljust(476, b"\0") + struct.pack("<I", uf2.UF2_MAGIC_END)
    return blk


def test_uf2_to_elf():
    vectors = struct.pack("<II", 0x20040000, 0x4101)
    body = vectors + bytes(range(256)) * 2
    blocks = [body[i:i + 256] for i in range(0, len(body), 256)]
    data = b"".join(_uf2_block(0x4000 + 256 * i, b, i, len(blocks))
                    for i, b in enumerate(blocks))
    parsed, families = uf2.parse(data)
    assert families == {0x55114460}
    segs = uf2.segments(parsed)
    assert segs == [(0x4000, body)]
    elf = uf2.to_elf(segs)
    assert elf[:4] == b"\x7fELF"
    e_entry, = struct.unpack_from("<I", elf, 24)
    assert e_entry == 0x4101
    ptype, poff, vaddr, _, fsz = struct.unpack_from("<5I", elf, 52)
    assert (ptype, vaddr, fsz) == (1, 0x4000, len(body))
    assert elf[poff:poff + fsz] == body


def test_uf2_rejects_other_files():
    with pytest.raises(uf2.Uf2Error):
        uf2.parse(b"\0" * 512)
