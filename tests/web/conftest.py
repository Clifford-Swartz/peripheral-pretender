# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Web view tests never see the user's saved benches (~/.pp_web)."""

import pytest


@pytest.fixture(autouse=True)
def _bench_dir(tmp_path, monkeypatch):
    monkeypatch.setenv("PP_WEB_BENCH_DIR", str(tmp_path / "benches"))
