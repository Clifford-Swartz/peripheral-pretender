# Copyright (c) 2026 Clifford Swartz.
"""A board's timeline: rows of pads, buses and exceptions over a window of
virtual time, from what its event recorder saw (tools/pp/trace.py).

A row is {"id", "kind", "label", ...}: kind "pad" (with "pad": "PA24", or
"PB3" on AVR), "uart", "spi" or "i2c" (with "src": "sercom2", "usart1"),
or "irq" (with "exc": the exception number, or the AVR vector). suggest()
picks rows from the board (its LEDs and buttons, the pins of its Click
boards) and from what has happened (the buses that talked, the exceptions
taken). build() decodes a window for the page; the MCP tools use the same
rows.
"""

from __future__ import annotations

import json
from pathlib import Path

from tools.pp import trace as tr

ROOT = Path(__file__).resolve().parents[2]
MAX_ITEMS = 400                 # bus items a row, before they merge
MAX_EDGES = 4000                # pad edges a row, before they squeeze


def device(name: str | None) -> dict:
    try:
        return json.loads((ROOT / "devices" / f"{name}.json").read_text(
            encoding="utf-8")) if name else {}
    except (OSError, ValueError):
        return {}


def vcom_source(board: dict) -> str | None:
    """"sercom0" from the board's "SERCOM0 (PA06/PA07), ...", "usart1"
    from "USART1 (PC0/PC1), ..." """
    vcom = board.get("vcom") or ""
    word = vcom.split(" ", 1)[0].lower()
    return word if word.startswith(("sercom", "usart")) else None


def board_pads(board: dict, clicks: dict, wired: dict) -> list[dict]:
    """The board's named pads: LEDs, buttons, and its Click boards' pins"""
    rows, seen = [], set()

    def add(pad: str | None, label: str) -> None:
        if pad and pad not in seen:
            seen.add(pad)
            rows.append({"id": f"pad:{pad}", "kind": "pad", "pad": pad,
                         "label": f"{label} {pad}"})

    for led in board.get("leds", []):
        add(led.get("pin"), led["id"])
    for b in board.get("buttons", []):
        add(b.get("pin"), b["id"])
    for h in board.get("headers", []):
        c = clicks.get(h.get("slot"))
        if not c:
            continue
        for role, pad in (h.get("mikrobus") or {}).items():
            add(pad, f"{h['id']} {role.upper()}")
    for part, w in wired.items():
        for role, pad in sorted((w.get("pins") or {}).items()):
            if role in ("int", "pwm", "rst", "cs", "an"):
                add(pad, f"{part} {role.upper()}")
    return rows


def suggest(board: dict, clicks: dict, wired: dict,
            store: tr.Store | None) -> list[dict]:
    """Rows worth showing: the board's pads, the buses that talked (the
    VCOM first), other pads that changed, the exceptions taken"""
    rows = board_pads(board, clicks, wired)
    if not store:
        return rows
    act = store.activity()
    vcom = vcom_source(board)
    for name in sorted(act["buses"], key=lambda n: (n != vcom, n)):
        for kind in act["buses"][name]:
            label = f"{name.upper()} {kind.upper()}"
            if name == vcom and kind == "uart":
                label += " (VCOM)"
            rows.append({"id": f"{kind}:{name}", "kind": kind, "src": name,
                         "label": label})
    named = {r.get("pad") for r in rows}
    for pad in act["pads"]:
        if pad not in named and len(rows) < 24:
            rows.append({"id": f"pad:{pad}", "kind": "pad", "pad": pad,
                         "label": pad})
    dev = device(board.get("device"))
    for exc in act["exceptions"]:
        rows.append({"id": f"irq:{exc}", "kind": "irq", "exc": exc,
                     "label": tr.exception_name(exc, dev, store.vectors)})
    return rows


def build(store: tr.Store, rows: list[dict], t0: int, t1: int,
          width: int = 1000) -> list[dict]:
    """Each row's data from @t0 to @t1 (ns), for a view @width px wide:
    pads squeeze edges closer than a pixel into busy spans, buses merge
    into bursts past MAX_ITEMS."""
    w = store.window(t0, t1)
    resolution = max(1, (t1 - t0) // max(1, width))
    def canonical(pad: str) -> int | None:
        try:
            return store.pad_number(pad)
        except (ValueError, IndexError):
            return None                 # not a pad of this part

    pads = {r["pad"]: canonical(r["pad"]) for r in rows
            if r.get("kind") == "pad"}
    numbers = sorted({n for n in pads.values() if n is not None})
    pad_data = tr.pad_trace(w, numbers, resolution) if numbers else {}
    exc_data = tr.irqs(w) if any(r.get("kind") == "irq" for r in rows) \
        else {}
    out = []
    for r in rows:
        kind, data = r.get("kind"), None
        if kind == "pad":
            n = pads.get(r["pad"])
            data = None if n is None else pad_data.get(store.pad_name(n))
            if data and len(data["edges"]) > MAX_EDGES:
                data["edges"] = data["edges"][-MAX_EDGES:]
        elif kind in ("uart", "spi", "i2c"):
            src = store.source(r.get("src", ""))
            if src is not None:
                items = {"uart": tr.uart, "spi": tr.spi,
                         "i2c": tr.i2c}[kind](w, src)
                data = {"items": tr.bursts(items, MAX_ITEMS)}
        elif kind == "irq":
            data = exc_data.get(r.get("exc"), {"spans": [], "pends": 0})
        out.append({**r, "data": data})
    return out
