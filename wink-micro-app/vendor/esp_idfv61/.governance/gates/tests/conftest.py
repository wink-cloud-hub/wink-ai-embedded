# SPDX-License-Identifier: Apache-2.0
"""
conftest.py
===========
Configures sys.path so tests can import from .gates and rules directly.
"""

import sys
from pathlib import Path

GATES_DIR = Path(__file__).resolve().parent.parent
TOOLS_DIR = GATES_DIR.parent / "tools"
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))
