# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""ATDF parsing and device-level facts for PIC32CM5164JH01100."""

SHARED = {
    0: ["MCLK", "OSCCTRL", "OSC32KCTRL", "PAC", "PM", "SUPC"],
    9: ["SERCOM0", "SERCOM6"],
    10: ["SERCOM1", "SERCOM7"],
    20: ["TC0", "TC5"],
    21: ["TC1", "TC6"],
    22: ["TC2", "TC7"],
}


def test_counts(dev):
    assert len(dev.instances) == 55
    assert len({i.name for i in dev.instances}) == 55
    assert len(dev.modules) == 37
    assert len(dev.memory_segments) == 18
    assert sum(len(i.signals) for i in dev.instances) == 578
    assert sum(len(i.params) for i in dev.instances) == 344
    assert (len(dev.event_generators), len(dev.event_users)) == (99, 49)
    assert (dev.dfp_name, dev.dfp_version) == ("PIC32CM-JH_DFP", "1.3.221")


def test_irq_lines_and_shared_vectors(ds):
    assert [l.index for l in ds.irqs] == list(range(32))
    assert len(ds.exceptions) == 6
    shared = {l.index: l.instances for l in ds.irqs if len(l.instances) > 1}
    assert shared == SHARED
    sercom4 = ds.instance("SERCOM4")
    assert (sercom4.irq, sercom4.irq_input, sercom4.flags) == (13, 0, [])
    for line, members in SHARED.items():
        for pos, name in enumerate(members):
            inst = ds.instance(name)
            assert (inst.irq, inst.irq_input) == (line, pos)
            assert "SHARED_IRQ" in inst.flags


def test_memory_segments(dev):
    segs = {s.name: (s.start, s.size) for s in dev.memory_segments}
    assert segs["FLASH"] == (0x0, 0x80000)
    assert segs["DATAFLASH"] == (0x00400000, 0x2000)
    assert segs["USER_PAGE"] == (0x00804000, 0x100)
    assert segs["OTP5"] == (0x00806020, 0x8)
    assert segs["DRAM"] == (0x20000000, 0x10000)
    assert segs["DIVAS"] == (0x48000000, 0x20)


def test_instance_ids(ds):
    s0 = ds.instance("SERCOM0")
    assert (s0.base, s0.pac_id, s0.gclk_core, s0.gclk_slow, s0.dmac_rx,
            s0.dmac_tx) == (0x42000400, 65, 18, 17, 2, 3)
    s6 = ds.instance("SERCOM6")
    assert (s6.base, s6.pac_id, s6.gclk_core, s6.dmac_rx) == (0x43000000, 96, 24, 49)
    tc0 = ds.instance("TC0")
    assert (tc0.gclk_core, tc0.gclk_slow, tc0.dmac_rx) == (30, -1, -1)
    assert ds.instance("PORT").pac_id == 32
    assert ds.instance("DIVAS").pac_id == -1
    assert ds.instance("NVMCTRL").params["PAGES"] == "8192"
    gclk = {(r.id, r.instance, r.param) for r in ds.gclk_users}
    assert (0, "OSCCTRL", "GCLK_ID_FDPLL") in gclk
    assert (22, "SERCOM4", "GCLK_ID_CORE") in gclk
    assert len(ds.gclk_users) == 53 and len(ds.dmac_triggers) == 60


def test_instance_flags(ds):
    flags = {i.name: i.flags for i in ds.instances}
    for core in ("SystemControl", "SysTick", "NVIC", "MPU"):
        assert "CORE" in flags[core]
        assert ds.instance(core).slot_size == 0
    assert "FUSES" in flags["FUSES"]
    assert "NO_REGS" in flags["PTC"]
    assert any("7a0" in n for n in ds.instance("PTC").notes)


def test_slots_and_aliases(ds):
    port = ds.instance("PORT")
    assert (port.slot_size, port.alias_name, port.alias_base, port.alias_size) == \
        (0x400, "PORT_IOBUS", 0x60000000, 0x200)
    dsu = ds.instance("DSU")
    assert (dsu.slot_size, dsu.alias_name, dsu.alias_base, dsu.alias_size) == \
        (0x2000, "DSU_EXT", 0x41002100, 0x100)
    divas = ds.instance("DIVAS")
    assert (divas.slot_size, divas.alias_base, divas.alias_size) == \
        (0x20, 0x60000200, 0x20)
    assert ds.instance("MTB").slot_size == 0x1000
    assert ds.instance("SERCOM4").slot_size == 0x400
    mapped = sorted((i for i in ds.instances if i.slot_size), key=lambda i: i.base)
    for a, b in zip(mapped, mapped[1:]):
        assert a.base + a.slot_size <= b.base, (a.name, b.name)


def test_signals(ds):
    by_key = {(s.instance, s.group, s.index, s.pad): s for s in ds.signals}
    tx = by_key[("SERCOM4", "PAD", 2, "PB10")]
    assert (tx.function, tx.pmux, tx.pin) == ("D", 3, 32 + 10)
    led = by_key[("PORT", "P", 69, "PC05")]
    assert (led.function, led.pmux, led.pin) == ("default", -1, 64 + 5)
    gpio = [s for s in ds.signals if s.group == "P"]
    assert len(gpio) == 84 and all(s.index == s.pin for s in gpio)
    xin = [s for s in ds.signals if s.function == "XIN32"]
    assert len(xin) == 1 and xin[0].pmux == -1
    assert [s.pin for s in ds.signals] == sorted(s.pin for s in ds.signals)


def test_modes_and_selectors(ds):
    sercom = ds.ip("SERCOM")
    assert {v.mode: v.value for v in sercom.views} == {
        "USART_EXT": 0, "USART_INT": 1, "SPIS": 2, "SPIM": 3, "I2CS": 4,
        "I2CM": 5}
    sel = sercom.selector
    assert (sel.register, sel.field, sel.offset, sel.shift, sel.width) == \
        ("CTRLA", "MODE", 0, 2, 3)
    assert sercom.default_view == "USART_EXT"
    tc = ds.ip("TC")
    assert {v.mode: v.value for v in tc.views} == {"COUNT8": 1, "COUNT16": 0,
                                                    "COUNT32": 2}
    assert tc.default_view == "COUNT16"
    assert [r.size for r in (tc.view(m).reg("COUNT") for m in
                             ("COUNT8", "COUNT16", "COUNT32"))] == [1, 2, 4]
    rtc = ds.ip("RTC")
    assert [v.mode for v in rtc.views] == ["MODE0", "MODE1", "MODE2"]
    assert rtc.default_view == "MODE0"


def test_arrays_and_nested_groups(ds):
    gclk = ds.ip("GCLK").view("")
    assert (gclk.reg("GENCTRL").count, gclk.reg("PCHCTRL").count) == (9, 43)
    assert ds.ip("EVSYS").view("").reg("USER").count == 51
    port = ds.ip("PORT")
    assert port.size == 0x180
    regs = port.view("").regs
    assert len(regs) == 3 * 14
    for g in range(3):
        assert port.view("").reg(f"GROUP[{g}].DIR").offset == g * 0x80
        pmux = port.view("").reg(f"GROUP[{g}].PMUX")
        assert (pmux.offset, pmux.count, pmux.stride) == (g * 0x80 + 0x30, 16, 1)
    scope = next(c for c in port.view("").consts if c.prefix == "MCHP_PORT_GROUP")
    assert (scope.offset, scope.stride, scope.count) == (0, 0x80, 3)
