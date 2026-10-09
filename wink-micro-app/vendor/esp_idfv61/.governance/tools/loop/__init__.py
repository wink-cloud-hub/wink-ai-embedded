# SPDX-License-Identifier: Apache-2.0
"""
tools.loop - Backward Compatibility Package
===========================================
Re-exports symbols from the canonical loop package (.governance/loop).
"""
import sys
from pathlib import Path

_GOV_DIR = Path(__file__).resolve().parents[2]
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

if "loop" not in sys.modules or getattr(sys.modules["loop"], "__file__", None) == __file__:
    import importlib.util
    _real_loop_init = _GOV_DIR / "loop" / "__init__.py"
    _spec = importlib.util.spec_from_file_location("loop", _real_loop_init)
    _real_loop = importlib.util.module_from_spec(_spec)
    sys.modules["loop"] = _real_loop
    _spec.loader.exec_module(_real_loop)
else:
    _real_loop = sys.modules["loop"]

for _attr in getattr(_real_loop, "__all__", []):
    globals()[_attr] = getattr(_real_loop, _attr)

__all__ = getattr(_real_loop, "__all__", [])
