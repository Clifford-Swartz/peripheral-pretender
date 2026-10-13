# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Generated headers compile, and the C NVM-rows builder matches Python.

Shim mode (acceptance check): gcc -std=gnu11 -Wall -Wextra -fsyntax-only
against tests/generator/shim, whose mchp_regfile.h holds the spec 5.1 block
verbatim and whose registerfields.h re-states QEMU's REGn()/FIELD() macros
(QEMU's own header needs qemu/osdep.h and cannot be included standalone).

QEMU mode: the same headers inside a real pp-qemu compile environment
(qemu/osdep.h, glib, QEMU warning flags); skipped without pp-qemu/build.
"""

import re
import subprocess
from pathlib import Path

import pytest

from gen_support import QEMU, SHIM, SPEC, gcc_env
from mchpgen import nvmrows

PRELUDE = """#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
"""

QEMU_WARNINGS = [
    "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wempty-body",
    "-Wendif-labels", "-Wexpansion-to-defined", "-Wformat-security",
    "-Wignored-qualifiers", "-Wimplicit-fallthrough=2", "-Winit-self",
    "-Wmissing-format-attribute", "-Wmissing-prototypes", "-Wnested-externs",
    "-Wold-style-declaration", "-Wold-style-definition", "-Wredundant-decls",
    "-Wshadow=local", "-Wstrict-prototypes", "-Wtype-limits", "-Wundef",
    "-Wvla", "-Wwrite-strings", "-Wno-missing-include-dirs", "-Wno-psabi",
    "-Wno-shift-negative-value"]


def headers(generated: Path) -> list[str]:
    root = generated / "qemu"
    out = []
    for p in sorted(root.rglob("*.h")):
        rel = p.relative_to(root).as_posix()
        out.append(rel[len("include/"):] if rel.startswith("include/") else rel)
    return out


def shim_includes(generated: Path) -> list[str]:
    return ["-I", str(SHIM), "-I", str(generated / "qemu" / "include"),
            "-I", str(generated / "qemu"), "-I", str(QEMU / "include")]


def run_gcc(gcc, args, cwd=None):
    return subprocess.run([gcc, *args], capture_output=True, text=True,
                          env=gcc_env(gcc), cwd=cwd, timeout=300)


def test_shim_is_spec_5_1_verbatim():
    spec = SPEC.read_text(encoding="utf-8")
    section = spec[spec.index("### 5.1"):spec.index("### 5.2")]
    block = re.search(r"```c\n(.*?)```", section, re.S).group(1)
    shim = (SHIM / "hw/misc/mchp_regfile.h").read_text(encoding="utf-8")
    copied = shim[shim.index("/* BEGIN spec 5.1 */\n") + 21:
                  shim.index("/* END spec 5.1 */")]
    assert copied == block


def test_device_types_header_exists():
    assert (QEMU / "include/hw/mchp/mchp_device.h").exists()


def test_each_header_compiles_alone(generated, gcc, tmp_path):
    failures = {}
    names = headers(generated)
    # 32 shared constant headers, 31 register-table headers of the device,
    # and its _gen.h, _nvmrows.h and _device.h
    assert len([n for n in names if n.count("/") == 3]) == 32
    assert len([n for n in names if "/gen/pic32cm5164jh01100/" in n]) == 31
    assert len(names) == 66
    for i, inc in enumerate(names):
        src = tmp_path / f"h{i}.c"
        src.write_text(PRELUDE + f'#include "{inc}"\n', encoding="utf-8")
        res = run_gcc(gcc, ["-std=gnu11", "-Wall", "-Wextra", "-fsyntax-only",
                            *shim_includes(generated), str(src)])
        if res.returncode or res.stderr:
            failures[inc] = res.stderr[-2000:]
    assert not failures, failures


def test_all_headers_together(generated, gcc, tmp_path):
    src = tmp_path / "all.c"
    src.write_text(PRELUDE + "".join(f'#include "{h}"\n'
                                     for h in headers(generated)),
                   encoding="utf-8")
    res = run_gcc(gcc, ["-std=gnu11", "-Wall", "-Wextra", "-fsyntax-only",
                        *shim_includes(generated), str(src)])
    assert res.returncode == 0 and not res.stderr, res.stderr[-3000:]


def test_nvmrows_fill_matches_python_image(generated, gcc, tmp_path, ds):
    sym = ds.symbol
    src = tmp_path / "nvm.c"
    src.write_text(PRELUDE + f"""#include <stdio.h>
#include "hw/arm/{sym}_nvmrows.h"

int main(int argc, char **argv)
{{
    static uint8_t buf[{sym.upper()}_NVMROWS_SIZE];
    FILE *f = fopen(argv[1], "wb");

    (void)argc;
    {sym}_nvmrows_fill(buf, {sym}_serial_default);
    fwrite(buf, 1, sizeof(buf), f);
    return fclose(f);
}}
""", encoding="utf-8")
    exe, out = tmp_path / "nvm.exe", tmp_path / "nvm.bin"
    res = run_gcc(gcc, ["-std=gnu11", "-Wall", "-Wextra", *shim_includes(generated),
                        str(src), "-o", str(exe)])
    assert res.returncode == 0 and not res.stderr, res.stderr
    subprocess.run([str(exe), str(out)], check=True, env=gcc_env(gcc), timeout=60)
    assert out.read_bytes() == nvmrows.image(ds.device, ds.profile.nvm_rows)


@pytest.mark.skipif(not (QEMU / "build" / "config-host.h").exists(),
                    reason="pp-qemu/build/config-host.h not found")
def test_headers_in_qemu_environment(generated, gcc, tmp_path, ds):
    """One TU with qemu/osdep.h, the SoC header (all IP headers) and NVM rows."""
    glib = Path(gcc).parent.parent
    src = tmp_path / "qemu_env.c"
    src.write_text('#include "qemu/osdep.h"\n'
                   f'#include "hw/arm/{ds.symbol}_gen.h"\n'
                   f'#include "hw/arm/{ds.symbol}_nvmrows.h"\n', encoding="utf-8")
    build = QEMU / "build"
    args = ["-m64", "-std=gnu11", "-D_GNU_SOURCE", "-D_FILE_OFFSET_BITS=64",
            "-D_LARGEFILE_SOURCE", "-DCOMPILING_SYSTEM_VS_USER",
            "-DCONFIG_SOFTMMU", "-fsyntax-only", *QEMU_WARNINGS,
            "-I", str(build), "-I", str(build / "qapi"), "-I", str(build / "trace"),
            "-I", str(glib / "include/glib-2.0"),
            "-I", str(glib / "lib/glib-2.0/include"),
            "-iquote", str(generated / "qemu" / "include"),
            "-iquote", str(generated / "qemu"),
            "-iquote", str(QEMU / "include"), "-iquote", str(QEMU),
            "-iquote", str(QEMU / "host/include/x86_64"),
            "-iquote", str(QEMU / "host/include/generic"),
            # the real mchp_regfile.h wins once pp-qemu has it
            "-idirafter", str(SHIM), str(src)]
    res = run_gcc(gcc, args, cwd=build)
    assert res.returncode == 0 and not res.stderr, res.stderr[-3000:]
