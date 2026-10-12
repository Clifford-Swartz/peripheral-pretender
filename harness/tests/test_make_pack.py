# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""make_pack on a tiny stand-in DFP folder."""
import zipfile

import pytest

from harness import make_pack

PDSC = """<?xml version="1.0" encoding="UTF-8"?>
<package schemaVersion="1.3">
  <vendor>Acme</vendor>
  <name>Widget_DFP</name>
  <releases>
    <release version="2.0.1">newest</release>
    <release version="1.0.0">older</release>
  </releases>
</package>
"""


@pytest.fixture
def dfp(tmp_path):
    root = tmp_path / "dfp"
    (root / "W1" / "atdf").mkdir(parents=True)
    (root / "Acme.Widget_DFP.pdsc").write_text(PDSC, encoding="utf-8")
    (root / "W1" / "atdf" / "W1.atdf").write_text("<avr-tools-device-file/>", encoding="utf-8")
    (root / "package.content").write_text("files\n", encoding="utf-8")
    return root


def test_pack_layout_and_name(dfp, tmp_path):
    assert make_pack.pack_name(make_pack.find_pdsc(dfp)) == "Acme.Widget_DFP.2.0.1.pack"
    assert make_pack.main(["--dfp", str(dfp), "--out", str(tmp_path)]) == 0
    with zipfile.ZipFile(tmp_path / "Acme.Widget_DFP.2.0.1.pack") as archive:
        assert archive.namelist() == ["Acme.Widget_DFP.pdsc", "W1/atdf/W1.atdf",
                                      "package.content"]


def test_pack_is_reproducible(dfp, tmp_path):
    first, second = tmp_path / "a.pack", tmp_path / "b.pack"
    make_pack.make_pack(dfp, first)
    make_pack.make_pack(dfp, second)
    assert first.read_bytes() == second.read_bytes()


def test_folder_without_pdsc_is_rejected(tmp_path, capsys):
    assert make_pack.main(["--dfp", str(tmp_path), "--out", str(tmp_path / "x.pack")]) == 2
    assert "expected one .pdsc" in capsys.readouterr().err
