# SPDX-License-Identifier: Apache-2.0
"""
Global conftest.py for wink-idf-governance test pyramid.
"""
from pathlib import Path
import sys

TESTS_DIR = Path(__file__).resolve().parent
GOV_DIR = TESTS_DIR.parent
GATES_DIR = GOV_DIR / "gates"
TOOLS_DIR = GOV_DIR / "tools"
LOOP_DIR = GOV_DIR / "loop"

for p in (GOV_DIR, GATES_DIR, TOOLS_DIR, LOOP_DIR, TESTS_DIR):
    if str(p) not in sys.path:
        sys.path.insert(0, str(p))
