# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""py -m tools.pp_analog: the analog host.

  serve CIRCUIT [--port N] [--tick-us US] [--tmax S] [--log FILE]
        [--interactive]
      Load CIRCUIT into ngspice, listen on 127.0.0.1 (port N, default any)
      and print "ready <port>"; serve one twin (QEMU with
      -chardev socket,id=cosim,host=127.0.0.1,port=<port>,nodelay=on and
      -machine cosim=cosim) until it goes away; then print "done <json>"
      (the run's statistics and errors). Exit 0, or 2 if ngspice failed
      during the run. --interactive: read "set NAME VALUE" lines (live
      controls) on stdin, and print "state <json>" lines, {"t": virtual
      seconds, "avg": {vector: average since the last line}}, about 30
      times a second.
  check CIRCUIT [--machine M]
      Load CIRCUIT into ngspice and print its pads and the ADC inputs on
      them.

Exit 1 when the circuit cannot be loaded or ngspice is missing.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import socket
import sys
import threading

from . import circuit as circ
from .ngspice import NgSpiceError


def _host(args: argparse.Namespace):
    from .host import Host
    log = open(args.log, "w", encoding="utf-8") if args.log else None
    on_state = None
    if getattr(args, "interactive", False):
        def on_state(t, avg):
            print("state " + json.dumps({"t": t, "avg": avg}), flush=True)
    host = Host(circ.load(args.circuit), tick_ns=int(args.tick_us * 1000),
                tmax=args.tmax, log=log, on_state=on_state)
    if on_state:
        threading.Thread(target=_controls, args=(host,), daemon=True).start()
    return host


def _controls(host) -> None:
    """Live controls from stdin: "set NAME VALUE" lines."""
    for line in sys.stdin:
        w = line.split()
        if len(w) == 3 and w[0] == "set":
            try:
                host.set_control(w[1], float(w[2]))
            except ValueError:
                pass


def cmd_serve(args: argparse.Namespace) -> int:
    host = _host(args)
    with socket.create_server(("127.0.0.1", args.port)) as srv:
        print(f"ready {srv.getsockname()[1]}", flush=True)
        srv.settimeout(args.accept_timeout)
        try:
            sock, _ = srv.accept()
        except socket.timeout:
            print("no twin connected", file=sys.stderr)
            return 1
    with sock:
        host.serve(sock)
    stats = dataclasses.asdict(host.stats)
    print("done " + json.dumps(stats), flush=True)
    return 2 if host.stats.errors else 0


def cmd_check(args: argparse.Namespace) -> int:
    c = circ.load(args.circuit)
    from .host import Host
    Host(c)                             # loads it into ngspice
    ain = circ.adc_inputs(circ.machine_device(args.machine))
    print(f"{c.title}: pads {', '.join(c.pads)}")
    for pad in c.pads:
        inputs = ", ".join(f"{p.rsplit('/', 1)[1]} AIN{ch}"
                           for p, ch in ain.get(pad, []))
        print(f"  {pad}: {inputs or 'no ADC input'}")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="python -m tools.pp_analog",
                                description="Analog host for the twin")
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("serve", help="serve one twin")
    s.add_argument("circuit")
    s.add_argument("--port", type=int, default=0)
    s.add_argument("--tick-us", type=float, default=1000.0,
                   help="input-pad update period in virtual time (0: none)")
    s.add_argument("--tmax", type=float, default=circ.TMAX_S,
                   help="largest ngspice time step, seconds (default "
                        "%(default)g: well inside an ADC LSB on an RC)")
    s.add_argument("--accept-timeout", type=float, default=60.0)
    s.add_argument("--log", metavar="FILE",
                   help="write the messages, both ways, to FILE")
    s.add_argument("--interactive", action="store_true",
                   help="live controls on stdin, state lines on stdout")
    c = sub.add_parser("check", help="load a circuit and list its pads")
    c.add_argument("circuit")
    c.add_argument("--machine", default="pic32cm-mc00-cpro")
    args = p.parse_args(argv)
    try:
        return cmd_serve(args) if args.cmd == "serve" else cmd_check(args)
    except (circ.CircuitError, NgSpiceError) as e:
        print(f"pp_analog: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
