# SPDX-License-Identifier: Apache-2.0
"""
conftest.py
===========
Configures sys.path so tests can import from .gates and rules directly.
"""

import sys
from pathlib import Path

GATES_DIR = Path(__file__).resolve().parent.parent
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))
