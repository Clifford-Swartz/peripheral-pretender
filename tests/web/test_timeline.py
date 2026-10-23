# Copyright (c) 2026 Clifford Swartz.
"""The event recorder's stream, its decoders, the board timeline and the
injected faults (tools/pp/trace.py, tools/pp_web/timeline.py)."""

import asyncio
import struct
from pathlib import Path

import pytest

from tools.pp import trace as tr
from tools.pp_web import timeline as tl
from tools.pp_web.server import App
from tools.pp_web.session import Session

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "firmware" / "hello" / "build" / "pic32cm_mc00_cpro"
THERMO = BUILD / "thermo.elf"
TC = BUILD / "tc.elf"
AVR_MATRIX = ROOT / "firmware" / "avr-hello" / "build" / "matrix.elf"
PYKIT_ELF = (Path.home() / ".pp_cache" / "circuitpython"
             / "microchip_curiosity_circuitpython-10.1.4.elf")
MS = 1_000_000


def rec(t, kind, src=0, a=0, b=0) -> bytes:
    return struct.pack("<QBBHI", t, kind, src, a, b)


def name(src, text) -> bytes:
    data = text.encode()
    return rec(0, tr.NAME, src, len(data)) + data + bytes(-len(data) % 16)


def store_of(*chunks: bytes) -> tr.Store:
    s = tr.Store()
    for c in chunks:
        s.feed(c)
    return s


def test_parser_names_meta_and_split_records():
    stream = (rec(0, tr.HELLO, a=1) + name(0, "PORT") + name(1, "NVIC") +
              name(2, "sercom2") + rec(5, tr.PAD, 0, 24, 1) +
              rec(9, tr.TIME) + rec(10, tr.LOST, b=3) +
              rec(12, tr.I2C_STOP, 2))
    p = tr.Parser()
    out = []
    for i in range(0, len(stream), 7):      # any split of the stream
        out += p.feed(stream[i:i + 7])
    assert p.version == 1 and p.lost == 3 and p.now == 12
    assert p.names == {0: "PORT", 1: "NVIC", 2: "sercom2"}
    assert out == [(5, tr.PAD, 0, 24, 1), (12, tr.I2C_STOP, 2, 0, 0)]


