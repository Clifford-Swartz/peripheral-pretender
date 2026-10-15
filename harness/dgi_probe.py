# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Read-only probe of a debugger's Data Gateway Interface (DGI) over USB:
what the twin's EDBG model has to answer the same way.

    py -m harness.dgi_probe [--serial <serial>] [--poll <interface hex> ...]

Signs on, lists the interfaces, reads each one's configuration and status,
and (with --poll) enables the given interfaces for a few seconds and dumps
what they deliver, always stopping them and signing off at the end (also
on errors). Write the output to a file rather than piping it into a reader
that may stop early. The command framing is the documented generic DGI one:
command byte, 16-bit big-endian length, payload; replies echo the command
then a status byte (0x80 OK, 0xA0 data). Needs pyusb and libusb-package.
"""

from __future__ import annotations

import argparse
import struct
import sys
import time

import libusb_package
import usb.core
import usb.util

VENDOR = 0x03EB
EP_OUT, EP_IN = 0x06, 0x86
NAMES = {0x00: "timestamp", 0x20: "SPI", 0x21: "USART", 0x22: "I2C",
         0x30: "GPIO", 0x40: "power data", 0x41: "power sync", 0x50: "0x50"}


class Dgi:
    def __init__(self, serial: str | None):
        be = libusb_package.get_libusb1_backend()
        devs = [d for d in usb.core.find(find_all=True, idVendor=VENDOR,
                                         backend=be)
                if self._has_dgi(d)]
        if serial:
            devs = [d for d in devs if d.serial_number == serial]
        if not devs:
            sys.exit("no debugger with a DGI interface found")
        self.dev = devs[0]
        self.intf = next(i.bInterfaceNumber for c in self.dev for i in c
                         if any(e.bEndpointAddress == EP_IN for e in i))
        usb.util.claim_interface(self.dev, self.intf)
        self.drain()

    def drain(self) -> int:
        """Throw away replies left over from an earlier session."""
        n = 0
        while True:
            try:
                n += len(self.dev.read(EP_IN, 512, timeout=100))
            except usb.core.USBTimeoutError:
                return n

    @staticmethod
    def _has_dgi(dev) -> bool:
        return any(e.bEndpointAddress == EP_IN for c in dev for i in c
                   for e in i)

    def cmd(self, code: int, payload: bytes = b"") -> bytes:
        self.dev.write(EP_OUT, bytes([code]) + struct.pack(">H", len(payload))
                       + payload, timeout=1000)
        r = bytes(self.dev.read(EP_IN, 512, timeout=1000))
        # A poll reply (command, 0xA0, interface, 16-bit length, data) can
        # span several USB packets: read all of it, or the rest is taken
        # for the next command's reply
        if code == 0x15 and len(r) >= 5 and r[1] == 0xA0:
            want = 5 + struct.unpack(">H", r[3:5])[0]
            while len(r) < want:
                r += bytes(self.dev.read(EP_IN, 512, timeout=1000))
        return r

    def close(self) -> None:
        usb.util.release_interface(self.dev, self.intf)


def hexs(b: bytes) -> str:
    return b.hex(" ")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--serial")
    ap.add_argument("--poll", nargs="*", default=[],
                    help="interfaces to enable and poll (hex ids)")
    ap.add_argument("--seconds", type=float, default=3.0)
    ap.add_argument("--timestamp", action="store_true",
                    help="enable with timestamping (state 2)")
    args = ap.parse_args(argv)
    d = Dgi(args.serial)
    print("device", hex(d.dev.idProduct), d.dev.serial_number,
          d.dev.product, flush=True)
    enabled: list[int] = []
    try:
        r = d.cmd(0x00)
        print("sign on  ", hexs(r), "|", r[4:].decode(errors="replace"))
        print("version  ", hexs(d.cmd(0x02)))
        r = d.cmd(0x08)
        print("list     ", hexs(r))
        ids = list(r[3:3 + r[2]]) if len(r) > 3 else []
        for i in ids:
            print(f"  {i:#04x} {NAMES.get(i, '?'):10} config",
                  hexs(d.cmd(0x13, bytes([i]))))
        print("status   ", hexs(d.cmd(0x11)))
        if args.poll:
            polls = [int(x, 16) for x in args.poll]
            for i in list(polls):
                r = d.cmd(0x10, bytes([i, 2 if args.timestamp else 1]))
                print("enable", hex(i), hexs(r))
                if len(r) > 1 and r[1] == 0x80:
                    enabled.append(i)
                else:
                    # A debugger does not answer a poll of an interface
                    # that is off: never poll one that failed to enable
                    polls.remove(i)
            end = time.monotonic() + args.seconds
            while time.monotonic() < end:
                for i in polls:
                    r = d.cmd(0x15, bytes([i]))
                    if len(r) > 5 and r[3:5] != b"\0\0":
                        print(f"poll {i:#04x}", hexs(r))
                time.sleep(0.05)
    finally:
        # Always stop what was enabled and sign off: a debugger left
        # streaming into a session nobody reads can wedge its USB (an
        # nEDBG dropped off the bus that way and needed a replug)
        for i in enabled:
            try:
                d.cmd(0x10, bytes([i, 0]))
            except usb.core.USBError:
                pass
        try:
            print("sign off ", hexs(d.cmd(0x01)))
        except usb.core.USBError as e:
            print("sign off failed:", e)
        d.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
