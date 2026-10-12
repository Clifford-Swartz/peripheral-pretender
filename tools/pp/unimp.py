# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Summarise QEMU's ``-d unimp,guest_errors`` log into a model backlog.

Every line is classified as an unimplemented access (LOG_UNIMP), a guest
error (LOG_GUEST_ERROR), a trace event (when its name matches an enabled
``-trace`` pattern) or other text, and counted per (kind, instance,
register). The log does not record which mask produced a message, so the
unimp/guest-error split for free-form messages is made from their wording.
"""

from __future__ import annotations

import fnmatch
import re
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

UNIMP = "unimp"
GUEST_ERROR = "guest_error"
OTHER = "other"
KINDS = (UNIMP, GUEST_ERROR, OTHER)

NO_REGISTER = "-"
UNPARSED = "(unparsed)"

# hw/misc/unimp.c:
#   "VGA: unimplemented device write (size 4, offset 0x000000, value 0x41)"
_UNIMP_DEVICE = re.compile(
    r"^(?P<instance>.+?): unimplemented device (?P<access>read|write)\s+"
    r"\(size \d+, offset (?P<offset>0x[0-9a-fA-F]+)")

# system/memory.c and system/physmem.c (LOG_INVALID_MEM):
#   "Invalid read at addr 0x41000000, size 4, region '(null)', reason: ..."
_INVALID_MEM = re.compile(
    r"^Invalid (?P<access>read|write|access)(?: to non-RAM device)? at addr "
    r"(?P<addr>0x[0-9a-fA-F]+), size \d+, region '(?P<region>[^']*)'")

# QEMU's convention for device messages: "<instance>: <message>".
_PREFIXED = re.compile(r"^(?P<prefix>[^:\s][^:]{0,79}): (?P<message>.*)$")

# "sercom4.CTRLA" or "gclk.PCHCTRL[22]" anywhere in a message.
_DOTTED = re.compile(r"\b(?P<instance>[a-z][a-z0-9_]*)\."
                     r"(?P<register>[A-Z][A-Z0-9_]*(?:\[\d+\])?)")
_OFFSET = re.compile(r"\boffset\s+(?:0x)?(?P<hex>[0-9a-fA-F]+)\b")
_ADDR = re.compile(r"\baddr(?:ess)?\s+(?P<hex>0x[0-9a-fA-F]+)\b")
_REGISTER = re.compile(r"\b(?P<register>[A-Z][A-Z0-9_]*[A-Z0-9](?:\[\d+\])?)")
_ACCESS = re.compile(r"\b(read|write)s?\b", re.IGNORECASE)
_PREFIX_ACCESS = re.compile(r"^(?P<instance>.+?) (?P<access>read|write)$")
_UNIMP_WORDING = re.compile(
    r"unimplemented|not implemented|\bunimp\b|not modell?ed|"
    r"unsupported|not supported", re.IGNORECASE)

# trace "log" backend (scripts/tracetool/backend/log.py): "<event> <args>",
# optionally prefixed with "<pid>@<sec>.<usec>:".
_TRACE = re.compile(r"^(?:\d+@\d+\.\d+:)?(?P<event>[a-z_][a-z0-9_]*)(?: |$)")


@dataclass(frozen=True)
class LogEntry:
    kind: str
    instance: str
    register: str
    access: str | None
    line: str


def _hex(text: str) -> str:
    return f"0x{int(text, 16):x}"


def _access(text: str) -> str | None:
    m = _ACCESS.search(text)
    return m.group(1).lower() if m else None


def _kind_from_wording(text: str) -> str:
    return UNIMP if _UNIMP_WORDING.search(text) else GUEST_ERROR


def _register_from_message(message: str) -> str:
    for pattern, render in ((_OFFSET, _hex),
                            (_ADDR, lambda h: "@" + _hex(h))):
        m = pattern.search(message)
        if m:
            return render(m.group("hex"))
    m = _REGISTER.search(message)
    return m.group("register") if m else NO_REGISTER


def _parse_prefixed(line: str, prefix: str, message: str) -> LogEntry:
    instance, access = prefix, None
    m = _PREFIX_ACCESS.match(prefix)
    if m:
        instance, access = m.group("instance"), m.group("access")
    dotted = _DOTTED.search(line)
    if dotted:
        instance, register = dotted.group("instance"), dotted.group("register")
    else:
        register = _register_from_message(message)
    return LogEntry(_kind_from_wording(message), instance, register,
                    access or _access(message), line)


def parse_line(line: str) -> LogEntry | None:
    """Classify one log line; None for blank lines."""
    line = line.rstrip("\r\n")
    if not line.strip():
        return None
    m = _UNIMP_DEVICE.match(line)
    if m:
        return LogEntry(UNIMP, m.group("instance"), _hex(m.group("offset")),
                        m.group("access"), line)
    m = _INVALID_MEM.match(line)
    if m:
        region = m.group("region")
        instance = region if region and region != "(null)" else "(unassigned)"
        access = m.group("access")
        return LogEntry(GUEST_ERROR, instance, "@" + _hex(m.group("addr")),
                        None if access == "access" else access, line)
    m = _PREFIXED.match(line)
    if m:
        return _parse_prefixed(line, m.group("prefix"), m.group("message"))
    kind = UNIMP if _UNIMP_WORDING.search(line) else OTHER
    return LogEntry(kind, UNPARSED, NO_REGISTER, _access(line), line)


def trace_event(line: str, patterns: Iterable[str]) -> str | None:
    """Name of the trace event on *line* if it matches an enabled pattern."""
    m = _TRACE.match(line)
    if m and _event_enabled(m.group("event"), patterns):
        return m.group("event")
    return None


def _event_enabled(event: str, patterns: Iterable[str]) -> bool:
    return any(fnmatch.fnmatchcase(event, p) for p in patterns)


_PARSE_CACHE_LINES = 4096
_NOT_CACHED = object()


@dataclass
class _Group:
    kind: str
    instance: str
    register: str
    count: int = 0
    reads: int = 0
    writes: int = 0
    samples: list[str] = field(default_factory=list)


class LogSummary:
    """Accumulates log lines; :meth:`as_dict` is the run's
    ``unimplemented_summary`` and :meth:`trace_dict` its trace digest."""

    def __init__(self, trace_patterns: Iterable[str] = (),
                 max_samples: int = 3,
                 max_trace_lines: int = 10000) -> None:
        self.trace_patterns = tuple(trace_patterns)
        self.max_samples = max_samples
        self.max_trace_lines = max_trace_lines
        self._groups: dict[tuple[str, str, str], _Group] = {}
        self._trace_events: Counter[str] = Counter()
        self._trace_lines: list[str] = []
        # Firmware polling an unmodelled register repeats one line millions
        # of times, so parsed lines and per-event pattern matches are cached.
        self._parsed: dict[str, LogEntry | None] = {}
        self._traced: dict[str, bool] = {}

    def add_line(self, line: str) -> None:
        line = line.rstrip("\r\n")
        event = self._trace_event(line)
        if event:
            self._trace_events[event] += 1
            if len(self._trace_lines) < self.max_trace_lines:
                self._trace_lines.append(line)
            return
        entry = self._parsed.get(line, _NOT_CACHED)
        if entry is _NOT_CACHED:
            entry = parse_line(line)
            if len(self._parsed) < _PARSE_CACHE_LINES:
                self._parsed[line] = entry
        if entry:
            self.add(entry)

    def _trace_event(self, line: str) -> str | None:
        if not self.trace_patterns:
            return None
        m = _TRACE.match(line)
        if not m:
            return None
        event = m.group("event")
        enabled = self._traced.get(event)
        if enabled is None:
            enabled = self._traced[event] = _event_enabled(
                event, self.trace_patterns)
        return event if enabled else None

    def add(self, entry: LogEntry) -> None:
        key = (entry.kind, entry.instance, entry.register)
        group = self._groups.get(key)
        if group is None:
            group = self._groups[key] = _Group(*key)
        group.count += 1
        if entry.access == "read":
            group.reads += 1
        elif entry.access == "write":
            group.writes += 1
        if (len(group.samples) < self.max_samples
                and entry.line not in group.samples):
            group.samples.append(entry.line)

    def as_dict(self) -> dict:
        groups = sorted(self._groups.values(),
                        key=lambda g: (-g.count, KINDS.index(g.kind),
                                       g.instance.lower(), g.register))
        counts = Counter({kind: 0 for kind in KINDS})
        instances: Counter[str] = Counter()
        for g in groups:
            counts[g.kind] += g.count
            instances[g.instance] += g.count
        return {
            "total": sum(counts.values()),
            "counts": dict(counts),
            "instances": dict(instances.most_common()),
            "groups": [{
                "kind": g.kind,
                "instance": g.instance,
                "register": g.register,
                "count": g.count,
                "reads": g.reads,
                "writes": g.writes,
                "samples": list(g.samples),
            } for g in groups],
        }

    def trace_dict(self) -> dict:
        total = sum(self._trace_events.values())
        return {
            "count": total,
            "events": dict(self._trace_events.most_common()),
            "lines": list(self._trace_lines),
            "truncated": total > len(self._trace_lines),
        }


def summarize_lines(lines: Iterable[str],
                    trace_patterns: Iterable[str] = ()) -> LogSummary:
    summary = LogSummary(trace_patterns)
    for line in lines:
        summary.add_line(line)
    return summary


def summarize_log(path: str | Path,
                  trace_patterns: Iterable[str] = ()) -> LogSummary:
    """Summarise a QEMU -D log file (missing file = empty summary)."""
    path = Path(path)
    if not path.exists():
        return LogSummary(trace_patterns)
    with path.open(encoding="utf-8", errors="replace") as f:
        return summarize_lines(f, trace_patterns)
