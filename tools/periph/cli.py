# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""``py -m tools.periph``: the mechanical steps of modelling a peripheral.

The procedure is playbooks/model-peripheral.md; this tool does the parts
that are the same every time:

  status   BOARD                which instances have a model, which are
                                register-only
  scaffold BOARD INSTANCE NAME  probe and checked firmware, model, header,
                                qtest, overlay and research note skeletons,
                                wired into meson and the SoC's model table;
                                --firmware-only for a model that exists
  regen                         generate every device in one run
  qemu     [--avr]              build pp-qemu (stops running QEMUs first)
  build    BOARD APP            build firmware/hello APP for BOARD
  twin     BOARD APP            run it on the twin (icount), keep the
                                transcript
  silicon  BOARD APP            program the desk board, keep the transcript
  probe    BOARD APP            build, silicon, twin, diff
  diff     BOARD APP            twin against silicon transcripts
  check    BOARD APP            build; twin and silicon must both pass
                                (PP:EXIT 0); the silicon transcript becomes
                                golden/<device>/<APP>-silicon.txt
  qtest    NAME                 build and run one pp qtest
  gate                          the full Python suite and every pp qtest
  export                        pp-qemu HEAD as the next qemu/patches file
  desk     BOARD --probe ID --port COMn [--allow-program]
                                record the desk board (.pp-desk.json, local)

Transcripts go to build/periph/<board>/. Paths: PP_QEMU_SRC (pp-qemu,
default ../pp-qemu, then C:/pp/pp-qemu), PP_PACKS (~/.mchp_packs),
PP_MSYS2_ROOT (C:/msys64, Windows only).
"""

from __future__ import annotations

import argparse
import difflib
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from string import Template

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "build" / "periph"
DESK = ROOT / ".pp-desk.json"
TEMPLATES = Path(__file__).resolve().parent / "templates"
PACKS = Path(os.environ.get("PP_PACKS", Path.home() / ".mchp_packs"))
MSYS2 = Path(os.environ.get("PP_MSYS2_ROOT", "C:/msys64"))
ICOUNT = 5


@dataclass(frozen=True)
class Board:
    key: str
    fw_board: str           # firmware/hello BOARD=
    machine: str
    device: str             # devices/<device>.json
    golden: str             # golden/<dir>
    qtest_prefix: str       # tests/qtest/<prefix>-<name>-test.c


BOARDS = {
    "mc00": Board("mc00", "pic32cm_mc00_cpro", "pic32cm-mc00-cpro",
                  "pic32cm1216mc00048", "pic32cm1216mc00048-a1", "pic32cm"),
    "jh01": Board("jh01", "pic32cm_jh01_cpro", "pic32cm-jh01-cpro",
                  "pic32cm5164jh01100", "pic32cm5164jh01100", "pic32cm"),
}


class PeriphError(Exception):
    pass


def log(msg: str) -> None:
    print(f"[periph] {msg}", flush=True)


def board(key: str) -> Board:
    if key not in BOARDS:
        raise PeriphError(f"unknown board {key!r}; boards: "
                          f"{', '.join(BOARDS)}")
    return BOARDS[key]


def qemu_src() -> Path:
    env = os.environ.get("PP_QEMU_SRC")
    for p in ([Path(env)] if env else []) + [ROOT.parent / "pp-qemu",
                                             Path("C:/pp/pp-qemu")]:
        if (p / "hw" / "arm" / "pic32cm_soc.c").is_file():
            return p
    raise PeriphError("pp-qemu not found: set PP_QEMU_SRC")


def device_json(b: Board) -> dict:
    return json.loads((ROOT / "devices" / f"{b.device}.json")
                      .read_text(encoding="utf-8"))


def dfp_dir(b: Board) -> Path:
    src = device_json(b)["source"]
    return PACKS / "Microchip" / src["dfp"] / src["dfp_version"]


def msys(cmd: str, cwd: Path) -> int:
    """Run @cmd in an MSYS2 UCRT64 login shell (Windows), else sh."""
    if os.name == "nt":
        bash = MSYS2 / "usr" / "bin" / "bash.exe"
        env = dict(os.environ, MSYSTEM="UCRT64", CHERE_INVOKING="1")
        return subprocess.call([str(bash), "-lc", cmd], cwd=cwd, env=env)
    return subprocess.call(["sh", "-c", cmd], cwd=cwd)


def run_env() -> dict:
    src = qemu_src()
    env = dict(os.environ)
    env.setdefault("PP_QEMU", str(src / "build" / "qemu-system-arm.exe"
                                  if os.name == "nt" else
                                  src / "build" / "qemu-system-arm"))
    env.setdefault("PP_QEMU_AVR", str(src / "build-avr" /
                                      ("qemu-system-avr.exe" if os.name == "nt"
                                       else "qemu-system-avr")))
    env.setdefault("PP_QEMU_SRC", str(src))
    if os.name == "nt":
        env.setdefault("PP_MSYS2_BIN", str(MSYS2 / "ucrt64" / "bin"))
    return env


def stop_qemu() -> None:
    if os.name == "nt":
        for exe in ("qemu-system-arm.exe", "qemu-system-avr.exe"):
            subprocess.call(["taskkill", "/F", "/IM", exe],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)


def elf(b: Board, app: str) -> Path:
    return ROOT / "firmware" / "hello" / "build" / b.fw_board / f"{app}.elf"


def transcript(b: Board, app: str, where: str) -> Path:
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / b.key).mkdir(exist_ok=True)
    return WORK / b.key / f"{app}-{where}.txt"


def uart_lines(path: Path) -> list[str]:
    """A transcript without the runner's own lines, CRs and blank lines."""
    out = []
    for line in path.read_bytes().decode("utf-8", errors="replace") \
            .replace("\r", "").split("\n"):
        if not line.strip() or line.startswith("[pp]") or \
                line.startswith("    "):
            continue
        out.append(line)
    return out


