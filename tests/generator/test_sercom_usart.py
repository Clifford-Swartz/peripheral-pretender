# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""SERCOM USART_INT table == the research table in docs/research/microchip-data.md.

Two sources from that document are parsed, not copied:
  * "Table: SERCOM USART_INT registers" (offsets, sizes, resets, prose rules)
  * the prototype's MchpRegInfo table in "Generated output (2/3)" (masks)
"""

import re

import pytest

from gen_support import RESEARCH

COLUMNS = ("name", "offset", "size", "reset", "impl", "wmask", "w1c", "rdz",
           "enprot", "syncbusy")

# Rules stated in prose in the research table's access column, as
# (phrase that must appear in the row, field names it names).
PROSE = {
    "CTRLA": [("enable-protected except SWRST/ENABLE", "enprot_except",
               ["SWRST", "ENABLE"]),
              ("SYNCBUSY.SWRST/ENABLE", "sync", ["SWRST", "ENABLE"]),
              ("SWRST self-clearing", "rdz", ["SWRST"])],
    "CTRLB": [("enable-protected except RXEN/TXEN", "enprot_except",
               ["RXEN", "TXEN"]),
              ("LINCMD strobe reads 0", "rdz", ["LINCMD"])],
    "INTFLAG": [("DRE, RXC read-only", "ro", ["DRE", "RXC"]),
                ("TXC, RXS, CTSIC, RXBRK, ERROR W1C", "w1c",
                 ["TXC", "RXS", "CTSIC", "RXBRK", "ERROR"])],
    "STATUS": [("PERR, FERR, BUFOVF, ISF, COLL, TXE W1C", "w1c",
                ["PERR", "FERR", "BUFOVF", "ISF", "COLL", "TXE"]),
               ("CTS read-only", "ro", ["CTS"])],
}


def _section(text: str, title: str) -> str:
    start = text.index(title)
    end = text.find("\n## ", start + 1)
    return text[start:end if end > 0 else None]


@pytest.fixture(scope="module")
def research() -> str:
    return RESEARCH.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def doc_rows(research) -> dict[str, list[str]]:
    sec = _section(research, "## Table: SERCOM USART_INT registers")
    rows = {}
    for line in sec.splitlines()[2:]:
        if line.startswith("|"):
            cells = [c.strip() for c in line.strip("|").split("|")]
            rows[cells[0]] = cells
    return rows


@pytest.fixture(scope="module")
def proto_rows(research) -> dict[str, dict]:
    sec = _section(research, "## Code: Generated output (2/3)")
    rows = {}
    for m in re.finditer(r'\{ "(\w+)",\s*([^}]*)\}', sec):
        values = [v.strip() for v in m.group(2).split(",")]
        row = {"name": m.group(1)}
        for key, v in zip(COLUMNS[1:], values):
            row[key] = int(v, 0)
        row["flags"] = set(re.findall(r"MCHP_F_(\w+)", values[-1]))
        rows[row["name"]] = row
    return rows


@pytest.fixture(scope="module")
def usart(ds):
    return ds.ip("SERCOM").view("USART_INT")


def test_research_tables_parsed(doc_rows, proto_rows):
    assert len(doc_rows) == 12 and len(proto_rows) == 12
    assert list(doc_rows) == list(proto_rows)


def test_offsets_sizes_resets_match_doc(usart, doc_rows):
    assert [r.name for r in usart.regs] == list(doc_rows)
    for r in usart.regs:
        cells = doc_rows[r.name]
        assert r.offset == int(cells[1], 0), r.name
        assert r.size == int(cells[2]), r.name
        assert r.reset == int(cells[4], 0), r.name
        impl = re.search(r"\(impl (0x[0-9A-Fa-f]+)\)", cells[5])
        if impl:
            assert r.impl == int(impl.group(1), 0), r.name


def test_masks_match_prototype_table(usart, proto_rows):
    for r in usart.regs:
        row = proto_rows[r.name]
        got = {key: getattr(r, key) for key in COLUMNS[1:]}
        want = {key: row[key] for key in COLUMNS[1:]}
        assert got == want, r.name
        # The prototype had no READ_ONLY/WRITE_ONLY flags (spec 5.1 adds them).
        assert set(r.flags) - {"READ_ONLY", "WRITE_ONLY"} == row["flags"], r.name


@pytest.mark.parametrize("reg", sorted(PROSE))
def test_prose_rules(usart, doc_rows, reg):
    r = usart.reg(reg)
    fields = r.field_union
    for phrase, kind, names in PROSE[reg]:
        assert phrase in doc_rows[reg][3], f"research row {reg} changed: {phrase}"
        mask = 0
        for n in names:
            mask |= fields[n]
        if kind == "enprot_except":
            assert r.enprot == r.wmask & ~mask
        elif kind == "sync":
            sb = {f.name: f.mask for f in usart.reg("SYNCBUSY").fields}
            assert r.syncbusy == sum(sb[n] for n in names)
        elif kind == "ro":
            assert r.ro & mask == mask and not r.wmask & mask and not r.w1c & mask
        else:
            assert getattr(r, kind) & mask == mask


def test_enable_protected_whole_registers(usart, doc_rows):
    for name in ("CTRLC", "BAUD", "RXPL"):
        assert "enable-protected" in doc_rows[name][3]
        r = usart.reg(name)
        assert r.enprot == r.wmask == r.impl


def test_ctrlb_sync_and_bus_error(usart, doc_rows):
    assert "synced via SYNCBUSY.CTRLB" in doc_rows["CTRLB"][3]
    ctrlb = usart.reg("CTRLB")
    assert ctrlb.syncbusy == 0x4 and "BUSY_BUSERR" in ctrlb.flags


def test_intenset_storage_alias(usart):
    regs = usart.regs
    clr, setr = usart.reg("INTENCLR"), usart.reg("INTENSET")
    assert regs[clr.alias_index] is setr and regs[setr.alias_index] is setr
    assert clr.flags == ["PAC", "ALIAS_CLEAR"] and setr.flags == ["PAC", "ALIAS_SET"]


def test_usart_ext_shares_rules(ds, usart):
    ext = ds.ip("SERCOM").view("USART_EXT")
    for a, b in zip(ext.regs, usart.regs):
        assert (a.name, a.offset, a.w1c, a.wmask, a.enprot, a.flags) == \
            (b.name, b.offset, b.w1c, b.wmask, b.enprot, b.flags)
