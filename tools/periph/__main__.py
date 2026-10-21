# Copyright (c) 2026 Clifford Swartz. MIT License (see LICENSE).
"""Entry point for ``py -m tools.periph``."""

import sys

from .cli import main

sys.exit(main())