def exit_code(path: Path) -> int | None:
    m = re.findall(r"PP:EXIT (\d+)", path.read_text(encoding="utf-8",
                                                    errors="replace"))
    return int(m[-1]) if m else None


# ---- commands ----

def cmd_build(b: Board, app: str) -> None:
    packs = str(PACKS).replace("\\", "/")
    fw = ROOT / "firmware" / "hello"
    if msys(f'make BOARD={b.fw_board} APP={app} PACKS="{packs}"', fw):
        raise PeriphError(f"build of {app} failed")
    log(f"built {elf(b, app).relative_to(ROOT)}")


def cmd_twin(b: Board, app: str, machine_opts: str = "") -> int:
    out = transcript(b, app, "twin")
    machine = b.machine + (f",{machine_opts}" if machine_opts else "")
    # No stop_qemu() here: it kills every QEMU on the host, a gate's included
    with out.open("w", encoding="utf-8") as f:
        rc = subprocess.call([sys.executable, "-m", "tools.pp", "run",
                              str(elf(b, app)), "--machine", machine,
                              "--timeout", "300", "--icount", str(ICOUNT)],
                             cwd=ROOT, env=run_env(), stdout=f,
                             stderr=subprocess.STDOUT)
    log(f"twin: exit {rc}, {out.relative_to(ROOT)}")
    return rc


def desk(b: Board) -> dict:
    cfg = json.loads(DESK.read_text(encoding="utf-8")) if DESK.is_file() \
        else {}
    if b.key not in cfg:
        raise PeriphError(f"no desk board {b.key}: py -m tools.periph desk "
                          f"{b.key} --probe <EDBG serial> --port <COMn>")
    return cfg[b.key]


