# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Session fixtures for generator tests (paths live in gen_support.py)."""

from pathlib import Path

import pytest

from gen_support import (ATDF, ATDF_AVR, ATDF_MC00, ATDF_SAME51, OVERLAYS,
                         PROFILES, find_gcc)
from mchpgen import atdf, cli, device, overlay, profile


@pytest.fixture(scope="session")
def dev():
    if not ATDF.exists():
        pytest.skip(f"ATDF not found: {ATDF}")
    return atdf.parse(str(ATDF))


@pytest.fixture(scope="session")
def overlays():
    return overlay.load_dir(str(OVERLAYS))


@pytest.fixture(scope="session")
def ds(dev, overlays):
    return device.build(dev, overlays, profile.find(str(PROFILES), dev.name))


@pytest.fixture(scope="session")
def ds_all(ds, overlays):
    """Every supported device, in the order the checked-in outputs use."""
    out = [ds]
    for path in (ATDF_MC00, ATDF_AVR, ATDF_SAME51):
        if not path.exists():
            pytest.skip(f"ATDF not found: {path}")
        d = atdf.parse(str(path))
        out.append(device.build(d, overlays,
                                profile.find(str(PROFILES), d.name)))
    return out


@pytest.fixture(scope="session")
def generated(ds, tmp_path_factory) -> Path:
    """All outputs written under a temporary root (qemu/, devices/, report.md)."""
    out = tmp_path_factory.mktemp("gen")
    files = cli.outputs(ds, str(out / "qemu"), str(out / "devices"),
                        str(out / "report.md"))
    for path, content in files.items():
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        Path(path).write_text(content, encoding="utf-8", newline="\n")
    return out


@pytest.fixture(scope="session")
def gcc():
    g = find_gcc()
    if not g:
        pytest.skip("gcc not found (set PP_GCC)")
    return g
