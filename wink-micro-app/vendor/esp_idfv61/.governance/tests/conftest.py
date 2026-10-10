# SPDX-License-Identifier: Apache-2.0
"""
Global conftest.py for wink-idf-governance test pyramid.
"""
from pathlib import Path
import sys

GOV_DIR = Path(__file__).resolve().parent.parent

if str(GOV_DIR) not in sys.path:
    sys.path.insert(0, str(GOV_DIR))