def cmd_silicon(b: Board, app: str, timeout: int = 120) -> int:
    d = desk(b)
    if not d.get("allow_program"):
        raise PeriphError(f"the owner has not allowed programming the {b.key}"
                          f" board: py -m tools.periph desk {b.key} "
                          "--allow-program (their decision)")
    out = transcript(b, app, "silicon")
    rc = subprocess.call([sys.executable, "-m", "harness.hw_run",
                          str(elf(b, app)), "--allow-program",
                          "--probe", d["probe"], "--target", b.device,
                          "--pack", str(dfp_dir(b)), "--port", d["port"],
                          "--timeout", str(timeout), "--out", str(out)],
                         cwd=ROOT, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    log(f"silicon: exit {rc}, {out.relative_to(ROOT)}")
    return rc


def fields(line: str) -> tuple[str, list[tuple[str, str]]]:
    """'E7 enable a=1 b=2' -> ('E7 enable', [('a', '1'), ('b', '2')])"""
    head, kv = [], []
    for tok in line.split():
        if "=" in tok and not tok.startswith("="):
            k, v = tok.split("=", 1)
            kv.append((k, v))
        elif not kv:
            head.append(tok)
        else:
            kv.append((tok, ""))
    return " ".join(head), kv


def field_diff(a: str, b: str) -> list[str] | None:
    """The fields of line @a (silicon) that differ in @b (twin), or None
    when the lines do not pair (different heads or field names)."""
    ha, fa = fields(a)
    hb, fb = fields(b)
    if ha != hb or [k for k, _ in fa] != [k for k, _ in fb]:
        return None
    return [f"{k}: {va} -> {vb}" for (k, va), (_, vb) in zip(fa, fb)
            if va != vb]


def cmd_diff(b: Board, app: str) -> bool:
    sil, twin = transcript(b, app, "silicon"), transcript(b, app, "twin")
    for p in (sil, twin):
        if not p.is_file():
            raise PeriphError(f"no {p.relative_to(ROOT)}")
    la, lb = uart_lines(sil), uart_lines(twin)
    differ = 0
    sm = difflib.SequenceMatcher(a=la, b=lb, autojunk=False)
    for op, i1, i2, j1, j2 in sm.get_opcodes():
        if op == "equal":
            continue
        if op == "replace" and i2 - i1 == j2 - j1:
            # Same shape: report each line by the fields that moved.
            for x, y in zip(la[i1:i2], lb[j1:j2]):
                differ += 1
                d = field_diff(x, y)
                if d is None:
                    print(f"- {x}")
                    print(f"+ {y}")
                    continue
                head = fields(x)[0].rstrip(":") or "(line)"
                print(f"{head}:  silicon -> twin")
                for item in d:
                    print(f"    {item}")
            continue
        for line in la[i1:i2]:
            print(f"- {line}")
        for line in lb[j1:j2]:
            print(f"+ {line}")
        differ += max(i2 - i1, j2 - j1)
    log("transcripts match" if not differ else f"{differ} lines differ")
    return not differ


def cmd_probe(b: Board, app: str, machine_opts: str) -> None:
    cmd_build(b, app)
    cmd_silicon(b, app)
    cmd_twin(b, app, machine_opts)
    cmd_diff(b, app)


def cmd_check(b: Board, app: str) -> None:
    cmd_build(b, app)
    if cmd_twin(b, app) != 0:
        print("\n".join(uart_lines(transcript(b, app, "twin"))[-15:]))
        raise PeriphError("the twin fails the checks: fix the model or the "
                          "check")
    if cmd_silicon(b, app) != 0:
        print("\n".join(uart_lines(transcript(b, app, "silicon"))[-15:]))
        raise PeriphError("silicon fails the checks: the check (or the "
                          "model it encodes) is wrong")
    dest = ROOT / "golden" / b.golden / f"{app}-silicon.txt"
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(transcript(b, app, "silicon"), dest)
    log(f"golden: {dest.relative_to(ROOT)} (tests/golden picks it up)")


def cmd_regen() -> None:
    sys.path.insert(0, str(ROOT / "tests" / "generator"))
    import gen_support as gs  # noqa: E402
    atdfs = [gs.ATDF, gs.ATDF_MC00, gs.ATDF_AVR, gs.ATDF_SAME51]
    args = [sys.executable, str(ROOT / "generator" / "atdf2qemu.py")]
    for a in atdfs:
        args += ["--atdf", str(a)]
    args += ["--qemu-out", str(qemu_src()), "--json-out", "devices",
             "--report", "generator/out/report-{device}.md"]
    p = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    for line in (p.stdout + p.stderr).splitlines():
        if "error" in line.lower() or line.startswith("atdf2qemu:"):
            print(line)
    if p.returncode:
        raise PeriphError("generator failed")


def ninja(targets: list[str], avr: bool = False) -> None:
    stop_qemu()
    build = "build-avr" if avr else "build"
    src = str(qemu_src()).replace("\\", "/")
    rc = msys(f"ninja -C '{src}/{build}' {' '.join(targets)} 2>&1 | "
              "grep -v 'cannot symlink'; exit ${PIPESTATUS[0]}", ROOT)
    if rc:
        raise PeriphError("ninja failed")


def cmd_qemu(avr: bool) -> None:
    ninja(["qemu-system-avr.exe" if avr else "qemu-system-arm.exe"
           if os.name == "nt" else "all"], avr)
    log("pp-qemu built")


def pp_qtests() -> list[str]:
    meson = (qemu_src() / "tests" / "qtest" / "meson.build") \
        .read_text(encoding="utf-8")
    names = set(re.findall(r"'((?:pic32cm|curiosity|avr64ea48)[\w-]*-test)'",
                           meson))
    return sorted(names)


def cmd_qtest(names: list[str]) -> bool:
    exe = ".exe" if os.name == "nt" else ""
    avr = [n for n in names if n.startswith("avr")]
    arm = [n for n in names if n not in avr]
    have_avr = (qemu_src() / "build-avr" / "build.ninja").is_file()
    if arm:
        ninja([f"tests/qtest/{n}{exe}" for n in arm] +
              ["qemu-system-arm" + exe])
    if avr and have_avr:
        ninja([f"tests/qtest/{n}{exe}" for n in avr] +
              ["qemu-system-avr" + exe], avr=True)
    ok = True
    for n in names:
        target = "avr" if n in avr else "arm"
        build = qemu_src() / ("build-avr" if n in avr else "build")
        qemu = build / f"qemu-system-{target}{exe}"
        if n in avr and not have_avr:
            log(f"{n}: skipped (no AVR build)")
            continue
        env = dict(run_env(), QTEST_QEMU_BINARY=str(qemu))
        if os.name == "nt":
            env["PATH"] = str(MSYS2 / "ucrt64" / "bin") + os.pathsep + \
                env["PATH"]
        p = subprocess.run([str(build / "tests" / "qtest" / f"{n}{exe}")],
                           env=env, capture_output=True, text=True)
        good = len(re.findall(r"^ok ", p.stdout, re.M))
        bad = re.findall(r"^not ok .*", p.stdout, re.M)
        ok &= p.returncode == 0 and not bad
        log(f"{n}: {good} ok, {len(bad)} not ok" +
            ("" if p.returncode == 0 else f" (exit {p.returncode})"))
        for line in bad:
            print("   ", line)
    return ok


def cmd_gate() -> None:
    # The suite runs whatever binaries exist: build them first
    cmd_qemu(False)
    if (qemu_src() / "build-avr" / "build.ninja").is_file():
        cmd_qemu(True)
    rc = subprocess.call([sys.executable, "-m", "pytest", "tests",
                          "harness/tests", "-q"], cwd=ROOT, env=run_env())
    qt = cmd_qtest(pp_qtests())
    if rc or not qt:
        raise PeriphError(f"gate failed (pytest exit {rc}, qtests "
                          f"{'ok' if qt else 'failing'})")
    log("gate passed")


def cmd_export() -> None:
    src = qemu_src()
    patches = ROOT / "qemu" / "patches"
    if subprocess.run(["git", "-C", str(src), "status", "--porcelain",
                       "--untracked-files=no"], capture_output=True,
                      text=True).stdout.strip():
        raise PeriphError("pp-qemu has uncommitted changes: commit first")
    head = subprocess.run(["git", "-C", str(src), "rev-parse", "HEAD"],
                          capture_output=True, text=True).stdout.strip()
    # Python's write_text() writes CRLF on Windows, and pp-qemu keeps bytes
    show = subprocess.run(["git", "-C", str(src), "show", "--format=",
                           "HEAD"], capture_output=True).stdout
    crlf = [line for line in show.split(b"\n")
            if line.startswith(b"+") and line.endswith(b"\r")]
    if crlf:
        raise PeriphError(f"HEAD adds {len(crlf)} CRLF lines: convert the "
                          "files to LF and amend")
    existing = sorted(patches.glob("[0-9][0-9][0-9][0-9]-*.patch"))
    if existing and existing[-1].read_text(encoding="utf-8",
                                           errors="replace") \
            .startswith(f"From {head} "):
        raise PeriphError(f"HEAD is already {existing[-1].name}")
    base = subprocess.run(["git", "-C", str(src), "describe", "--tags",
                           "--abbrev=0"], capture_output=True,
                          text=True).stdout.strip()
    count = int(subprocess.run(["git", "-C", str(src), "rev-list", "--count",
                                f"{base}..HEAD"], capture_output=True,
                               text=True).stdout)
    if count != len(existing) + 1:
        raise PeriphError(f"pp-qemu has {count} commits since {base} but "
                          f"qemu/patches has {len(existing)}: export by hand")
    subprocess.check_call(["git", "-C", str(src), "format-patch", "-q", "-1",
                           "--start-number", str(count), "-o", str(patches),
                           "HEAD"])
    log(f"exported {sorted(patches.glob(f'{count:04d}-*.patch'))[0].name}")


def cmd_desk(b: Board, probe: str | None, port: str | None,
             allow: bool) -> None:
    cfg = json.loads(DESK.read_text(encoding="utf-8")) if DESK.is_file() \
        else {}
    entry = cfg.setdefault(b.key, {})
    if probe:
        entry["probe"] = probe
    if port:
        entry["port"] = port
    if allow:
        entry["allow_program"] = True
    DESK.write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")
    log(f"{b.key}: {entry}")


def soc_models() -> dict[str, list[tuple[str, str | None]]]:
    src = (qemu_src() / "hw" / "arm" / "pic32cm_soc.c").read_text(
        encoding="utf-8")
    table = src[src.index("pic32cm_models[] = {"):]
    table = table[:table.index("};")]
    out: dict[str, list[tuple[str, str | None]]] = {}
    for mod, typ, ip in re.findall(
            r'\{ "(\w+)", (\w+)(?:, "(\w+)")? \}', table):
        out.setdefault(mod, []).append((typ, ip or None))
    return out


CORE = {"SystemControl", "SysTick", "NVIC", "MPU"}


def cmd_status(b: Board) -> None:
    models = soc_models()
    goldens = {p.name[:-len("-silicon.txt")]
               for p in (ROOT / "golden" / b.golden).glob("*-silicon.txt")}
    print(f"{'instance':12} {'module':10} {'ip':7} model")
    for inst in device_json(b)["instances"]:
        if inst["name"] in CORE:
            kind = "QEMU Cortex-M core"
        elif inst["module"] == "FUSES":
            kind = "NVM rows (memory)"
        else:
            hits = [t for t, ip in models.get(inst["module"], [])
                    if ip in (None, inst["ip_id"])]
            kind = hits[0] if hits else "REGISTER-ONLY"
        print(f"{inst['name']:12} {inst['module']:10} "
              f"{str(inst['ip_id']):7} {kind}")
    print(f"\nchecked apps with a silicon golden ({b.golden}): "
          f"{', '.join(sorted(goldens))}")


def insert_sorted(path: Path, start: str, stop: str, line: str,
                  key=lambda s: s.strip()) -> bool:
    """Insert @line into the sorted block of @path between @start, @stop."""
    text = path.read_text(encoding="utf-8")
    if line.strip() in text:
        return False
    i = text.index(start) + len(start)
    j = text.index(stop, i)
    block = text[i:j].splitlines(keepends=True)
    pos = len(block)
    for n, existing in enumerate(block):
        if existing.strip() and key(existing) > key(line):
            pos = n
            break
    block.insert(pos, line + "\n")
    path.write_text(text[:i] + "".join(block) + text[j:], encoding="utf-8")
    return True


def instance_of(b: Board, instance: str) -> dict:
    inst = next((i for i in device_json(b)["instances"]
                 if i["name"] == instance), None)
    if not inst:
        raise PeriphError(f"no instance {instance} on {b.device}")
    return inst


def scaffold_subst(b: Board, instance: str, name: str) -> dict[str, str]:
    """The template variables for @instance of @b's device."""
    dev = device_json(b)
    inst = instance_of(b, instance)
    module, ip_id = inst["module"], inst["ip_id"]
    regs = dev["ips"][inst["ip"]]["views"][0]["registers"]
    year = "2026"
    events = dev.get("events", {})
    return {
        "NAME": name.upper(), "name": name, "MODULE": module, "IP": ip_id,
        "ip": ip_id.lower(), "module": module.lower(), "year": year,
        "DEVICE": dev["device"]["name"], "device": b.device,
        "TYPE": f"TYPE_MCHP_{name.upper()}_{ip_id.upper()}",
        "type": f"mchp-{name}-{ip_id.lower()}",
        "STATE": f"Mchp{name.capitalize()}State",
        "CAST": f"MCHP_{name.upper()}_{ip_id.upper()}",
        "BASE": f"0x{inst['base']:08X}", "IRQ": str(inst["irq"]["line"]
                                                    if inst["irq"] else "-"),
        "GCLK": str(inst["gclk_core"]), "BOARD": b.fw_board,
        "MACHINE": b.machine,
        "REGS": "\n".join(f"| 0x{r['offset']:02X} | {r['name']} | "
                          f"{r['size'] * 8} |" for r in regs),
        "OVERLAY_REGS": "\n".join(f"  {r['name']}: {{}}" for r in regs),
        "EVENTS": ", ".join(f"{e['name']} ({k[:-1]} {e['index']})"
                            for k in ("generators", "users")
                            for e in events.get(k, [])
                            if e["instance"] == instance) or "none",
        "SIGNALS": ", ".join(sorted({f"{s['group']}{s['index']} {s['pad']}"
                                     for s in dev["signals"]
                                     if s["instance"] == instance
                                     and s["pin"] >= 0})) or "none",
    }


def cmd_scaffold(b: Board, instance: str, name: str,
                 firmware_only: bool = False) -> None:
    inst = instance_of(b, instance)
    module, ip_id = inst["module"], inst["ip_id"]
    subst = scaffold_subst(b, instance, name)
    src = qemu_src()
    files = {
        ROOT / "firmware/hello/tests" / f"{name}probe.c": "probe.c",
        ROOT / "firmware/hello/tests" / f"{name}.c": "check.c",
        ROOT / "overlays" / f"{module.lower()}_{ip_id.lower()}.yaml":
            "overlay.yaml",
        ROOT / "docs/research/peripherals" /
            f"{module.lower()}-{ip_id.lower()}.md": "note.md",
        src / "hw/misc" / f"mchp_{name}_{ip_id.lower()}.c": "model.c",
        src / "include/hw/misc" / f"mchp_{name}.h": "model.h",
        src / "tests/qtest" / f"{b.qtest_prefix}-{name}-test.c": "qtest.c",
    }
    if firmware_only:
        files = {k: v for k, v in files.items()
                 if v in ("probe.c", "check.c", "note.md")}
    for dest, tpl in files.items():
        if dest.exists():
            log(f"keep {dest} (exists)")
            continue
        text = Template((TEMPLATES / tpl).read_text(encoding="utf-8")) \
            .safe_substitute(subst)
        dest.write_text(text, encoding="utf-8")
        log(f"wrote {dest}")
    if firmware_only:
        log("next: playbooks/model-peripheral.md, phase 1 (research)")
        return
    # Wiring: meson (sorted lists) and the SoC's model table
    if insert_sorted(src / "hw/misc/meson.build",
                     "system_ss.add(when: 'CONFIG_PIC32CM_SOC', "
                     "if_true: files(\n", "))",
                     f"  'mchp_{name}_{ip_id.lower()}.c',"):
        log("hw/misc/meson.build: added")
    meson_q = src / "tests/qtest/meson.build"
    text = meson_q.read_text(encoding="utf-8")
    qname = f"'{b.qtest_prefix}-{name}-test'"
    if qname not in text:
        text = text.replace("['pic32cm-mc00-test', ",
                            f"['pic32cm-mc00-test', {qname}, ", 1)
        meson_q.write_text(text, encoding="utf-8")
        log("tests/qtest/meson.build: added")
    soc = src / "hw/arm/pic32cm_soc.c"
    if insert_sorted(soc, "} pic32cm_models[] = {\n", "};",
                     f'    {{ "{module}", {subst["TYPE"]}, "{ip_id}" }},'):
        text = soc.read_text(encoding="utf-8")
        inc = f'#include "hw/misc/mchp_{name}.h"\n'
        if inc not in text:
            text = text.replace('#include "hw/misc/mchp_ac.h"\n',
                                '#include "hw/misc/mchp_ac.h"\n' + inc, 1)
            soc.write_text(text, encoding="utf-8")
        log("hw/arm/pic32cm_soc.c: model table entry and include added")
    log("next: playbooks/model-peripheral.md, phase 1 (research)")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="py -m tools.periph",
                                 description=__doc__,
                                 formatter_class=argparse.
                                 RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for c in ("build", "twin", "silicon", "probe", "diff", "check"):
        p = sub.add_parser(c)
        p.add_argument("board")
        p.add_argument("app")
        if c in ("twin", "probe"):
            p.add_argument("--machine-opts", default="",
                           help="e.g. vcc=4890")
    p = sub.add_parser("status")
    p.add_argument("board")
    p = sub.add_parser("scaffold")
    p.add_argument("board")
    p.add_argument("instance", help="ATDF instance name, e.g. WDT")
    p.add_argument("name", help="short model name, e.g. wdt")
    p.add_argument("--firmware-only", action="store_true",
                   help="the model exists: probe, check and note only")
    sub.add_parser("regen")
    p = sub.add_parser("qemu")
    p.add_argument("--avr", action="store_true")
    p = sub.add_parser("qtest")
    p.add_argument("names", nargs="+")
    sub.add_parser("gate")
    sub.add_parser("export")
    p = sub.add_parser("desk")
    p.add_argument("board")
    p.add_argument("--probe")
    p.add_argument("--port")
    p.add_argument("--allow-program", action="store_true")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "build":
            cmd_build(board(a.board), a.app)
        elif a.cmd == "twin":
            return cmd_twin(board(a.board), a.app, a.machine_opts)
        elif a.cmd == "silicon":
            return cmd_silicon(board(a.board), a.app)
        elif a.cmd == "probe":
            cmd_probe(board(a.board), a.app, a.machine_opts)
        elif a.cmd == "diff":
            return 0 if cmd_diff(board(a.board), a.app) else 1
        elif a.cmd == "check":
            cmd_check(board(a.board), a.app)
        elif a.cmd == "status":
            cmd_status(board(a.board))
        elif a.cmd == "scaffold":
            cmd_scaffold(board(a.board), a.instance, a.name,
                         a.firmware_only)
        elif a.cmd == "regen":
            cmd_regen()
        elif a.cmd == "qemu":
            cmd_qemu(a.avr)
        elif a.cmd == "qtest":
            return 0 if cmd_qtest(a.names) else 1
        elif a.cmd == "gate":
            cmd_gate()
        elif a.cmd == "export":
            cmd_export()
        elif a.cmd == "desk":
            cmd_desk(board(a.board), a.probe, a.port, a.allow_program)
    except PeriphError as e:
        print(f"[periph] error: {e}", file=sys.stderr)
        return 1
    return 0