def test_store_levels_chunks_and_order(monkeypatch):
    monkeypatch.setattr(tr, "CHUNK", 4)
    s = tr.Store(max_records=12)            # three chunks of four
    for t in range(0, 40, 2):
        s.add(t * MS, tr.PAD, 0, 5, (t // 2) % 2)
    assert len(s.chunks) == 3 and s.start == 16 * MS   # the oldest went
    assert s.level_at(5, 31 * MS) == 1 and s.level_at(5, 33 * MS) == 0
    assert [r[0] for r in s.records(20 * MS, 26 * MS)] == \
        [20 * MS, 22 * MS, 24 * MS, 26 * MS]
    # A busy window stamped before records already kept still sorts in
    s.add(37 * MS, tr.PAD_BUSY, 0, 6, 10)
    assert [r[1] for r in s.records(36 * MS, 38 * MS)] == \
        [tr.PAD, tr.PAD_BUSY, tr.PAD]


def test_i2c_transactions_repeated_start_and_nacks():
    s = store_of(name(2, "sercom2"),
                 rec(1 * MS, tr.I2C_START, 2, 0x18, 0),        # write
                 rec(1 * MS + 1, tr.I2C_BYTE, 2, 0x05, 0),
                 rec(1 * MS + 2, tr.I2C_START, 2, 0x18, 1),    # Sr read
                 rec(1 * MS + 3, tr.I2C_BYTE, 2, 0xC1, 2),
                 rec(1 * MS + 4, tr.I2C_BYTE, 2, 0x90, 2),
                 rec(1 * MS + 5, tr.I2C_HOST_NACK, 2),
                 rec(1 * MS + 6, tr.I2C_STOP, 2),
                 rec(2 * MS, tr.I2C_START, 2, 0x48, 2),        # NACKed
                 rec(2 * MS + 1, tr.I2C_STOP, 2),
                 rec(3 * MS, tr.I2C_START, 2, 0x18, 0),
                 rec(3 * MS + 1, tr.I2C_BYTE, 2, 0x01, 1))     # not stopped
    txs = tr.i2c(s.window(0, 4 * MS), s.source("SERCOM2"))
    assert [t["text"] for t in txs] == [
        "0x18 W 05 | 0x18 R C1 90", "0x48 W NACK", "0x18 W 01 NACK ..."]
    assert txs[0]["host_nack"] and txs[0]["end"] == 1 * MS + 6
    assert txs[2]["end"] is None


def test_uart_runs_split_at_newlines_pauses_and_direction():
    s = store_of(name(2, "sercom0"),
                 *[rec(i * 100_000, tr.UART_TX, 2, c)
                   for i, c in enumerate(b"Hi\r\nOK")],
                 rec(5 * MS, tr.UART_TX, 2, ord("!")),           # a pause
                 rec(5 * MS + 1, tr.UART_RX, 2, 0x07))
    runs = tr.uart(s.window(0, 6 * MS), 2)
    assert [(r["dir"], r["text"]) for r in runs] == [
        ("tx", "Hi\r\n"), ("tx", "OK"), ("tx", "!"), ("rx", "\\x07")]


def test_spi_transfers_by_gap_or_chip_select():
    s = store_of(name(2, "sercom1"),
                 rec(0, tr.PAD, 0, 17, 0),
                 rec(10, tr.SPI, 2, 0xAA, 0xFF), rec(20, tr.SPI, 2, 0x55, 1),
                 rec(30, tr.PAD, 0, 17, 1), rec(40, tr.PAD, 0, 17, 0),
                 rec(50, tr.SPI, 2, 0x01, 2),
                 rec(5 * MS, tr.SPI, 2, 0x02, 3))
    w = s.window(0, 6 * MS)
    assert [t["mosi"] for t in tr.spi(w, 2)] == [[0xAA, 0x55, 0x01], [0x02]]
    assert [t["mosi"] for t in tr.spi(w, 2, cs=17)] == \
        [[0xAA, 0x55], [0x01, 0x02]]


def test_irq_spans_nest_and_carry_over():
    s = store_of(rec(1 * MS, tr.IRQ_PEND, 1, 32), rec(1 * MS + 5, tr.IRQ_ENTER, 1, 32),
                 rec(1 * MS + 9, tr.IRQ_ENTER, 1, 15),       # SysTick on top
                 rec(1 * MS + 20, tr.IRQ_EXIT, 1, 15),
                 rec(1 * MS + 30, tr.IRQ_EXIT, 1, 32),
                 rec(9 * MS, tr.IRQ_ENTER, 1, 32))           # still running
    spans = tr.irqs(s.window(1 * MS + 10, 10 * MS))
    assert spans[15]["spans"] == [[1 * MS + 10, 1 * MS + 20]]   # clipped
    assert spans[32]["spans"] == [[1 * MS + 10, 1 * MS + 30], [9 * MS, None]]
    assert tr.exception_name(15, None) == "SysTick"
    assert tr.exception_name(18, tl.device("pic32cm1216mc00048")) == "RTC"


def test_pad_trace_squeezes_fast_edges_and_keeps_busy_windows():
    edges = [rec(1000 * i, tr.PAD, 0, 10, i % 2) for i in range(1, 21)]
    s = store_of(rec(0, tr.PAD, 0, 10, 0), *edges,
                 rec(50_000, tr.PAD, 0, 10, 0),
                 rec(2 * MS, tr.PAD_BUSY, 0, 11, 300 | 128 << 24))
    w = s.window(500, 3 * MS)
    raw = tr.pad_trace(w, [10])["PA10"]
    assert raw["level"] == 0 and len(raw["edges"]) == 21
    fine = tr.pad_trace(w, [10, 11], resolution=5000)
    # high 1 us in every 2, from the first edge to the last (10 of 19 us)
    assert fine["PA10"]["busy"] == [[1000, 20000, 0.526]]
    assert fine["PA10"]["edges"] == [[20000, 0], [50000, 0]]
    assert fine["PA11"]["busy"] == [[2 * MS, 3 * MS, 0.502]]


def test_digests_say_where_two_runs_part():
    a = store_of(rec(1 * MS, tr.PAD, 0, 5, 1), rec(12 * MS, tr.UART_TX, 2, 65),
                 rec(25 * MS, tr.INPUT, 0, 7, 0))
    b = store_of(rec(1 * MS, tr.PAD, 0, 5, 1), rec(12 * MS, tr.UART_TX, 2, 66),
                 rec(25 * MS, tr.INPUT, 0, 7, 0))
    da, db = tr.digests(a, 30 * MS), tr.digests(b, 30 * MS)
    assert [d[0] for d in da] == [0, 10 * MS, 20 * MS]
    assert a.inputs == [(25 * MS, 7, True)]
    assert tr.compare(da, da) == {"same": True, "from": 0,
                                  "differs_at": None, "events": 3}
    assert tr.compare(da, db)["differs_at"] == 10 * MS


def test_bursts_merge_a_busy_bus():
    items = [{"t": i, "end": i} for i in range(1000)]
    out = tr.bursts(items, 10)
    assert len(out) == 10 and out[0] == {"t": 0, "end": 99, "count": 100}


def test_suggested_rows_name_the_board_pads():
    app = App("ev15n46a", None)
    t = app.twins[0]
    t.clicks["ext1"] = {"id": "thermo8", "values": {}}
    rows = tl.suggest(t.board, t.clicks, t.wired(), None)
    assert [r["label"] for r in rows][:3] == ["LED0 PA24", "SW0 PA25",
                                              "EXT1 RST PA20"]
    s = store_of(name(2, "sercom0"), name(3, "sercom2"),
                 rec(1, tr.UART_TX, 2, 65), rec(2, tr.I2C_START, 3, 0x18),
                 rec(3, tr.IRQ_ENTER, 1, 18))
    labels = [r["label"] for r in tl.suggest(t.board, t.clicks, {}, s)]
    assert "SERCOM0 UART (VCOM)" in labels and "SERCOM2 I2C" in labels
    assert labels[-1] == "RTC"


def test_avr_pads_and_vectors_by_the_hello():
    """An AVR stream says 8 pads a group and vectors: PB3, "TCB0_INT"; a
    row's pad may be written either way, and a pad the part lacks is
    empty, not an error"""
    assert tr.pad_name(42) == "PB10" and tr.pad_number("pb10") == 42
    assert tr.pad_name(11, 8) == "PB3" and tr.pad_number("PB3", 8) == 11
    s = store_of(rec(0, tr.HELLO, a=1, b=8 | tr.VECTORS), name(0, "PORT"),
                 name(1, "NVIC"), name(3, "usart1"),
                 rec(0, tr.PAD, 0, 11, 1),
                 rec(5_000, tr.PAD, 0, 11, 0), rec(9_000, tr.PAD, 0, 11, 1),
                 rec(10_000, tr.UART_TX, 3, 65),
                 rec(12_000, tr.IRQ_ENTER, 1, 13),
                 rec(13_000, tr.IRQ_EXIT, 1, 13))
    assert s.group_pins == 8 and s.vectors
    assert s.activity()["pads"] == ["PB3"]
    dev = tl.device("avr64ea48")
    assert tr.exception_name(13, dev, vectors=True) == "TCB0_INT"
    assert tr.exception_name(99, dev, vectors=True) == "vector 99"
    board = {"device": "avr64ea48", "vcom": "USART1 (PC0/PC1), nEDBG",
             "leds": [{"id": "LED0", "pin": "PB3"}], "buttons": []}
    assert tl.vcom_source(board) == "usart1"
    labels = [r["label"] for r in tl.suggest(board, {}, {}, s)]
    assert labels == ["LED0 PB3", "USART1 UART (VCOM)", "TCB0_INT"]
    rows = [{"id": "a", "kind": "pad", "pad": "PB03", "label": ""},
            {"id": "b", "kind": "pad", "pad": "PZ1", "label": ""}]
    out = tl.build(s, rows, 1_000, 30_000)
    assert out[0]["data"]["level"] == 1
    assert out[0]["data"]["edges"] == [[5_000, 0], [9_000, 1]]
    assert out[1]["data"] is None


def test_faults_are_checked():
    app = App("ev15n46a", None)
    t = app.twins[0]
    t.clicks["ext1"] = {"id": "thermo8", "values": {}}
    assert t.check_fault({"kind": "short", "pad": "pa24", "level": "vdd"}) \
        == {"kind": "short", "pad": "PA24", "level": 1}
    assert t.check_fault({"kind": "nack", "slot": "ext1"}) == \
        {"kind": "nack", "addr": 0x18, "slot": "ext1"}
    assert t.check_fault({"kind": "nack", "addr": "48"})["addr"] == 0x48
    assert t.check_fault({"kind": "miso", "sercom": "SERCOM1",
                          "value": "0"}) == \
        {"kind": "miso", "host": "sercom1", "value": 0}
    for bad in ({"kind": "short", "pad": "PZ99"},
                {"kind": "nack", "slot": "ext2"},       # nothing there
                {"kind": "nack", "addr": 200},
                {"kind": "miso", "sercom": "sercom9"},
                {"kind": "melt"}):
        with pytest.raises(ValueError):
            t.check_fault(bad)
    # The AVR's bus hosts are its SPI and TWI
    a = App("ev66e56a", None).twins[0]
    assert a.hosts == {"spi": ["spi0"], "i2c": ["twi0"]}
    assert a.check_fault({"kind": "miso", "host": "SPI0", "value": "1a5"}) \
        == {"kind": "miso", "host": "spi0", "value": 0xA5}
    assert a.check_fault({"kind": "short", "pad": "pb3"})["pad"] == "PB3"
    for bad in ({"kind": "miso", "host": "sercom1"},
                {"kind": "short", "pad": "PB9"}):
        with pytest.raises(ValueError):
            a.check_fault(bad)


@pytest.mark.qemu
def test_recorder_sees_i2c_and_interrupts(qemu_path, qemu_machines):
    """The Thermo 8 demo's I2C reads and the TC demo's interrupts reach the
    store, decoded."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file() or not TC.is_file():
        pytest.skip("thermo.elf / tc.elf not built")
    if not tr.machine_has_trace(qemu_path, "pic32cm-mc00-cpro",
                                __import__("tools.pp.qemu", fromlist=[
                                    "qemu_env"]).qemu_env()):
        pytest.skip("QEMU build has no event recorder")

    async def run(machine, elf, seconds):
        s = Session(machine, elf, lambda m: None)
        await s.start()
        try:
            await asyncio.sleep(seconds)
        finally:
            await s.stop()
        return s.trace_store

    store = asyncio.run(run("pic32cm-mc00-cpro,ext1=thermo8", THERMO, 1.5))
    act = store.activity()
    assert act["buses"]["sercom2"] == ["i2c"]
    assert "uart" in act["buses"]["sercom0"]
    txs = tr.i2c(store.window(0, store.now), store.source("sercom2"))
    assert txs and all(seg["addr"] == 0x18 for tx in txs
                       for seg in tx["segments"])
    assert any(not tx["segments"][0]["nack"] for tx in txs)
    store = asyncio.run(run("pic32cm-mc00-cpro", TC, 1.5))
    spans = tr.irqs(store.window(0, store.now))
    assert spans and all(s_["spans"] for s_ in spans.values())


@pytest.mark.qemu
def test_faults_reach_the_twin(qemu_path, qemu_machines):
    """A NACKed Thermo 8 shows as NACKed transactions; LED0's pad shorted
    to GND lights it; clearing restores both."""
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file():
        pytest.skip("thermo.elf not built")

    async def scenario():
        app = App("ev15n46a", None)
        app.emit = lambda m: None
        t = app.twins[0]
        t.clicks["ext1"] = {"id": "thermo8", "values": {"temperature": 25}}
        await t.run(THERMO)
        try:
            if not t.session.trace_store:
                pytest.skip("QEMU build has no event recorder")
            await asyncio.sleep(1.0)
            await t.set_faults([{"kind": "nack", "slot": "ext1"},
                                {"kind": "short", "pad": "PA24"}])
            mark = t.session.trace_store.now
            await asyncio.sleep(1.5)
            store = t.session.trace_store
            faulted = tr.i2c(store.window(mark, store.now),
                             store.source("sercom2"))
            lit = t.summary()["leds"][0]["on"]
            await t.set_faults([])
            mark = store.now
            await asyncio.sleep(1.5)
            healed = tr.i2c(store.window(mark, store.now),
                            store.source("sercom2"))
            dark = t.summary()["leds"][0]["on"]
        finally:
            await t.stop()
        return faulted, lit, healed, dark

    faulted, lit, healed, dark = asyncio.run(scenario())
    assert faulted and all(tx["segments"][0]["nack"] for tx in faulted)
    assert healed and not healed[-1]["segments"][0]["nack"]
    assert lit is True and dark is False


@pytest.mark.qemu
def test_record_and_replay_exactly(qemu_path, qemu_machines):
    """A recorded run (a slider moved, SW0 pressed, text on the console,
    each when the host got to it) replays event for event; a recording
    with an input moved replays differently, and the check says so."""
    import json
    if "pic32cm-mc00-cpro" not in qemu_machines:
        pytest.skip("QEMU build has no pic32cm-mc00-cpro machine")
    if not THERMO.is_file():
        pytest.skip("thermo.elf not built")

    async def scenario():
        app = App("ev15n46a", None)
        app.emit = lambda m: None
        t = app.twins[0]
        t.clicks["ext1"] = {"id": "thermo8", "values": {"temperature": 25}}
        await t.run(THERMO)
        if not t.session.trace_store:
            pytest.skip("QEMU build has no event recorder")
        await t.record_start()
        try:
            await asyncio.sleep(0.5)
            await t.control("ext1", "temperature", 35)
            await asyncio.sleep(0.3)
            await t.session.press("sw0", True)
            await asyncio.sleep(0.2)
            await t.session.press("sw0", False)
            await t.session.uart_send("hi\r\n")
            await asyncio.sleep(0.5)
            saved = t.record_stop("demo")
            verdicts = []
            for name in ("demo", "moved"):
                if name == "moved":     # the slider an instant later
                    rec = json.loads(Path(saved["path"]).read_text())
                    rec["name"], rec["inputs"][0][0] = "moved", \
                        rec["inputs"][0][0] + 1_000_000
                    (t.recordings_dir() / "moved.json").write_text(
                        json.dumps(rec))
                await t.replay(name, pace=False)
                for _ in range(300):
                    await asyncio.sleep(0.1)
                    if t.replaying["state"] != "running":
                        break
                verdicts.append(dict(t.replaying))
        finally:
            await t.stop()
        return saved, verdicts

    saved, (same, moved) = asyncio.run(scenario())
    assert saved["inputs"] == 4 and saved["events"] > 100
    assert same["state"] == "same" and same["verdict"]["events"] == \
        saved["events"]
    assert moved["state"] == "differs"
    assert moved["verdict"]["differs_at"] is not None


def _avr_qemu():
    from tools.pp.qemu import QemuNotFound, find_qemu, list_machines
    try:
        qemu = find_qemu(target="avr")
    except QemuNotFound as exc:
        pytest.skip(str(exc))
    if "avr64ea48-cnano" not in {m.name for m in list_machines(qemu)}:
        pytest.skip(f"{qemu} has no avr64ea48-cnano machine")
    if not AVR_MATRIX.is_file():
        pytest.skip("matrix.elf not built (make -C firmware/avr-hello)")


@pytest.mark.qemu
def test_avr_recorder_faults_and_replay():
    """The AVR64EA48 Nano: SPI0's frames to the 8x8 G Click with its CS
    pad (PA7), TCB0's vector by name; a stuck MISO and a short on LED0's
    pad take effect; a run with SW0 and console input replays event for
    event (an interrupt pending across SEI is taken where it was)."""
    _avr_qemu()

    async def scenario():
        app = App("ev66e56a", None)
        app.emit = lambda m: None
        t = app.twins[0]
        t.clicks["mb1"] = {"id": "8x8g", "values": {}}
        await t.run(AVR_MATRIX)
        s = t.session
        if not s.trace_store:
            pytest.skip("QEMU build has no event recorder")
        try:
            await asyncio.sleep(1.0)
            st = s.trace_store
            labels = [r["label"] for r in
                      tl.suggest(t.board, t.clicks, {}, st)]
            frames = tr.spi(st.window(0, st.now), st.source("spi0"))
            await t.set_faults([{"kind": "miso", "host": "spi0",
                                 "value": "a5"},
                                {"kind": "short", "pad": "PB3",
                                 "level": 0}])
            mark = st.now
            await asyncio.sleep(0.5)
            stuck = tr.spi(st.window(mark, st.now), st.source("spi0"))
            led = t.summary()["leds"][0]["on"]
            await t.set_faults([])
            await t.record_start()
            await asyncio.sleep(0.4)
            await t.session.press("sw0", True)
            await asyncio.sleep(0.2)
            await t.session.press("sw0", False)
            await t.session.uart_send("hi\r\n")
            await asyncio.sleep(0.4)
            saved = t.record_stop("avr-demo")
            await t.replay("avr-demo", pace=False)
            for _ in range(300):
                await asyncio.sleep(0.1)
                if t.replaying["state"] != "running":
                    break
            replayed = dict(t.replaying)
        finally:
            await t.stop()
        return st, labels, frames, stuck, led, saved, replayed

    st, labels, frames, stuck, led, saved, replayed = asyncio.run(scenario())
    assert st.group_pins == 8 and st.vectors
    assert {"LED0 PB3", "SW0 PB2", "mikroBUS 1 CS PA7", "SPI0 SPI",
            "TCB0_INT"} <= set(labels)
    assert frames and len(frames[0]["mosi"]) >= 2
    assert stuck and set(stuck[-1]["miso"]) == {0xA5}
    assert led is True                  # LED0 is active low
    assert saved["inputs"] == 3 and saved["events"] > 100
    assert replayed["state"] == "same", replayed


@pytest.mark.qemu
def test_pykit_recorder(tmp_path, qemu_machines):
    """Stock CircuitPython on the PyKit's SAM E51: an I2C scan is a run of
    transactions on SERCOM5, and the interrupts go by the device's names."""
    from tools.pp import fatfs
    if "curiosity-circuitpython" not in qemu_machines:
        pytest.skip("QEMU build has no curiosity-circuitpython machine")
    if not PYKIT_ELF.is_file():
        pytest.skip("CircuitPython not fetched (py -m tools.pp.circuitpython "
                    "fetch)")
    code = (b"import board, time\n"
            b"i2c = board.I2C()\n"
            b"while not i2c.try_lock():\n"
            b"    pass\n"
            b"print(i2c.scan())\n"
            b"while True:\n"
            b"    time.sleep(1)\n")
    drive = tmp_path / "circuitpy.img"
    drive.write_bytes(fatfs.build_image({"code.py": code}))

    async def scenario():
        import time
        s = Session("curiosity-circuitpython", PYKIT_ELF, lambda m: None,
                    drive=drive)
        await s.start()
        try:
            st = s.trace_store
            if not st:
                pytest.skip("QEMU build has no event recorder")
            end, txs = time.monotonic() + 90, []
            while time.monotonic() < end and len(txs) < 112:
                await asyncio.sleep(0.5)
                src = st.source("sercom5")
                txs = tr.i2c(st.window(0, st.now), src) \
                    if src is not None else []
            names = [tr.exception_name(e, tl.device("atsame51j20a"))
                     for e in st.activity()["exceptions"]]
        finally:
            await s.stop()
        return st, txs, names

    st, txs, names = asyncio.run(scenario())
    assert st.group_pins == 32 and not st.vectors
    assert len(txs) >= 112              # 0x08-0x77, one start each
    assert txs[0]["text"].startswith("0x08 W")
    assert "RTC" in names and not any(n.startswith("IRQ ") for n in names)
